#include "StateMachine.hpp"

#include "API.hpp"
#include "CamCommunication.hpp"
#include "Data.hpp"
#include "DisplayCommunication.hpp"
#include "ImageReconition.hpp"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

namespace {
State currentState = State::Idle;
constexpr size_t MaxCardImageBytes = 40 * 1024;
constexpr size_t MaxTestImageBytes = 128 * 1024;
constexpr int TestImageCount = 9;
constexpr char TestImagePathFormat[] = "/storage/test%d.jpeg";
// The printings offered on the screen for the card being added.
constexpr int MaxPrintings = 48;
constexpr size_t MaxSetTitleLength = 47;
ApiPrinting pendingPrintings[MaxPrintings];
int pendingCount = 0;
char setListPayload[4 * 1024];

const char* stateName(State state)
{
    switch (state) {
    case State::Display:
        return "DISPLAY";
    case State::Collection:
        return "COLLECTION";
    case State::Cam:
        return "CAM";
    case State::AddCard:
        return "ADD_CARD";
    case State::OcrDemo:
        return "OCR_DEMO";
    case State::CardInfo:
        return "CARD_INFO";
    case State::CardImage:
        return "CARD_IMAGE";
    case State::CardRemove:
        return "CARD_REMOVE";
    case State::CardRemoveOne:
        return "CARD_REMOVE_ONE";
    case State::Import:
        return "IMPORT";
    case State::Export:
        return "EXPORT";
    case State::Idle:
    default:
        return "IDLE";
    }
}

// Typed card name -> Scryfall -> collection.
bool addCardByTitle(const char* title)
{
    if (title == nullptr) {
        return false;
    }
    while (*title == ' ') {
        ++title;
    }
    if (title[0] == '\0') {
        printf("Add card: empty card name\n");
        return false;
    }
    printf("Add card: looking up \"%s\"\n", title);
    char* cardJson = apiFetchCardByName(title);
    if (cardJson == nullptr) {
        printf("Add card: Scryfall lookup failed for \"%s\"\n", title);
        return false;
    }

    const DataResult result = dataAddCard(cardJson);
    apiFreeJson(cardJson);
    if (result == DataResult::AlreadyExists) {
        printf("Add card: \"%s\" is already in the collection\n", title);
        return true;
    }
    if (result != DataResult::Ok) {
        printf("Add card: could not store the card (error %d)\n",
               static_cast<int>(result));
        return false;
    }
    printf("Add card: \"%s\" added to the collection\n", title);
    return true;
}

// Card name -> every set it was printed in -> the screen asks which one.
// Returns false when the sets could not be fetched; the caller then falls back
// to adding the fuzzy match directly.
bool startSetSelection(const char* cardName)
{
    while (cardName != nullptr && *cardName == ' ') {
        ++cardName;
    }
    if (cardName == nullptr || cardName[0] == '\0') {
        printf("Add card: empty card name\n");
        return false;
    }

    printf("Add card: looking up printings of \"%s\"\n", cardName);
    pendingCount = apiFetchPrintings(cardName, pendingPrintings, MaxPrintings);
    if (pendingCount <= 0) {
        printf("Add card: no printings for \"%s\"\n", cardName);
        return false;
    }

    size_t length = 0;
    for (const char* c = cardName; *c != '\0' && length + 1 < MaxSetTitleLength; ++c) {
        const char character = static_cast<unsigned char>(*c) < 32 || *c == '|' ? ' ' : *c;
        setListPayload[length++] = character;
    }
    setListPayload[length] = '\0';
    for (int index = 0; index < pendingCount; ++index) {
        const ApiPrinting& printing = pendingPrintings[index];
        const int written =
            snprintf(setListPayload + length, sizeof(setListPayload) - length,
                     "|%s|%s|%s", printing.set, printing.number,
                     printing.setName);
        if (written <= 0 ||
            static_cast<size_t>(written) >= sizeof(setListPayload) - length) {
            pendingCount = index;
            break;
        }
        length += static_cast<size_t>(written);
    }
    if (pendingCount == 0 || !displayCommunicationSendAddCardSets(setListPayload)) {
        printf("Add card: failed to send the set list\n");
        pendingCount = 0;
        return false;
    }
    printf("Add card: %d printings of \"%s\" sent to the screen\n", pendingCount,
           cardName);
    return true;
}

// The chosen printing -> Scryfall -> collection.
void choosePrinting(int index)
{
    bool added = false;
    if (index >= 0 && index < pendingCount) {
        const ApiPrinting& printing = pendingPrintings[index];
        printf("Add card: fetching %s #%s (%s)\n", printing.set, printing.number,
               printing.setName);
        char* cardJson = apiFetchCardByPrinting(printing.set, printing.number);
        if (cardJson != nullptr) {
            const DataResult result = dataAddCard(cardJson);
            apiFreeJson(cardJson);
            added = result == DataResult::Ok || result == DataResult::AlreadyExists;
        }
    } else {
        printf("Add card: bad set choice %d\n", index);
    }
    pendingCount = 0;
    if (!displayCommunicationSendAddCardResult(added)) {
        printf("Failed to send add card result\n");
    }
}

void testImagePath(int number, char* path, size_t pathSize)
{
    snprintf(path, pathSize, TestImagePathFormat, number);
}

bool hasTestImage(int number)
{
    char path[40];
    testImagePath(number, path, sizeof(path));
    FILE* file = fopen(path, "rb");
    if (file == nullptr) {
        return false;
    }
    fclose(file);
    return true;
}

uint8_t* readTestImage(int number, size_t* length)
{
    char path[40];
    testImagePath(number, path, sizeof(path));
    FILE* file = fopen(path, "rb");
    if (file == nullptr) {
        return nullptr;
    }
    fseek(file, 0, SEEK_END);
    const long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size <= 0 || size > static_cast<long>(MaxTestImageBytes)) {
        printf("OCR demo: %s has an unusable size (%ld)\n", path, size);
        fclose(file);
        return nullptr;
    }
    uint8_t* data = static_cast<uint8_t*>(malloc(static_cast<size_t>(size)));
    const size_t read = data != nullptr ? fread(data, 1, static_cast<size_t>(size), file) : 0;
    fclose(file);
    if (data == nullptr || read != static_cast<size_t>(size)) {
        printf("OCR demo: could not read %s\n", path);
        free(data);
        return nullptr;
    }
    *length = static_cast<size_t>(size);
    return data;
}

