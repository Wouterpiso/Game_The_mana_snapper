#include "StateMachine.hpp"

#include "CamCommunication.hpp"
#include "Data.hpp"
#include "DisplayCommunication.hpp"
#include <stdio.h>

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
}

void stateMachineInit()
{
    currentState = State::Idle;
}

void stateMachineRun()
{
    bool receivedCommand = false;

    if (displayCommunicationHasData()) {
        const DisplayRequest request = displayCommunicationReadRequest();
        currentState = request == DisplayRequest::Cam
                           ? State::Cam
                           : request == DisplayRequest::Collection
                                 ? State::Collection
                                 : request == DisplayRequest::Display
                                       ? State::Display
                                       : State::Idle;
        receivedCommand = true;
    } else if (camCommunicationHasData()) {
        currentState = State::Cam;
        camCommunicationHandle();
        receivedCommand = true;
    }

    if (receivedCommand) {
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