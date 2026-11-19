/**
 * @file CamCommunication.cpp
 * @brief Implements the JPEG frame protocol with the ESP32-CAM.
 */
#include "CamCommunication.hpp"

#include "DisplayCommunication.hpp"
#include "driver/uart.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>

namespace {
constexpr uart_port_t CamUart = UART_NUM_2;
// The camera only sends; the Master listens. The data wire is either on IO26
// (the documented UOR pin) or on IO25, so both are tried when nothing arrives.
constexpr int CamRxPins[] = {26, 25};
constexpr size_t CamRxPinCount = sizeof(CamRxPins) / sizeof(CamRxPins[0]);
constexpr uint32_t PinRetryMs = 4000;
constexpr int BaudRate = 921600;
constexpr char Tag[] = "CamCommunication";
constexpr uint8_t FrameMagic[4] = {0xA5, 0x5A, 0xE7, 0xC3};
constexpr size_t MaxImageSize = 512 * 1024;
constexpr size_t MaxScreenFrameBytes = 60 * 1024;
// The camera interleaves small QVGA previews with full VGA photo frames. The
// scan demo only uses the previews, so anything wider is skipped.
constexpr uint16_t PhotoMinWidth = 400;
constexpr uint32_t CaptureWaitMs = 6000;

enum class ReceiveState {
  WaitingForMagic,
  ReadingHeader,
  ReadingImage,
};

ReceiveState receiveState = ReceiveState::WaitingForMagic;
size_t magicMatches = 0;
uint8_t lengthBytes[4];
size_t lengthBytesReceived = 0;
size_t imageLength = 0;
size_t imageBytesReceived = 0;
uint8_t* imageBuffer = nullptr;
size_t rxPinIndex = 0;
TickType_t lastDataTick = 0;

uint8_t* previewFrame = nullptr;
size_t previewFrameLength = 0;
bool previewActive = false;
bool frameRequested = false;

bool captureArmed = false;
TickType_t captureStartTick = 0;
uint16_t lastFrameWidth = 0;
size_t lastFrameLength = 0;

/**
 * @brief Reads a little-endian 32-bit frame length.
 * @param bytes Four length bytes.
 * @return Decoded length.
 */
uint32_t readLength(const uint8_t* bytes) {
  return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

/**
 * @brief Drops a partial frame and returns to the sync state.
 */
void resetReceive() {
  receiveState = ReceiveState::WaitingForMagic;
  magicMatches = 0;
  lengthBytesReceived = 0;
  imageLength = 0;
  imageBytesReceived = 0;
  if (imageBuffer != nullptr) {
    heap_caps_free(imageBuffer);
    imageBuffer = nullptr;
  }
}

/**
 * @brief Switches the receiver to one of the candidate data pins.
 * @param index Index into CamRxPins.
 */
void selectRxPin(size_t index) {
  rxPinIndex = index;
  ESP_ERROR_CHECK(uart_set_pin(CamUart, UART_PIN_NO_CHANGE, CamRxPins[index], UART_PIN_NO_CHANGE,
                               UART_PIN_NO_CHANGE));
  uart_flush_input(CamUart);
  resetReceive();
}

/**
 * @brief Finds the picture width in the JPEG start-of-frame marker.
 * @param data JPEG bytes.
 * @param length Number of JPEG bytes.
 * @param width Receives the picture width.
 * @return True when the width was found.
 */
bool jpegDimensions(const uint8_t* data, size_t length, uint16_t* width) {
  if (length < 4 || data[0] != 0xFF || data[1] != 0xD8) {
    return false;
  }
  size_t index = 2;
  while (index + 9 < length) {
    if (data[index] != 0xFF) {
      ++index;
      continue;
    }
    const uint8_t marker = data[index + 1];
    if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
      index += 2;
      continue;
    }
    if (marker == 0xD9 || marker == 0xDA) {
      return false;
    }
    const size_t segmentLength = (static_cast<size_t>(data[index + 2]) << 8) | data[index + 3];
    // SOF0..SOF15, except DHT (C4), JPG (C8) and DAC (CC).
    if (marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC) {
      *width = static_cast<uint16_t>((data[index + 7] << 8) | data[index + 8]);
      return *width != 0;
    }
    if (segmentLength < 2) {
      return false;
    }
    index += 2 + segmentLength;
  }
  return false;
}

/**
 * @brief Releases the stored preview frame.
 */
void freePreview() {
  if (previewFrame != nullptr) {
    heap_caps_free(previewFrame);
    previewFrame = nullptr;
  }
  previewFrameLength = 0;
}

/**
 * @brief Sends the stored preview frame when the Screen asked for one.
 */
void sendPreviewToScreen() {
  if (previewFrame == nullptr || !frameRequested) {
    return;
  }
  frameRequested = false;
  if (previewFrameLength > MaxScreenFrameBytes) {
    ESP_LOGW(Tag, "Frame of %u bytes is too big for the screen", (unsigned)previewFrameLength);
    return;
  }
  if (!displayCommunicationSendCamImage(previewFrame, previewFrameLength)) {
    ESP_LOGW(Tag, "Failed to forward the camera frame to the display");
  }
}

/**
 * @brief Handles a complete JPEG frame.
 *
 * The frame is kept as the newest preview, or forwarded as the SNAP capture
 * when a capture is armed.
 */
void handleCompleteFrame() {
  uint16_t width = 0;
  const bool isPhoto = jpegDimensions(imageBuffer, imageLength, &width) && width >= PhotoMinWidth;
  lastFrameWidth = width;
  lastFrameLength = imageLength;

  if (captureArmed && !isPhoto) {
    // Scan demo: the preview frame itself is the photo. It is forwarded
    // straight to the screen and never stored.
    captureArmed = false;
    if (imageLength > MaxScreenFrameBytes) {
      ESP_LOGW(Tag, "Capture: frame of %u bytes is too big for the screen", (unsigned)imageLength);
      if (!displayCommunicationSendCaptureFailed()) {
        ESP_LOGW(Tag, "Failed to tell the display the capture failed");
      }
    } else if (!displayCommunicationSendCamPhoto(imageBuffer, imageLength)) {
      ESP_LOGW(Tag, "Failed to send the photo to the display");
    }
  } else if (previewActive) {
    freePreview();
    previewFrame = imageBuffer;
    previewFrameLength = imageLength;
    imageBuffer = nullptr;
    sendPreviewToScreen();
  }

  if (imageBuffer != nullptr) {
    heap_caps_free(imageBuffer);
    imageBuffer = nullptr;
  }
  resetReceive();
}
} // namespace

