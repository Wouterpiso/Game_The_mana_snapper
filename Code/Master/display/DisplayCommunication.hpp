#pragma once

#include <cstddef>
#include <cstdint>

enum class DisplayRequest {
    None,
    Idle,
    Display,
    Collection,
    CamFrame,
    Capture,
    CaptureAbort,
    AddCardTitle,
    AddCardSet,
    OcrDemo,
    CardInfo,
    CardImage,
    CardRemove,
    CardRemoveOne,
    Import,
    Export
};

void displayCommunicationInit();
bool displayCommunicationHasData();
DisplayRequest displayCommunicationReadRequest();
bool displayCommunicationSendCollectionList(const char* collectionListJson);
bool displayCommunicationSendAddCardResult(bool success);

// The card title typed on the screen's keyboard ("ADD_CARD_TITLE <title>").
const char* displayCommunicationReadTitle();
// The set list for a found card: "<card name>|<set>|<number>|<set name>|..."
// with one group of three per printing. The screen answers a choice with
// "ADD_CARD_SET <index>" into this list.
bool displayCommunicationSendAddCardSets(const char* payload);
// The test image number and the text the OCR server read from it.
bool displayCommunicationSendOcrDemoResult(int testImage, const char* text);
bool displayCommunicationSendOcrDemoFailed();

// The card number that came with the last card request (info, image, remove).
int displayCommunicationReadArgument();
bool displayCommunicationSendCardInfo(const char* cardJson);
bool displayCommunicationSendCardInfoFailed();
bool displayCommunicationSendCardImage(const uint8_t* data, size_t length);
bool displayCommunicationSendCardImageFailed();
// Live camera frame ("CAM_IMAGE <length>" followed by the JPEG bytes).
bool displayCommunicationSendCamImage(const uint8_t* data, size_t length);
// The preview frame behind SNAP ("CAM_PHOTO <length>" + JPEG) so the screen
// can show it while the user decides.
bool displayCommunicationSendCamPhoto(const uint8_t* data, size_t length);
// No preview frame arrived in time; the screen goes back to the preview.
bool displayCommunicationSendCaptureFailed();
// Answer to a CardRemove request ("CARD_REMOVE_DONE" / "CARD_REMOVE_FAILED").
bool displayCommunicationSendCardRemoveResult(bool success);
// Answers to Import and Export requests.
bool displayCommunicationSendImportResult(bool success);
bool displayCommunicationSendExportResult(bool success);