// A random test image -> OCR -> the recognised text on the screen.
void runOcrDemo()
{
    int available[TestImageCount];
    int count = 0;
    for (int number = 1; number <= TestImageCount; ++number) {
        if (hasTestImage(number)) {
            available[count++] = number;
        }
    }
    if (count == 0) {
        printf("OCR demo: no test images found; copy test1.jpeg .. test9.jpeg "
               "into Code/Master/test_Image and flash again\n");
        displayCommunicationSendOcrDemoFailed();
        return;
    }

    const int number = available[esp_random() % static_cast<uint32_t>(count)];
    size_t length = 0;
    uint8_t* image = readTestImage(number, &length);
    char name[128];
    const bool recognised =
        image != nullptr && imageRecognitionGetCardName(image, length, name, sizeof(name));
    free(image);

    if (!recognised) {
        printf("OCR demo: test%d.jpeg could not be read\n", number);
        displayCommunicationSendOcrDemoFailed();
        return;
    }
    printf("OCR demo: test%d.jpeg -> \"%s\"\n", number, name);
    // The demo card goes into the collection like a typed one: first let the
    // user pick the set, then fall back to the fuzzy match.
    if (!startSetSelection(name) &&
        !displayCommunicationSendAddCardResult(addCardByTitle(name))) {
        printf("Failed to send the OCR demo result\n");
    }
}

// Card number -> collection data -> screen.
void sendCardInfo(int index)
{
    char* cardJson = dataGetCardDetail(index);
    if (cardJson == nullptr || !displayCommunicationSendCardInfo(cardJson)) {
        printf("Card info: could not send card %d\n", index);
        displayCommunicationSendCardInfoFailed();
    }
    dataFreeJson(cardJson);
}

// Card number -> remove (all copies or just one) -> tell the screen.
void removeCard(int index, bool onlyOneCopy)
{
    const DataResult result = onlyOneCopy ? dataRemoveOneCardByIndex(index)
                                          : dataDeleteCardByIndex(index);
    if (result != DataResult::Ok) {
        printf("Remove card: could not remove card %d (error %d)\n", index,
               static_cast<int>(result));
    } else if (onlyOneCopy) {
        printf("Remove card: one copy of card %d removed\n", index);
    } else {
        printf("Remove card: card %d removed\n", index);
    }
    if (!displayCommunicationSendCardRemoveResult(result == DataResult::Ok)) {
        printf("Failed to send remove card result\n");
    }
}

// Card number -> thumbnail URL -> download -> screen.
void sendCardImage(int index)
{
    char* url = dataGetCardThumbUrl(index);
    size_t imageLength = 0;
    uint8_t* image =
        url != nullptr ? apiFetchBinary(url, MaxCardImageBytes, &imageLength) : nullptr;
    if (image == nullptr || !displayCommunicationSendCardImage(image, imageLength)) {
        printf("Card image: could not send image of card %d\n", index);
        displayCommunicationSendCardImageFailed();
    } else {
        printf("Card image: sent %u bytes\n", static_cast<unsigned>(imageLength));
    }
    apiFreeBinary(image);
    dataFreeJson(url);
}