void camCommunicationInit() {
  const uart_config_t config = {
      .baud_rate = BaudRate,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 0,
      .source_clk = UART_SCLK_DEFAULT,
      .flags = {},
  };

  ESP_ERROR_CHECK(uart_param_config(CamUart, &config));
  ESP_ERROR_CHECK(uart_driver_install(CamUart, 32 * 1024, 2048, 0, nullptr, 0));
  selectRxPin(0);
  lastDataTick = xTaskGetTickCount();
}

void camCommunicationUpdate() {
  const TickType_t now = xTaskGetTickCount();
  if (captureArmed && now - captureStartTick > pdMS_TO_TICKS(CaptureWaitMs)) {
    captureArmed = false;
    ESP_LOGW(Tag, "Capture: no preview frame arrived (last frame: %u bytes, %u wide)",
             (unsigned)lastFrameLength, (unsigned)lastFrameWidth);
    if (!displayCommunicationSendCaptureFailed()) {
      ESP_LOGW(Tag, "Failed to tell the display the capture failed");
    }
  }

  // Answer a request right away with the newest frame that is already in.
  sendPreviewToScreen();

  size_t available = 0;
  ESP_ERROR_CHECK(uart_get_buffered_data_len(CamUart, &available));
  if (available == 0) {
    if (now - lastDataTick > pdMS_TO_TICKS(PinRetryMs)) {
      selectRxPin((rxPinIndex + 1) % CamRxPinCount);
      lastDataTick = now;
    }
    return;
  }
  lastDataTick = now;

  uint8_t data[1024];
  // Drain a good part of the driver buffer per call so the receive ring does
  // not overflow while the display or the OCR server is busy.
  for (int round = 0; round < 16; ++round) {
    ESP_ERROR_CHECK(uart_get_buffered_data_len(CamUart, &available));
    if (available == 0) {
      return;
    }

    const size_t bytesToRead = available < sizeof(data) ? available : sizeof(data);
    const int bytesRead = uart_read_bytes(CamUart, data, bytesToRead, 0);
    if (bytesRead <= 0) {
      return;
    }

    for (int index = 0; index < bytesRead; ++index) {
      const uint8_t byte = data[index];

      if (receiveState == ReceiveState::WaitingForMagic) {
        if (byte == FrameMagic[magicMatches]) {
          ++magicMatches;
          if (magicMatches == sizeof(FrameMagic)) {
            receiveState = ReceiveState::ReadingHeader;
            magicMatches = 0;
            lengthBytesReceived = 0;
          }
        } else {
          magicMatches = (byte == FrameMagic[0]) ? 1 : 0;
        }
        continue;
      }

      if (receiveState == ReceiveState::ReadingHeader) {
        lengthBytes[lengthBytesReceived++] = byte;
        if (lengthBytesReceived == sizeof(lengthBytes)) {
          imageLength = readLength(lengthBytes);
          if (imageLength == 0 || imageLength > MaxImageSize) {
            ESP_LOGW(Tag, "Invalid image frame length: %u", (unsigned)imageLength);
            resetReceive();
            continue;
          }

          imageBuffer = static_cast<uint8_t*>(heap_caps_malloc(imageLength, MALLOC_CAP_8BIT));
          if (imageBuffer == nullptr) {
            ESP_LOGE(Tag, "Failed to allocate %u byte image buffer", (unsigned)imageLength);
            resetReceive();
            continue;
          }

          imageBytesReceived = 0;
          receiveState = ReceiveState::ReadingImage;
        }
        continue;
      }

      imageBuffer[imageBytesReceived] = byte;
      ++imageBytesReceived;
      if (imageBytesReceived == imageLength) {
        handleCompleteFrame();
      }
    }
  }
}

void camCommunicationRequestFrame() {
  previewActive = true;
  frameRequested = true;
}

void camCommunicationStartCapture() {
  captureArmed = true;
  captureStartTick = xTaskGetTickCount();
  // Preview frames are not wanted while the screen waits for the photo.
  previewActive = false;
  frameRequested = false;
}

void camCommunicationCancelCapture() {
  captureArmed = false;
}
