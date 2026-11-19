/**
 * @file StateMachine.cpp
 * @brief Handles Screen requests and forwards the results back to the Screen.
 */
#include "StateMachine.hpp"

#include "API.hpp"
#include "CamCommunication.hpp"
#include "Data.hpp"
#include "DisplayCommunication.hpp"
#include "ImageRecognition.hpp"
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

/**
 * @brief Looks a card up by title and adds it to the collection.
 * @param title Card title.
 * @return True when the card was added or was already present.
 */
bool addCardByTitle(const char* title) {
  if (title == nullptr) {
    return false;
  }
  while (*title == ' ') {
    ++title;
  }
  if (title[0] == '\0') {
    return false;
  }
  char* cardJson = apiFetchCardByName(title);
  if (cardJson == nullptr) {
    return false;
  }

  const DataResult result = dataAddCard(cardJson);
  apiFreeJson(cardJson);
  if (result == DataResult::AlreadyExists) {
    return true;
  }
  return result == DataResult::Ok;
}

/**
 * @brief Sends every printing of a card to the Screen.
 *
 * The Screen then asks which printing the user wants. When the printings
 * cannot be fetched, the caller falls back to adding the fuzzy match directly.
 *
 * @param cardName Card name.
 * @return True when the set list was sent.
 */
bool startSetSelection(const char* cardName) {
  while (cardName != nullptr && *cardName == ' ') {
    ++cardName;
  }
  if (cardName == nullptr || cardName[0] == '\0') {
    return false;
  }

  pendingCount = apiFetchPrintings(cardName, pendingPrintings, MaxPrintings);
  if (pendingCount <= 0) {
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
    const int written = snprintf(setListPayload + length, sizeof(setListPayload) - length,
                                 "|%s|%s|%s", printing.set, printing.number, printing.setName);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(setListPayload) - length) {
      pendingCount = index;
      break;
    }
    length += static_cast<size_t>(written);
  }
  if (pendingCount == 0 || !displayCommunicationSendAddCardSets(setListPayload)) {
    pendingCount = 0;
    return false;
  }
  return true;
}

/**
 * @brief Adds the printing the user chose on the Screen.
 * @param index Index into the pending printing list.
 */
void choosePrinting(int index) {
  bool added = false;
  if (index >= 0 && index < pendingCount) {
    const ApiPrinting& printing = pendingPrintings[index];
    char* cardJson = apiFetchCardByPrinting(printing.set, printing.number);
    if (cardJson != nullptr) {
      const DataResult result = dataAddCard(cardJson);
      apiFreeJson(cardJson);
      added = result == DataResult::Ok || result == DataResult::AlreadyExists;
    }
  }
  pendingCount = 0;
  displayCommunicationSendAddCardResult(added);
}

/**
 * @brief Builds the SPIFFS path of an OCR test image.
 * @param number Test image number, starting at 1.
 * @param path Buffer that receives the path.
 * @param pathSize Size of @p path in bytes.
 */
void testImagePath(int number, char* path, size_t pathSize) {
  snprintf(path, pathSize, TestImagePathFormat, number);
}

/**
 * @brief Checks whether an OCR test image is present in SPIFFS.
 * @param number Test image number, starting at 1.
 * @return True when the image exists.
 */
bool hasTestImage(int number) {
  char path[40];
  testImagePath(number, path, sizeof(path));
  FILE* file = fopen(path, "rb");
  if (file == nullptr) {
    return false;
  }
  fclose(file);
  return true;
}

/**
 * @brief Reads an OCR test image from SPIFFS.
 * @param number Test image number, starting at 1.
 * @param length Receives the number of bytes read.
 * @return Image buffer, or nullptr on failure.
 */
uint8_t* readTestImage(int number, size_t* length) {
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
    fclose(file);
    return nullptr;
  }
  uint8_t* data = static_cast<uint8_t*>(malloc(static_cast<size_t>(size)));
  const size_t read = data != nullptr ? fread(data, 1, static_cast<size_t>(size), file) : 0;
  fclose(file);
  if (data == nullptr || read != static_cast<size_t>(size)) {
    free(data);
    return nullptr;
  }
  *length = static_cast<size_t>(size);
  return data;
}

/**
 * @brief Recognises a random test image and starts adding the card.
 */
void runOcrDemo() {
  int available[TestImageCount];
  int count = 0;
  for (int number = 1; number <= TestImageCount; ++number) {
    if (hasTestImage(number)) {
      available[count++] = number;
    }
  }
  if (count == 0) {
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
    displayCommunicationSendOcrDemoFailed();
    return;
  }
  // The demo card goes into the collection like a typed one: first let the
  // user pick the set, then fall back to the fuzzy match.
  if (!startSetSelection(name)) {
    displayCommunicationSendAddCardResult(addCardByTitle(name));
  }
}

/**
 * @brief Sends the detail data of a card to the Screen.
 * @param index Zero-based position in the collection list.
 */
void sendCardInfo(int index) {
  char* cardJson = dataGetCardDetail(index);
  if (cardJson == nullptr || !displayCommunicationSendCardInfo(cardJson)) {
    displayCommunicationSendCardInfoFailed();
  }
  dataFreeJson(cardJson);
}

/**
 * @brief Removes a card or one copy and reports the result.
 * @param index Zero-based position in the collection list.
 * @param onlyOneCopy True to remove one copy instead of every copy.
 */
void removeCard(int index, bool onlyOneCopy) {
  const DataResult result =
      onlyOneCopy ? dataRemoveOneCardByIndex(index) : dataDeleteCardByIndex(index);
  displayCommunicationSendCardRemoveResult(result == DataResult::Ok);
}

/**
 * @brief Downloads and sends the image of a card.
 * @param index Zero-based position in the collection list.
 */
void sendCardImage(int index) {
  char* url = dataGetCardThumbUrl(index);
  size_t imageLength = 0;
  uint8_t* image = url != nullptr ? apiFetchBinary(url, MaxCardImageBytes, &imageLength) : nullptr;
  if (image == nullptr || !displayCommunicationSendCardImage(image, imageLength)) {
    displayCommunicationSendCardImageFailed();
  }
  apiFreeBinary(image);
  dataFreeJson(url);
}

/**
 * @brief Sends the collection to the card server.
 */
void exportCollection() {
  char* json = dataGetCollectionJson();
  const bool exported =
      json != nullptr && imageRecognitionEnsureWifi() && apiExportCollection(json);
  dataFreeJson(json);
  displayCommunicationSendExportResult(exported);
}

/**
 * @brief Replaces the collection with the one saved on the card server.
 */
void importCollection() {
  char* json = imageRecognitionEnsureWifi() ? apiFetchCollectionJson() : nullptr;
  bool imported = false;
  if (json != nullptr) {
    imported = dataReplaceCollection(json) == DataResult::Ok;
  }
  apiFreeJson(json);
  displayCommunicationSendImportResult(imported);
}
} // namespace

void stateMachineInit() {
  currentState = State::Idle;
}

void stateMachineRun() {
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
        if (!startSetSelection(title)) {
          displayCommunicationSendAddCardResult(addCardByTitle(title));
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
        displayCommunicationSendCollectionList(collectionList);
        dataFreeJson(collectionList);
      }
      currentState = State::Idle;
    }
  }

  camCommunicationUpdate();
}