// The whole collection -> the card server, which saves it as a JSON file.
void exportCollection()
{
    char* json = dataGetCollectionJson();
    const bool exported =
        json != nullptr && imageRecognitionEnsureWifi() && apiExportCollection(json);
    dataFreeJson(json);
    printf("Export: %s\n", exported ? "collection saved on the server"
                                    : "could not save the collection");
    displayCommunicationSendExportResult(exported);
}

// The collection file on the card server -> this board's collection.
void importCollection()
{
    char* json = imageRecognitionEnsureWifi() ? apiFetchCollectionJson() : nullptr;
    bool imported = false;
    if (json != nullptr) {
        const DataResult result = dataReplaceCollection(json);
        imported = result == DataResult::Ok;
        if (!imported) {
            printf("Import: could not store the collection (error %d)\n",
                   static_cast<int>(result));
        }
    } else {
        printf("Import: no collection file received\n");
    }
    apiFreeJson(json);
    printf("Import: %s\n", imported ? "collection replaced"
                                    : "collection left unchanged");
    displayCommunicationSendImportResult(imported);
}
}

void stateMachineInit()
{
    currentState = State::Idle;
    printf("OCR demo images on /storage:");
    for (int number = 1; number <= TestImageCount; ++number) {
        if (hasTestImage(number)) {
            printf(" test%d", number);
        }
    }
    printf("\n");
}

void stateMachineRun()
{
    if (displayCommunicationHasData()) {
        const DisplayRequest request = displayCommunicationReadRequest();
        bool stateChanged = false;
        switch (request) {
        case DisplayRequest::None:
            break;
        case DisplayRequest::CamFrame:
            // The screen finished the previous frame and wants the next one;
            // the preview keeps running.
            camCommunicationRequestFrame();
            break;
        case DisplayRequest::Capture:
            currentState = State::Cam;
            camCommunicationStartCapture();
            stateChanged = true;
            break;
        case DisplayRequest::CaptureAbort:
            camCommunicationCancelCapture();
            currentState = State::Cam;
            stateChanged = true;
            break;
        case DisplayRequest::AddCardTitle:
            currentState = State::AddCard;
            {
                const char* title = displayCommunicationReadTitle();
                if (!startSetSelection(title) &&
                    !displayCommunicationSendAddCardResult(addCardByTitle(title))) {
                    printf("Failed to send add card result\n");
                }
            }
            stateChanged = true;
            break;
        case DisplayRequest::AddCardSet:
            currentState = State::AddCard;
            choosePrinting(displayCommunicationReadArgument());
            stateChanged = true;
            break;
        case DisplayRequest::OcrDemo:
            currentState = State::OcrDemo;
            runOcrDemo();
            stateChanged = true;
            break;
        case DisplayRequest::Collection:
            currentState = State::Collection;
            stateChanged = true;
            break;
        case DisplayRequest::Display:
            currentState = State::Display;
            stateChanged = true;
            break;
        case DisplayRequest::CardInfo:
            currentState = State::CardInfo;
            stateChanged = true;
            break;
        case DisplayRequest::CardImage:
            currentState = State::CardImage;
            stateChanged = true;
            break;
        case DisplayRequest::CardRemove:
            currentState = State::CardRemove;
            stateChanged = true;
            break;
        case DisplayRequest::CardRemoveOne:
            currentState = State::CardRemoveOne;
            stateChanged = true;
            break;
        case DisplayRequest::Import:
            currentState = State::Import;
            stateChanged = true;
            break;
        case DisplayRequest::Export:
            currentState = State::Export;
            stateChanged = true;
            break;
        case DisplayRequest::Idle:
        default:
            camCommunicationCancelCapture();
            // The screen left the set list; the offer is no longer valid.
            pendingCount = 0;
            currentState = State::Idle;
            break;
        }

        if (stateChanged) {
            if (currentState == State::CardInfo) {
                sendCardInfo(displayCommunicationReadArgument());
            } else if (currentState == State::CardImage) {
                sendCardImage(displayCommunicationReadArgument());
            } else if (currentState == State::CardRemove) {
                removeCard(displayCommunicationReadArgument(), false);
            } else if (currentState == State::CardRemoveOne) {
                removeCard(displayCommunicationReadArgument(), true);
            } else if (currentState == State::Import) {
                importCollection();
            } else if (currentState == State::Export) {
                exportCollection();
            }
            if (currentState == State::Collection) {
                char* collectionList = dataGetCardNamesAndColors();
                if (collectionList == nullptr ||
                    !displayCommunicationSendCollectionList(collectionList)) {
                    printf("Failed to send collection list\n");
                } else {
                    printf("Collection list sent to display\n");
                }
                dataFreeJson(collectionList);
            }
            printf("Current state: %s\n", stateName(currentState));
            currentState = State::Idle;
        }
    }

    camCommunicationUpdate();
}
