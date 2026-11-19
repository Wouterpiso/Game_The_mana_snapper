#pragma once

enum class DisplayRequest {
    Idle,
    Display,
    Collection,
    Cam
};

void displayCommunicationInit();
bool displayCommunicationHasData();
DisplayRequest displayCommunicationReadRequest();
bool displayCommunicationSendCollectionList(const char* collectionListJson);
bool displayCommunicationSendAddCardResult(bool success);
