/**
 * @file DisplayCommunication.hpp
 * @brief Line-based UART protocol between the Master and the Screen.
 */
#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @brief Request received from the Screen.
 */
enum class DisplayRequest {
  None,          /**< No complete line is available yet. */
  Idle,          /**< Screen returned to the main menu. */
  Display,       /**< Reserved menu command. */
  Collection,    /**< Screen requests the collection list. */
  CamFrame,      /**< Screen is ready for the next camera frame. */
  Capture,       /**< User pressed SNAP. */
  CaptureAbort,  /**< User left the scan screen. */
  AddCardTitle,  /**< Card title typed on the keyboard. */
  AddCardSet,    /**< Chosen printing of a found card. */
  OcrDemo,       /**< Run the OCR demo. */
  CardInfo,      /**< Request the data of a card. */
  CardImage,     /**< Request the image of a card. */
  CardRemove,    /**< Remove every copy of a card. */
  CardRemoveOne, /**< Remove one copy of a card. */
  Import,        /**< Import the collection from the card server. */
  Export         /**< Export the collection to the card server. */
};

/**
 * @brief Sets up the UART to the Screen.
 */
void displayCommunicationInit();

/**
 * @brief Checks whether the Screen sent at least one byte.
 * @return True when data is waiting in the UART buffer.
 */
bool displayCommunicationHasData();

/**
 * @brief Reads the next complete request from the Screen.
 * @return Parsed request, or DisplayRequest::None when no line is complete.
 */
DisplayRequest displayCommunicationReadRequest();

/**
 * @brief Sends the collection list to the Screen.
 * @param collectionListJson JSON array with the card summaries.
 * @return True when the whole message was written.
 */
bool displayCommunicationSendCollectionList(const char* collectionListJson);

/**
 * @brief Reports the result of adding a card.
 * @param success True when the card was added.
 * @return True when the message was written.
 */
bool displayCommunicationSendAddCardResult(bool success);

/**
 * @brief Returns the title that came with the last AddCardTitle request.
 * @return NUL-terminated card title.
 */
const char* displayCommunicationReadTitle();

/**
 * @brief Sends the selectable printings of a found card.
 * @param payload "<card name>|<set>|<number>|<set name>|..." groups.
 * @return True when the whole message was written.
 */
bool displayCommunicationSendAddCardSets(const char* payload);

/**
 * @brief Reports what the OCR demo recognised.
 * @param testImage Number of the test image that was used.
 * @param text Recognised card title.
 * @return True when the message was written.
 */
bool displayCommunicationSendOcrDemoResult(int testImage, const char* text);

/**
 * @brief Reports that the OCR demo could not recognise a card.
 * @return True when the message was written.
 */
bool displayCommunicationSendOcrDemoFailed();

/**
 * @brief Returns the card number that came with the last card request.
 * @return Zero-based card index, or -1 when no card request was handled.
 */
int displayCommunicationReadArgument();

/**
 * @brief Sends the detail data of a card.
 * @param cardJson Card detail JSON.
 * @return True when the whole message was written.
 */
bool displayCommunicationSendCardInfo(const char* cardJson);

/**
 * @brief Reports that the detail data of a card could not be read.
 * @return True when the message was written.
 */
bool displayCommunicationSendCardInfoFailed();

/**
 * @brief Sends a card image to the Screen.
 * @param data JPEG bytes.
 * @param length Number of JPEG bytes.
 * @return True when the whole message was written.
 */
bool displayCommunicationSendCardImage(const uint8_t* data, size_t length);

/**
 * @brief Reports that a card image could not be loaded.
 * @return True when the message was written.
 */
bool displayCommunicationSendCardImageFailed();

/**
 * @brief Sends a live camera frame to the Screen.
 * @param data JPEG bytes.
 * @param length Number of JPEG bytes.
 * @return True when the whole message was written.
 */
bool displayCommunicationSendCamImage(const uint8_t* data, size_t length);

/**
 * @brief Sends the preview frame that was shown when SNAP was pressed.
 * @param data JPEG bytes.
 * @param length Number of JPEG bytes.
 * @return True when the whole message was written.
 */
bool displayCommunicationSendCamPhoto(const uint8_t* data, size_t length);

/**
 * @brief Reports that no preview frame arrived in time.
 * @return True when the message was written.
 */
bool displayCommunicationSendCaptureFailed();

/**
 * @brief Reports the result of a remove request.
 * @param success True when the card was removed.
 * @return True when the message was written.
 */
bool displayCommunicationSendCardRemoveResult(bool success);

/**
 * @brief Reports the result of an import request.
 * @param success True when the collection was imported.
 * @return True when the message was written.
 */
bool displayCommunicationSendImportResult(bool success);

/**
 * @brief Reports the result of an export request.
 * @param success True when the collection was exported.
 * @return True when the message was written.
 */
bool displayCommunicationSendExportResult(bool success);
