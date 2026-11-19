#include "Ili9341Display.hpp"
#include "MenuScreen.hpp"
#include "StartScreenEntity.hpp"
#include "TouchReader.hpp"

#include "driver/uart.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdio.h>

namespace {
constexpr char Tag[] = "ScreenCommunication";
constexpr uart_port_t MasterUart = UART_NUM_2;
constexpr int MasterTxPin = 22;
constexpr int MasterRxPin = 27;
constexpr int BaudRate = 921600;
constexpr size_t MasterReceiveBufferSize = 6144;
// VGA JPEG frames arrive around 30 KB; anything far beyond that is garbage.
constexpr size_t MaxImageBytes = 64 * 1024;
constexpr size_t MaxScreenSwallowBytes = 512 * 1024;
constexpr TickType_t ImageStallTimeout = pdMS_TO_TICKS(3000);

char masterReceiveBuffer[MasterReceiveBufferSize];
size_t masterReceiveLength = 0;

// A picture arrives as "<CARD_IMAGE|CAM_IMAGE> <bytes>\n" followed by that many
// raw bytes.
uint8_t* imageBuffer = nullptr;      // being filled (nullptr while skipping a picture)
size_t imageExpected = 0;
size_t imageReceived = 0;
TickType_t imageLastByteTick = 0;
bool imageIsCamera = false;          // the picture being received is a camera frame
bool imageIsPhoto = false;           // ... and it is the photo taken at SNAP
bool imageDiscard = false;           // picture is read and thrown away (not shown)
uint8_t* receivedImage = nullptr;    // finished picture, owned by the main loop
size_t receivedImageLength = 0;

// Payload that came with the last OCR demo result.
char ocrDemoText[128];
int ocrDemoImage = 0;

void freeReceivedImage()
{
    std::free(receivedImage);
    receivedImage = nullptr;
    receivedImageLength = 0;
}

void masterCommunicationInit()
{
    const uart_config_t config = {
        .baud_rate = BaudRate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(MasterUart, &config));
    ESP_ERROR_CHECK(
        uart_set_pin(MasterUart, MasterTxPin, MasterRxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    // Kept at 8 KB: the display backbuffer already takes most of DRAM.
    ESP_ERROR_CHECK(uart_driver_install(MasterUart, 8192, 2048, 0, nullptr, 0));
}

void sendMasterCommand(const char* command)
{
    const size_t commandLength = std::strlen(command);
    ESP_LOGD(Tag, "TX command: %s", command);
    const int bytesWritten =
        uart_write_bytes(MasterUart, command, commandLength);
    if (bytesWritten != static_cast<int>(commandLength)) {
        ESP_LOGE(Tag, "Failed to send command");
        return;
    }
    ESP_ERROR_CHECK(uart_wait_tx_done(MasterUart, pdMS_TO_TICKS(100)));
}

enum class MasterEvent {
    None,
    AddCardDone,
    AddCardFailed,
    CapturePhoto,
    CaptureFailed,
    CardInfo,
    CardInfoFailed,
    CardImage,
    CardImageFailed,
    CamImage,
    CamImageFailed,
    CardRemoved,
    CardRemoveFailed,
    OcrDemoResult,
    OcrDemoFailed,
    AddCardSets,
    ImportDone,
    ImportFailed,
    ExportDone,
    ExportFailed,
};

MasterEvent beginImage(const char* lengthText, bool camera, bool photo, bool keep)
{
    const long length = std::atol(lengthText);
    const MasterEvent failure =
        camera ? MasterEvent::CamImageFailed : MasterEvent::CardImageFailed;
    if (length <= 0 || length > static_cast<long>(MaxScreenSwallowBytes)) {
        ESP_LOGE(Tag, "Bad picture size %ld", length);
        return photo ? MasterEvent::CaptureFailed : failure;
    }
    // A picture that cannot be stored is still read (and thrown away) so its
    // bytes are not mistaken for text lines; the master is told it failed.
    imageIsCamera = camera;
    imageIsPhoto = photo;
    imageDiscard = !keep;
    const size_t size = static_cast<size_t>(length);
    imageBuffer = (!imageDiscard && size <= MaxImageBytes)
                      ? static_cast<uint8_t*>(std::malloc(size))
                      : nullptr;
    if (!imageDiscard && imageBuffer == nullptr) {
        ESP_LOGE(Tag, "No memory for %u byte picture (free DRAM %u)", (unsigned)size,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }
    imageExpected = size;
    imageReceived = 0;
    imageLastByteTick = xTaskGetTickCount();
    return MasterEvent::None;
}

MasterEvent handleMasterLine(Ili9341Display& display, MenuScreen& menu,
                             bool liveCamera, bool keepPhoto)
{
    constexpr char collectionPrefix[] = "COLLECTION_LIST ";
    constexpr char infoPrefix[] = "CARD_INFO ";
    constexpr char imagePrefix[] = "CARD_IMAGE ";
    constexpr char camImagePrefix[] = "CAM_IMAGE ";
    constexpr char camPhotoPrefix[] = "CAM_PHOTO ";
    constexpr char ocrResultPrefix[] = "OCR_DEMO_RESULT ";
    const char* line = masterReceiveBuffer;

    if (std::strncmp(line, infoPrefix, sizeof(infoPrefix) - 1) == 0) {
        menu.setCardInfo(line + sizeof(infoPrefix) - 1);
        return MasterEvent::CardInfo;
    }
    if (std::strncmp(line, "CARD_REMOVE_DONE", 16) == 0) {
        return MasterEvent::CardRemoved;
    }
    if (std::strncmp(line, "CARD_REMOVE_FAILED", 18) == 0) {
        return MasterEvent::CardRemoveFailed;
    }
    if (std::strncmp(line, "CARD_INFO_FAILED", 16) == 0) {
        return MasterEvent::CardInfoFailed;
    }
    if (std::strncmp(line, "CARD_IMAGE_FAILED", 17) == 0) {
        return MasterEvent::CardImageFailed;
    }
    if (std::strncmp(line, "CAPTURE_FAILED", 14) == 0) {
        return MasterEvent::CaptureFailed;
    }
    if (std::strncmp(line, "OCR_DEMO_FAILED", 15) == 0) {
        return MasterEvent::OcrDemoFailed;
    }
    if (std::strncmp(line, "IMPORT_DONE", 11) == 0) {
        return MasterEvent::ImportDone;
    }
    if (std::strncmp(line, "IMPORT_FAILED", 13) == 0) {
        return MasterEvent::ImportFailed;
    }
    if (std::strncmp(line, "EXPORT_DONE", 11) == 0) {
        return MasterEvent::ExportDone;
    }
    if (std::strncmp(line, "EXPORT_FAILED", 13) == 0) {
        return MasterEvent::ExportFailed;
    }
    if (std::strncmp(line, ocrResultPrefix, sizeof(ocrResultPrefix) - 1) == 0) {
        const char* payload = line + sizeof(ocrResultPrefix) - 1;
        ocrDemoImage = std::atoi(payload);
        const char* text = std::strchr(payload, ' ');
        ocrDemoText[0] = '\0';
        if (text != nullptr) {
            std::snprintf(ocrDemoText, sizeof(ocrDemoText), "%s", text + 1);
        }
        return MasterEvent::OcrDemoResult;
    }
    if (std::strncmp(line, camPhotoPrefix, sizeof(camPhotoPrefix) - 1) == 0) {
        return beginImage(line + sizeof(camPhotoPrefix) - 1, true, true, keepPhoto);
    }
    if (std::strncmp(line, camImagePrefix, sizeof(camImagePrefix) - 1) == 0) {
        if (liveCamera) {
            ESP_LOGD(Tag, "Camera picture: %ld bytes",
                     std::atol(line + sizeof(camImagePrefix) - 1));
        }
        return beginImage(line + sizeof(camImagePrefix) - 1, true, false, liveCamera);
    }
    if (std::strncmp(line, imagePrefix, sizeof(imagePrefix) - 1) == 0) {
        return beginImage(line + sizeof(imagePrefix) - 1, false, false, true);
    }
    if (std::strncmp(line, "ADD_CARD_SETS ", 14) == 0) {
        menu.drawAddSets(display, line + 14);
        return MasterEvent::AddCardSets;
    }
    if (std::strstr(line, "ADD_CARD_DONE") != nullptr) {
        return MasterEvent::AddCardDone;
    }
    if (std::strstr(line, "ADD_CARD_FAILED") != nullptr) {
        return MasterEvent::AddCardFailed;
    }
    const char* response = std::strstr(line, collectionPrefix);
    if (response != nullptr) {
        menu.drawCollection(display, response + sizeof(collectionPrefix) - 1);
    } else {
        ESP_LOGW(Tag, "Unknown line from master: %s", line);
    }
    return MasterEvent::None;
}

MasterEvent receiveMasterData(Ili9341Display& display, MenuScreen& menu,
                              bool liveCamera, bool keepPhoto)
{
    MasterEvent event = MasterEvent::None;

    // A picture that stops arriving must not block the following messages.
    if (imageExpected > 0 && xTaskGetTickCount() - imageLastByteTick > ImageStallTimeout) {
        ESP_LOGE(Tag, "Picture transfer timed out (%u of %u bytes)",
                 (unsigned)imageReceived, (unsigned)imageExpected);
        std::free(imageBuffer);
        imageBuffer = nullptr;
        imageExpected = 0;
        if (imageIsPhoto) {
            event = MasterEvent::CaptureFailed;
        } else {
            event = imageIsCamera ? MasterEvent::CamImageFailed : MasterEvent::CardImageFailed;
        }
        imageIsCamera = false;
        imageIsPhoto = false;
        imageDiscard = false;
    }

    // Read everything that is waiting, so a picture is taken in quickly.
    for (int round = 0; round < 64; ++round) {
        size_t available = 0;
        ESP_ERROR_CHECK(uart_get_buffered_data_len(MasterUart, &available));
        if (available == 0) {
            break;
        }

        uint8_t data[512];
        const size_t bytesToRead = available < sizeof(data) ? available : sizeof(data);
        const int bytesRead = uart_read_bytes(MasterUart, data, bytesToRead, 0);
        if (bytesRead <= 0) {
            break;
        }

        for (int index = 0; index < bytesRead; ++index) {
            if (imageExpected > 0) {
                if (imageBuffer != nullptr) {
                    imageBuffer[imageReceived] = data[index];
                }
                ++imageReceived;
                imageLastByteTick = xTaskGetTickCount();
                if (imageReceived == imageExpected) {
                    if (imageDiscard) {
                        std::free(imageBuffer);
                        imageBuffer = nullptr;
                        imageExpected = 0;
                        imageIsCamera = false;
                        imageIsPhoto = false;
                        imageDiscard = false;
                        continue;
                    }
                    const bool camera = imageIsCamera;
                    const bool photo = imageIsPhoto;
                    imageIsCamera = false;
                    imageIsPhoto = false;
                    freeReceivedImage();
                    receivedImage = imageBuffer;
                    receivedImageLength = imageExpected;
                    if (imageBuffer == nullptr) {
                        event = photo ? MasterEvent::CaptureFailed
                                      : (camera ? MasterEvent::CamImageFailed
                                                : MasterEvent::CardImageFailed);
                    } else {
                        event = photo ? MasterEvent::CapturePhoto
                                      : (camera ? MasterEvent::CamImage
                                                : MasterEvent::CardImage);
                    }
                    imageBuffer = nullptr;
                    imageExpected = 0;
                }
                continue;
            }

            const char character = static_cast<char>(data[index]);
            if (character == '\n') {
                masterReceiveBuffer[masterReceiveLength] = '\0';
                const MasterEvent lineEvent =
                    handleMasterLine(display, menu, liveCamera, keepPhoto);
                if (lineEvent != MasterEvent::None) {
                    event = lineEvent;
                }
                masterReceiveLength = 0;
                continue;
            }
            if (masterReceiveLength < MasterReceiveBufferSize - 1) {
                masterReceiveBuffer[masterReceiveLength++] = character;
            } else {
                masterReceiveLength = 0;
                ESP_LOGE(Tag, "Master response exceeded receive buffer");
            }
        }
    }
    return event;
}

enum class Page {
    Home,
    AddMenu,   // TYPE NAME / SCAN DEMO / OCR DEMO
    Scan,      // camera preview and photo
    Keyboard,  // typing a card name
    AddSets,   // picking the printing (set) of the found card
    Ocr,       // OCR demo result
    Collection,
    Decks,
};

enum class CollectionView {
    List,         // the card names
    InfoLoading,  // waiting for the card's data
    DetailMenu,   // CARD / LEGALITIES / OTHER
    ImageLoading, // waiting for the picture
    RemoveConfirm, // "REMOVE CARD?" YES / NO
    Removing,     // waiting for the master to remove the card
    Card,
    Legalities,
    Other,
    TransferMenu,    // IMPORT / EXPORT buttons
    TransferWaiting, // waiting for the master's answer
};

enum class CameraView {
    Live,     // the camera feed is drawn frame by frame
    Taking,   // SNAP pressed; waiting for the master's photo
    Review,   // the photo is stored and is being shown
};

void sendCardCommand(const char* command, int cardNumber)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%s %d\n", command, cardNumber);
    sendMasterCommand(text);
}
}

extern "C" void app_main(void) {
    constexpr int lcdMosi = 13;
    constexpr int lcdSclk = 14;
    constexpr int lcdCs = 15;
    constexpr int lcdDc = 2;
    constexpr int lcdRst = -1;
    constexpr int lcdBacklight = 21;

    masterCommunicationInit();

    Ili9341Display display(lcdMosi, lcdSclk, lcdCs, lcdDc, lcdRst, lcdBacklight);
    display.init();
    ESP_LOGI(Tag, "free DRAM %u, largest DMA block %u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));

    StartScreenEntity startScreen;
    display.fillScreen(display.rgb565(255, 255, 255));
    startScreen.draw(display);
    display.present();
    sendMasterCommand("IDLE\n");

    MenuScreen menu;
    // TPM408-2.8 / XPT2046 touch wiring.
    TouchReader touch(SPI3_HOST, GPIO_NUM_25, GPIO_NUM_32, GPIO_NUM_39,
                      GPIO_NUM_33, GPIO_NUM_36);
    touch.start();

    while (!touch.touched()) {
        receiveMasterData(display, menu, false, false);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    touch.waitForRelease();

    menu.drawHome(display);
    sendMasterCommand("IDLE\n");
    Page page = Page::Home;
    int collectionPage = 0;
    CollectionView view = CollectionView::List;
    int selectedCard = -1;
    CameraView cameraView = CameraView::Live;
    TickType_t cameraPageTick = 0;
    TickType_t cameraRequestTick = 0;
    TickType_t cameraTakeTick = 0;
    bool cameraFrameSeen = false;
    bool cameraTimeoutShown = false;

    char keyboardText[48] = {};
    int keyboardLength = 0;
    bool keyboardSending = false;
    TickType_t keyboardTakeTick = 0;

    int addSetsPage = 0;
    bool addSetsSending = false;
    TickType_t addSetsTick = 0;

    bool ocrWaiting = false;
    TickType_t ocrTick = 0;

    bool transferImport = true;
    TickType_t transferTick = 0;

    while (true) {
        const bool cameraLive = page == Page::Scan && cameraView == CameraView::Live;
        const bool keepPhoto = page == Page::Scan && cameraView == CameraView::Taking;
        const MasterEvent masterEvent =
            receiveMasterData(display, menu, cameraLive, keepPhoto);

        if (page == Page::Keyboard && keyboardSending &&
            (masterEvent == MasterEvent::AddCardDone ||
             masterEvent == MasterEvent::AddCardFailed)) {
            // The master finished looking up the typed card: go back to the
            // main menu after telling the user how it went.
            keyboardSending = false;
            keyboardLength = 0;
            menu.drawPage(display, masterEvent == MasterEvent::AddCardDone ? "ADDED"
                                                                          : "FAILED");
            vTaskDelay(pdMS_TO_TICKS(1500));
            page = Page::Home;
            menu.drawHome(display);
            sendMasterCommand("IDLE\n");
            continue;
        }
        if (masterEvent == MasterEvent::AddCardSets &&
            ((page == Page::Keyboard && keyboardSending) || page == Page::Ocr)) {
            // The card was found: the set list has been drawn; now the user
            // picks the printing.
            keyboardSending = false;
            ocrWaiting = false;
            addSetsSending = false;
            addSetsPage = 0;
            page = Page::AddSets;
        }
        if (page == Page::AddSets && masterEvent == MasterEvent::AddCardSets) {
            addSetsPage = 0;
        }
        if (page != Page::AddSets && masterEvent == MasterEvent::AddCardSets) {
            // A lookup the user walked away from; drop the list and restore
            // the page the master's line drew over.
            sendMasterCommand("IDLE\n");
            if (page == Page::Home) {
                menu.drawHome(display);
            } else if (page == Page::AddMenu) {
                menu.drawAddMenu(display);
            } else if (page == Page::Keyboard) {
                menu.drawKeyboard(display, keyboardText);
            }
        }
        if (page == Page::AddSets && addSetsSending &&
            (masterEvent == MasterEvent::AddCardDone ||
             masterEvent == MasterEvent::AddCardFailed)) {
            // The chosen printing was added: back to the main menu.
            addSetsSending = false;
            menu.drawPage(display, masterEvent == MasterEvent::AddCardDone ? "ADDED"
                                                                          : "FAILED");
            vTaskDelay(pdMS_TO_TICKS(1500));
            page = Page::Home;
            menu.drawHome(display);
            sendMasterCommand("IDLE\n");
            continue;
        }
        if (page == Page::Ocr) {
            if (masterEvent == MasterEvent::OcrDemoResult) {
                ocrWaiting = false;
                menu.drawOcrResult(display, ocrDemoImage, ocrDemoText);
            } else if (masterEvent == MasterEvent::OcrDemoFailed) {
                ocrWaiting = false;
                menu.drawOcrFailed(display);
            } else if (masterEvent == MasterEvent::AddCardDone) {
                // Fallback: no set list, the card was added straight away.
                ocrWaiting = false;
                menu.drawPage(display, "ADDED");
                vTaskDelay(pdMS_TO_TICKS(1500));
                page = Page::Home;
                menu.drawHome(display);
                sendMasterCommand("IDLE\n");
                continue;
            } else if (masterEvent == MasterEvent::AddCardFailed) {
                ocrWaiting = false;
                menu.drawOcrFailed(display);
            }
        }
        if (page == Page::Scan && cameraView == CameraView::Live &&
            masterEvent == MasterEvent::CamImage) {
            // Live camera preview; a failed frame keeps the last picture.
            const size_t heapBefore =
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            if (menu.drawCameraView(display, receivedImage, receivedImageLength)) {
                if (!cameraFrameSeen) {
                    ESP_LOGI(Tag, "camera frame drawn, free DRAM %u",
                             (unsigned)heapBefore);
                }
                cameraFrameSeen = true;
            } else {
                ESP_LOGW(Tag, "camera frame not decodable, free DRAM %u",
                         (unsigned)heapBefore);
                if (!cameraFrameSeen) {
                    menu.drawCameraWaiting(display, "NO SIGNAL");
                }
            }
            // The master sends one frame per request; ask for the next one
            // once this frame is on screen.
            sendMasterCommand("CAM_FRAME\n");
            cameraRequestTick = xTaskGetTickCount();
        } else if (page == Page::Scan && cameraView == CameraView::Live &&
                   masterEvent == MasterEvent::CamImageFailed) {
            if (!cameraFrameSeen) {
                menu.drawCameraWaiting(display, "NO SIGNAL");
            }
            sendMasterCommand("CAM_FRAME\n");
            cameraRequestTick = xTaskGetTickCount();
        } else if (page == Page::Scan && cameraView == CameraView::Taking &&
                   masterEvent == MasterEvent::CapturePhoto) {
            // The preview frame behind SNAP arrived: show it with the demo
            // disclaimer, then drop the copy in RAM (the screen keeps the
            // picture buffer on the display).
            ESP_LOGI(Tag, "capture photo received: %u bytes",
                     (unsigned)receivedImageLength);
            cameraView = CameraView::Review;
            if (!menu.drawDemoPhoto(display, receivedImage, receivedImageLength)) {
                ESP_LOGW(Tag, "capture photo not decodable (free DRAM %u)",
                         (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
            }
        } else if (page == Page::Scan && cameraView == CameraView::Taking &&
                   (masterEvent == MasterEvent::CaptureFailed ||
                    masterEvent == MasterEvent::CamImageFailed)) {
            cameraView = CameraView::Live;
            cameraTimeoutShown = true;
            menu.drawCameraWaiting(display, "NO PHOTO");
            sendMasterCommand("CAPTURE_ABORT\n");
            sendMasterCommand("CAM_FRAME\n");
            cameraRequestTick = xTaskGetTickCount();
        }
        if (page == Page::Collection && view == CollectionView::InfoLoading) {
            if (masterEvent == MasterEvent::CardInfo) {
                view = CollectionView::DetailMenu;
                menu.drawDetailMenu(display);
            } else if (masterEvent == MasterEvent::CardInfoFailed) {
                menu.drawPage(display, "NO DATA");
                vTaskDelay(pdMS_TO_TICKS(1500));
                view = CollectionView::List;
                menu.drawCollectionPage(display, collectionPage);
            }
        } else if (page == Page::Collection && view == CollectionView::Removing &&
                   (masterEvent == MasterEvent::CardRemoved ||
                    masterEvent == MasterEvent::CardRemoveFailed)) {
            if (masterEvent == MasterEvent::CardRemoved) {
                // Back to the collection; ask for the list again so the card is gone.
                view = CollectionView::List;
                collectionPage = 0;
                selectedCard = -1;
                menu.drawCollectionLoading(display);
                sendMasterCommand("COLLECTION\n");
            } else {
                menu.drawPage(display, "FAILED");
                vTaskDelay(pdMS_TO_TICKS(1500));
                view = CollectionView::DetailMenu;
                menu.drawDetailMenu(display);
            }
        } else if (page == Page::Collection && view == CollectionView::TransferWaiting &&
                   (masterEvent == MasterEvent::ImportDone ||
                    masterEvent == MasterEvent::ImportFailed ||
                    masterEvent == MasterEvent::ExportDone ||
                    masterEvent == MasterEvent::ExportFailed)) {
            const bool done = masterEvent == MasterEvent::ImportDone ||
                              masterEvent == MasterEvent::ExportDone;
            const char* title =
                done ? (transferImport ? "IMPORTED" : "EXPORTED") : "FAILED";
            menu.drawPage(display, title);
            vTaskDelay(pdMS_TO_TICKS(1500));
            // Import may have replaced the collection: ask for the list again.
            view = CollectionView::List;
            collectionPage = 0;
            menu.drawCollectionLoading(display);
            sendMasterCommand("COLLECTION\n");
        } else if (page == Page::Collection && view == CollectionView::ImageLoading &&
                   (masterEvent == MasterEvent::CardImage ||
                    masterEvent == MasterEvent::CardImageFailed)) {
            // Without a picture the card screen still shows the oracle text.
            view = CollectionView::Card;
            menu.drawCardView(display,
                              masterEvent == MasterEvent::CardImage ? receivedImage : nullptr,
                              receivedImageLength);
        }
        if (receivedImage != nullptr) {
            freeReceivedImage();
        }
        if (page == Page::Scan && cameraView == CameraView::Live && imageExpected == 0 &&
            xTaskGetTickCount() - cameraRequestTick > pdMS_TO_TICKS(2000)) {
            // The request or the frame got lost; ask for a frame again.
            sendMasterCommand("CAM_FRAME\n");
            cameraRequestTick = xTaskGetTickCount();
        }
        if (page == Page::Scan && cameraView == CameraView::Taking &&
            xTaskGetTickCount() - cameraTakeTick > pdMS_TO_TICKS(10000)) {
            // The master never sent the photo; go back to the preview.
            cameraView = CameraView::Live;
            cameraTimeoutShown = true;
            menu.drawCameraWaiting(display, "NO PHOTO");
            sendMasterCommand("CAPTURE_ABORT\n");
            sendMasterCommand("CAM_FRAME\n");
            cameraRequestTick = xTaskGetTickCount();
        }
        if (page == Page::Scan && cameraView == CameraView::Live && !cameraFrameSeen &&
            !cameraTimeoutShown &&
            xTaskGetTickCount() - cameraPageTick > pdMS_TO_TICKS(4000)) {
            // Nothing came back from the camera at all.
            cameraTimeoutShown = true;
            menu.drawCameraWaiting(display, "NO CAMERA");
        }
        if (page == Page::Ocr && ocrWaiting &&
            xTaskGetTickCount() - ocrTick > pdMS_TO_TICKS(25000)) {
            ocrWaiting = false;
            menu.drawOcrFailed(display);
        }
        if (page == Page::Keyboard && keyboardSending &&
            xTaskGetTickCount() - keyboardTakeTick > pdMS_TO_TICKS(40000)) {
            // The lookup never answered; do not leave the keyboard stuck.
            keyboardSending = false;
            keyboardLength = 0;
            menu.drawPage(display, "FAILED");
            vTaskDelay(pdMS_TO_TICKS(1200));
            page = Page::AddMenu;
            menu.drawAddMenu(display);
            sendMasterCommand("IDLE\n");
        }
        if (page == Page::AddSets && addSetsSending &&
            xTaskGetTickCount() - addSetsTick > pdMS_TO_TICKS(40000)) {
            // The lookup never answered; do not leave the set list stuck.
            addSetsSending = false;
            page = Page::Home;
            menu.drawHome(display);
            sendMasterCommand("IDLE\n");
        }
        if (page == Page::Collection && view == CollectionView::TransferWaiting &&
            xTaskGetTickCount() - transferTick > pdMS_TO_TICKS(30000)) {
            // The import/export never answered; do not leave the page stuck.
            view = CollectionView::List;
            menu.drawPage(display, "FAILED");
            vTaskDelay(pdMS_TO_TICKS(1200));
            menu.drawCollectionPage(display, collectionPage);
        }

        if (!touch.touched()) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        const TouchReader::Point point = touch.readPoint();
        touch.waitForRelease();
        if (page == Page::Home) {
            const int homeButton = menu.hitTest(point.x, point.y);
            if (homeButton == 0) {
                page = Page::AddMenu;
                menu.drawAddMenu(display);
            } else if (homeButton == 1) {
                page = Page::Collection;
                collectionPage = 0;
                view = CollectionView::List;
                menu.drawCollectionLoading(display);
                sendMasterCommand("COLLECTION\n");
            } else if (homeButton == 2) {
                page = Page::Decks;
                menu.drawPage(display, "DECKS");
                sendMasterCommand("DECKS\n");
            }
        } else if (page == Page::AddMenu) {
            if (menu.hitExit(point.x, point.y)) {
                page = Page::Home;
                menu.drawHome(display);
                sendMasterCommand("IDLE\n");
                continue;
            }
            const int choice = menu.hitAddMenu(point.x, point.y);
            if (choice == 0) {
                page = Page::Keyboard;
                keyboardLength = 0;
                keyboardText[0] = '\0';
                keyboardSending = false;
                menu.drawKeyboard(display, keyboardText);
            } else if (choice == 1) {
                page = Page::Scan;
                cameraPageTick = xTaskGetTickCount();
                cameraView = CameraView::Live;
                cameraFrameSeen = false;
                cameraTimeoutShown = false;
                menu.drawCameraWaiting(display, "SCAN DEMO");
                sendMasterCommand("CAM_FRAME\n");
                cameraRequestTick = xTaskGetTickCount();
            } else if (choice == 2) {
                page = Page::Ocr;
                ocrWaiting = true;
                ocrTick = xTaskGetTickCount();
                ocrDemoText[0] = '\0';
                ocrDemoImage = 0;
                menu.drawOcrLoading(display);
                sendMasterCommand("OCR_DEMO\n");
            }
        } else if (page == Page::Keyboard) {
            if (keyboardSending) {
                continue;
            }
            if (menu.hitExit(point.x, point.y)) {
                page = Page::AddMenu;
                keyboardLength = 0;
                menu.drawAddMenu(display);
                sendMasterCommand("IDLE\n");
                continue;
            }
            const char key = menu.hitKeyboardKey(point.x, point.y);
            if (key == '\n') {
                if (keyboardLength == 0) {
                    continue;
                }
                keyboardText[keyboardLength] = '\0';
                keyboardSending = true;
                keyboardTakeTick = xTaskGetTickCount();
                menu.drawKeyboard(display, keyboardText, "SENDING");
                char command[72];
                std::snprintf(command, sizeof(command), "ADD_CARD_TITLE %s\n",
                              keyboardText);
                sendMasterCommand(command);
            } else if (key == '\b') {
                if (keyboardLength > 0) {
                    --keyboardLength;
                    keyboardText[keyboardLength] = '\0';
                    menu.drawKeyboard(display, keyboardText);
                }
            } else if (key != 0) {
                if (keyboardLength < static_cast<int>(sizeof(keyboardText)) - 1) {
                    keyboardText[keyboardLength++] = key;
                    keyboardText[keyboardLength] = '\0';
                    menu.drawKeyboard(display, keyboardText);
                }
            }
        } else if (page == Page::AddSets) {
            if (addSetsSending) {
                continue;
            }
            if (menu.hitExit(point.x, point.y)) {
                page = Page::Home;
                menu.drawHome(display);
                sendMasterCommand("IDLE\n");
            } else if (menu.hitCollectionPrevious(point.x, point.y)) {
                if (addSetsPage > 0) {
                    addSetsPage = menu.drawAddSetsPage(display, addSetsPage - 1);
                }
            } else if (menu.hitCollectionNext(point.x, point.y)) {
                addSetsPage = menu.drawAddSetsPage(display, addSetsPage + 1);
            } else {
                const int printing = menu.hitAddSetRow(point.x, point.y, addSetsPage);
                if (printing >= 0) {
                    addSetsSending = true;
                    addSetsTick = xTaskGetTickCount();
                    menu.drawPage(display, "ADDING");
                    sendCardCommand("ADD_CARD_SET", printing);
                }
            }
        } else if (page == Page::Ocr) {
            if (menu.hitExit(point.x, point.y)) {
                page = Page::AddMenu;
                menu.drawAddMenu(display);
                sendMasterCommand("IDLE\n");
            }
        } else if (page == Page::Scan) {
            if (menu.hitExit(point.x, point.y)) {
                // EXIT works in every camera view.
                if (cameraView != CameraView::Live) {
                    sendMasterCommand("CAPTURE_ABORT\n");
                }
                cameraView = CameraView::Live;
                page = Page::AddMenu;
                menu.drawAddMenu(display);
                sendMasterCommand("IDLE\n");
            } else if (cameraView == CameraView::Live && cameraFrameSeen &&
                       menu.hitCameraShutter(point.x, point.y)) {
                // SNAP: the master forwards the next preview frame while the
                // picture on screen stays frozen.
                cameraView = CameraView::Taking;
                cameraTakeTick = xTaskGetTickCount();
                menu.drawCaptureSending(display, "TAKING PHOTO");
                sendMasterCommand("CAPTURE\n");
            } else if (cameraView == CameraView::Review &&
                       menu.hitDemoBack(point.x, point.y)) {
                cameraView = CameraView::Live;
                cameraTimeoutShown = true;
                menu.drawCameraWaiting(display, "WAITING");
                sendMasterCommand("CAM_FRAME\n");
                cameraRequestTick = xTaskGetTickCount();
            }
        } else if (page == Page::Collection) {
            const bool back = menu.hitExit(point.x, point.y);
            switch (view) {
            case CollectionView::List:
                if (menu.hitTransfer(point.x, point.y)) {
                    view = CollectionView::TransferMenu;
                    menu.drawTransferMenu(display);
                } else if (menu.hitCollectionPrevious(point.x, point.y)) {
                    if (collectionPage > 0) {
                        collectionPage = menu.drawCollectionPage(display, collectionPage - 1);
                    }
                } else if (menu.hitCollectionNext(point.x, point.y)) {
                    collectionPage = menu.drawCollectionPage(display, collectionPage + 1);
                } else if (back) {
                    page = Page::Home;
                    collectionPage = 0;
                    menu.drawHome(display);
                    sendMasterCommand("IDLE\n");
                } else {
                    const int card = menu.hitCollectionRow(point.x, point.y, collectionPage);
                    if (card >= 0) {
                        selectedCard = card;
                        view = CollectionView::InfoLoading;
                        menu.drawDetailLoading(display);
                        sendCardCommand("CARD_INFO", selectedCard);
                    }
                }
                break;
            case CollectionView::InfoLoading:
                if (back) {
                    view = CollectionView::List;
                    menu.drawCollectionPage(display, collectionPage);
                }
                break;
            case CollectionView::DetailMenu:
                if (back) {
                    view = CollectionView::List;
                    menu.drawCollectionPage(display, collectionPage);
                } else {
                    const int button = menu.hitDetailButton(point.x, point.y);
                    if (button == 0) {
                        view = CollectionView::ImageLoading;
                        menu.drawDetailLoading(display);
                        sendCardCommand("CARD_IMAGE", selectedCard);
                    } else if (button == 1) {
                        view = CollectionView::Legalities;
                        menu.drawLegalities(display);
                    } else if (button == 2) {
                        view = CollectionView::Other;
                        menu.drawOther(display);
                    } else if (button == 3) {
                        view = CollectionView::RemoveConfirm;
                        menu.drawRemoveConfirm(display);
                    }
                }
                break;
            case CollectionView::RemoveConfirm: {
                const int choice = menu.hitRemoveConfirm(point.x, point.y);
                if (choice == 0) {
                    view = CollectionView::Removing;
                    menu.drawDetailLoading(display);
                    sendCardCommand("CARD_REMOVE", selectedCard);
                } else if (choice == 2) {
                    view = CollectionView::Removing;
                    menu.drawDetailLoading(display);
                    sendCardCommand("CARD_REMOVE_ONE", selectedCard);
                } else if (choice == 1) {
                    // NO: straight back to the collection menu.
                    view = CollectionView::List;
                    menu.drawCollectionPage(display, collectionPage);
                }
                break;
            }
            case CollectionView::Removing:
                // Wait for the master's answer; touches are ignored.
                break;
            case CollectionView::TransferMenu: {
                const int choice = menu.hitTransferMenu(point.x, point.y);
                if (back) {
                    view = CollectionView::List;
                    menu.drawCollectionPage(display, collectionPage);
                } else if (choice >= 0) {
                    transferImport = choice == 0;
                    view = CollectionView::TransferWaiting;
                    transferTick = xTaskGetTickCount();
                    menu.drawPage(display, transferImport ? "IMPORTING" : "EXPORTING");
                    sendMasterCommand(transferImport ? "IMPORT\n" : "EXPORT\n");
                }
                break;
            }
            case CollectionView::TransferWaiting:
                // Wait for the master's answer; touches are ignored.
                break;
            case CollectionView::ImageLoading:
            case CollectionView::Card:
            case CollectionView::Legalities:
            case CollectionView::Other:
                if (back) {
                    view = CollectionView::DetailMenu;
                    menu.drawDetailMenu(display);
                }
                break;
            }
        } else if (menu.hitExit(point.x, point.y)) {
            page = Page::Home;
            collectionPage = 0;
            menu.drawHome(display);
            sendMasterCommand("IDLE\n");
        }
    }
}
