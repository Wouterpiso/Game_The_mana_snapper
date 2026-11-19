#include "StateMachine.hpp"

#include "API.hpp"
#include "CamCommunication.hpp"
#include "Data.hpp"
#include "DisplayCommunication.hpp"
#include "ImageReconition.hpp"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// test_Image/test.jpeg, linked into the firmware by EMBED_FILES in main/CMakeLists.txt.
extern "C" const uint8_t testImageStart[] asm("_binary_test_jpeg_start");
extern "C" const uint8_t testImageEnd[] asm("_binary_test_jpeg_end");

namespace {
State currentState = State::Idle;

const char* stateName(State state)
{
    switch (state) {
    case State::Display:
        return "DISPLAY";
    case State::Collection:
        return "COLLECTION";
    case State::Cam:
        return "CAM";
    case State::Idle:
    default:
        return "IDLE";
    }
}

// image -> OCR server (card name) -> Scryfall -> collection
bool addCardFromImage(const uint8_t* image, size_t imageLength)
{
    char cardName[128];
    if (!imageRecognitionGetCardName(image, imageLength, cardName, sizeof(cardName))) {
        printf("Add card: could not read a card name from the image\n");
        return false;
    }
    printf("Add card: recognised \"%s\"\n", cardName);

    char* cardJson = apiFetchCardByName(cardName);
    if (cardJson == nullptr) {
        printf("Add card: Scryfall lookup failed for \"%s\"\n", cardName);
        return false;
    }

    const DataResult result = dataAddCard(cardJson);
    apiFreeJson(cardJson);
    if (result == DataResult::AlreadyExists) {
        printf("Add card: \"%s\" is already in the collection\n", cardName);
        return true;
    }
    if (result != DataResult::Ok) {
        printf("Add card: could not store the card (error %d)\n", static_cast<int>(result));
        return false;
    }
    printf("Add card: \"%s\" added to the collection\n", cardName);
    return true;
}
}

void stateMachineInit()
{
    currentState = State::Idle;
}

void stateMachineRun()
{
    bool receivedCommand = false;
    bool addCardRequested = false;

    if (displayCommunicationHasData()) {
        const DisplayRequest request = displayCommunicationReadRequest();
        currentState = request == DisplayRequest::Cam
                           ? State::Cam
                           : request == DisplayRequest::Collection
                                 ? State::Collection
                                 : request == DisplayRequest::Display
                                       ? State::Display
                                       : State::Idle;
        addCardRequested = request == DisplayRequest::Cam;
        receivedCommand = true;
    } else if (camCommunicationHasData()) {
        currentState = State::Cam;
        camCommunicationHandle();
        receivedCommand = true;
    }

    if (receivedCommand) {
        if (addCardRequested) {
            // For now the camera is skipped: the embedded test image is used.
            // The screen shows LOADING until it gets the result back.
            const bool added = addCardFromImage(
                testImageStart, static_cast<size_t>(testImageEnd - testImageStart));
            if (!displayCommunicationSendAddCardResult(added)) {
                printf("Failed to send add card result\n");
            }
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
