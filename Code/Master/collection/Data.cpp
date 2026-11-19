/**
 * @file Data.cpp
 * @brief Implements the in-memory collection and its SPIFFS persistence.
 */
#include "Data.hpp"

#include "esp_log.h"
#include "esp_spiffs.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
cJSON* collectionDocument = nullptr;

constexpr char Tag[] = "Data";
// The collection file lives in flash for now; when the SD card is wired up,
// only these three constants (and the mount call) have to change.
constexpr char StoragePartition[] = "storage";
constexpr char StorageMountPoint[] = "/storage";
constexpr char StoragePath[] = "/storage/collection.json";
bool storageMounted = false;

constexpr char DefaultCollectionJson[] = R"json({
  "object": "list",
  "total_cards": 1,
  "has_more": false,
  "next_page": null,
  "data": [
    {
      "name": "Test Card",
      "colors": ["W"],
      "amount": 1
    }
  ]
})json";

/**
 * @brief Mounts the SPIFFS partition that holds collection.json.
 * @return True when the partition is mounted.
 */
bool ensureStorageMounted() {
  if (storageMounted) {
    return true;
  }
  esp_vfs_spiffs_conf_t config = {};
  config.base_path = StorageMountPoint;
  config.partition_label = StoragePartition;
  config.max_files = 4;
  config.format_if_mount_failed = true;
  const esp_err_t error = esp_vfs_spiffs_register(&config);
  if (error != ESP_OK) {
    ESP_LOGE(Tag, "Could not mount %s: %s", StoragePartition, esp_err_to_name(error));
    return false;
  }
  storageMounted = true;
  return true;
}

/**
 * @brief Reads collection.json from SPIFFS.
 * @return File content, or nullptr on first boot or a read failure.
 */
char* readStoredCollection() {
  if (!ensureStorageMounted()) {
    return nullptr;
  }
  FILE* file = std::fopen(StoragePath, "rb");
  if (file == nullptr) {
    return nullptr;
  }
  std::fseek(file, 0, SEEK_END);
  const long size = std::ftell(file);
  std::rewind(file);
  if (size <= 0) {
    std::fclose(file);
    return nullptr;
  }
  char* json = static_cast<char*>(std::malloc(static_cast<size_t>(size) + 1));
  if (json == nullptr) {
    std::fclose(file);
    return nullptr;
  }
  const size_t read = std::fread(json, 1, static_cast<size_t>(size), file);
  std::fclose(file);
  if (read != static_cast<size_t>(size)) {
    std::free(json);
    return nullptr;
  }
  json[size] = '\0';
  return json;
}

/**
 * @brief Writes the in-memory collection to collection.json.
 * @return True when the file was written completely.
 */
bool writeStoredCollection() {
  if (collectionDocument == nullptr || !ensureStorageMounted()) {
    return false;
  }
  char* json = cJSON_PrintUnformatted(collectionDocument);
  if (json == nullptr) {
    return false;
  }
  FILE* file = std::fopen(StoragePath, "wb");
  if (file == nullptr) {
    ESP_LOGE(Tag, "Could not open %s for writing", StoragePath);
    cJSON_free(json);
    return false;
  }
  const size_t length = std::strlen(json);
  const size_t written = std::fwrite(json, 1, length, file);
  const bool closed = std::fclose(file) == 0;
  cJSON_free(json);
  if (written != length || !closed) {
    ESP_LOGE(Tag, "Could not save %s", StoragePath);
    return false;
  }
  return true;
}

/**
 * @brief Returns the "data" array of a collection document.
 * @param document Collection document.
 * @return Array item, or nullptr when it is missing.
 */
cJSON* cards(cJSON* document) {
  return cJSON_GetObjectItemCaseSensitive(document, "data");
}

/**
 * @brief Reads a string field from a card.
 * @param card Card object.
 * @param field Field name.
 * @return Field value, or nullptr when the field is missing or not a string.
 */
const char* cardString(cJSON* card, const char* field) {
  cJSON* item = cJSON_GetObjectItemCaseSensitive(card, field);
  return cJSON_IsString(item) ? item->valuestring : nullptr;
}

/**
 * @brief Finds a card by name and, optionally, set.
 * @param cardName Name to look for.
 * @param cardSet Set code, or nullptr to match on name only.
 * @return Zero-based index in the collection, or -1 when not found.
 */
int findCardIndex(const char* cardName, const char* cardSet) {
  cJSON* cardList = cards(collectionDocument);
  if (!cJSON_IsArray(cardList)) {
    return -1;
  }

  const int count = cJSON_GetArraySize(cardList);
  for (int index = 0; index < count; ++index) {
    cJSON* card = cJSON_GetArrayItem(cardList, index);
    const char* name = cardString(card, "name");
    if (name == nullptr || std::strcmp(name, cardName) != 0) {
      continue;
    }
    if (cardSet == nullptr) {
      return index;
    }
    const char* set = cardString(card, "set");
    if (std::strcmp(set != nullptr ? set : "", cardSet) == 0) {
      return index;
    }
  }
  return -1;
}

/**
 * @brief Saves the collection to SPIFFS.
 * @return Result of the save operation.
 */
DataResult saveCollection() {
  return writeStoredCollection() ? DataResult::Ok : DataResult::StorageError;
}

/**
 * @brief Checks that a card name is present and not empty.
 * @param cardName Name to check.
 * @return True when the name can be used.
 */
bool validCardName(const char* cardName) {
  return cardName != nullptr && cardName[0] != '\0';
}

/**
 * @brief Updates the total_cards field of the collection document.
 */
void updateTotalCards() {
  cJSON* totalCards = cJSON_GetObjectItemCaseSensitive(collectionDocument, "total_cards");
  const int count = cJSON_GetArraySize(cards(collectionDocument));
  if (cJSON_IsNumber(totalCards)) {
    cJSON_SetNumberValue(totalCards, count);
  } else {
    cJSON_AddNumberToObject(collectionDocument, "total_cards", count);
  }
}
} // namespace

DataResult dataInitialize(const char* collectionJson) {
  if (collectionJson == nullptr) {
    return DataResult::InvalidArgument;
  }

  cJSON* document = cJSON_Parse(collectionJson);
  if (document == nullptr || !cJSON_IsObject(document)) {
    cJSON_Delete(document);
    return DataResult::InvalidJson;
  }

  cJSON* cardList = cards(document);
  if (!cJSON_IsArray(cardList)) {
    cJSON_Delete(document);
    return DataResult::InvalidJson;
  }

  cJSON_Delete(collectionDocument);
  collectionDocument = document;
  return DataResult::Ok;
}

DataResult dataInitializeDefault() {
  char* stored = readStoredCollection();
  if (stored != nullptr) {
    const DataResult result = dataInitialize(stored);
    std::free(stored);
    if (result == DataResult::Ok) {
      return result;
    }
    ESP_LOGW(Tag, "Stored collection is invalid (error %d); using the default",
             static_cast<int>(result));
  }
  return dataInitialize(DefaultCollectionJson);
}

DataResult dataReplaceCollection(const char* collectionJson) {
  const DataResult result = dataInitialize(collectionJson);
  if (result != DataResult::Ok) {
    return result;
  }
  updateTotalCards();
  return saveCollection();
}

DataResult dataAddCard(const char* cardJson) {
  if (collectionDocument == nullptr) {
    return DataResult::NotInitialized;
  }
  if (cardJson == nullptr) {
    return DataResult::InvalidArgument;
  }

  cJSON* card = cJSON_Parse(cardJson);
  if (card == nullptr || !cJSON_IsObject(card)) {
    cJSON_Delete(card);
    return DataResult::InvalidJson;
  }

  cJSON* name = cJSON_GetObjectItemCaseSensitive(card, "name");
  if (!cJSON_IsString(name) || !validCardName(name->valuestring)) {
    cJSON_Delete(card);
    return DataResult::InvalidArgument;
  }
  const char* set = cardString(card, "set");
  const int existingIndex = findCardIndex(name->valuestring, set != nullptr ? set : "");
  if (existingIndex >= 0) {
    // The same card (name and set) is already owned: raise its amount.
    cJSON_Delete(card);
    cJSON* existingCard = cJSON_GetArrayItem(cards(collectionDocument), existingIndex);
    cJSON* amount = cJSON_GetObjectItemCaseSensitive(existingCard, "amount");
    if (cJSON_IsNumber(amount)) {
      cJSON_SetNumberValue(amount, amount->valueint + 1);
    } else {
      cJSON_AddNumberToObject(existingCard, "amount", 2);
    }
    return saveCollection();
  }

  if (cJSON_GetObjectItemCaseSensitive(card, "amount") == nullptr) {
    cJSON_AddNumberToObject(card, "amount", 1);
  }
  cJSON* cardList = cards(collectionDocument);
  cJSON_AddItemToArray(cardList, card);
  updateTotalCards();
  return saveCollection();
}

DataResult dataDeleteCard(const char* cardName) {
  if (collectionDocument == nullptr) {
    return DataResult::NotInitialized;
  }
  if (!validCardName(cardName)) {
    return DataResult::InvalidArgument;
  }

  const int cardIndex = findCardIndex(cardName, nullptr);
  if (cardIndex < 0) {
    return DataResult::NotFound;
  }

  cJSON_DeleteItemFromArray(cards(collectionDocument), cardIndex);
  updateTotalCards();
  return saveCollection();
}

DataResult dataDeleteCardByIndex(int index) {
  if (collectionDocument == nullptr) {
    return DataResult::NotInitialized;
  }
  cJSON* cardList = cards(collectionDocument);
  if (!cJSON_IsArray(cardList) || index < 0 || index >= cJSON_GetArraySize(cardList)) {
    return DataResult::NotFound;
  }

  cJSON_DeleteItemFromArray(cardList, index);
  updateTotalCards();
  return saveCollection();
}

DataResult dataRemoveOneCardByIndex(int index) {
  if (collectionDocument == nullptr) {
    return DataResult::NotInitialized;
  }
  cJSON* cardList = cards(collectionDocument);
  if (!cJSON_IsArray(cardList) || index < 0 || index >= cJSON_GetArraySize(cardList)) {
    return DataResult::NotFound;
  }

  cJSON* card = cJSON_GetArrayItem(cardList, index);
  cJSON* amount = cJSON_GetObjectItemCaseSensitive(card, "amount");
  const int current = cJSON_IsNumber(amount) ? amount->valueint : 1;
  if (current > 1) {
    cJSON_SetNumberValue(amount, current - 1);
    return saveCollection();
  }

  // The last copy goes, so the card leaves the collection.
  cJSON_DeleteItemFromArray(cardList, index);
  updateTotalCards();
  return saveCollection();
}

char* dataGetCardDetail(int index) {
  if (collectionDocument == nullptr || index < 0) {
    return nullptr;
  }
  cJSON* card = cJSON_GetArrayItem(cards(collectionDocument), index);
  if (card == nullptr) {
    return nullptr;
  }

  // Everything the screen can show, except the image link (the image is
  // requested separately) and with an amount that is always present.
  cJSON* detail = cJSON_Duplicate(card, true);
  if (detail == nullptr) {
    return nullptr;
  }
  cJSON_DeleteItemFromObjectCaseSensitive(detail, "image");
  if (cJSON_GetObjectItemCaseSensitive(detail, "amount") == nullptr) {
    cJSON_AddNumberToObject(detail, "amount", 1);
  }
  char* result = cJSON_PrintUnformatted(detail);
  cJSON_Delete(detail);
  return result;
}

char* dataGetCardThumbUrl(int index) {
  if (collectionDocument == nullptr || index < 0) {
    return nullptr;
  }
  cJSON* card = cJSON_GetArrayItem(cards(collectionDocument), index);
  cJSON* image = cJSON_GetObjectItemCaseSensitive(card, "image");
  cJSON* thumb = cJSON_GetObjectItemCaseSensitive(image, "thumb");
  if (!cJSON_IsString(thumb) || thumb->valuestring[0] == '\0') {
    return nullptr;
  }
  const size_t length = std::strlen(thumb->valuestring);
  char* copy = static_cast<char*>(cJSON_malloc(length + 1));
  if (copy != nullptr) {
    std::memcpy(copy, thumb->valuestring, length + 1);
  }
  return copy;
}

char* dataGetCardNamesAndColors() {
  if (collectionDocument == nullptr) {
    return nullptr;
  }

  cJSON* summaries = cJSON_CreateArray();
  cJSON* cardList = cards(collectionDocument);
  const int count = cJSON_GetArraySize(cardList);
  for (int index = 0; index < count; ++index) {
    cJSON* card = cJSON_GetArrayItem(cardList, index);
    cJSON* name = cJSON_GetObjectItemCaseSensitive(card, "name");
    cJSON* colors = cJSON_GetObjectItemCaseSensitive(card, "colors");
    if (!cJSON_IsString(name) || !cJSON_IsArray(colors)) {
      cJSON_Delete(summaries);
      return nullptr;
    }

    cJSON* summary = cJSON_CreateObject();
    if (summary == nullptr) {
      cJSON_Delete(summaries);
      return nullptr;
    }
    cJSON* amount = cJSON_GetObjectItemCaseSensitive(card, "amount");
    cJSON_AddStringToObject(summary, "name", name->valuestring);
    cJSON_AddItemToObject(summary, "colors", cJSON_Duplicate(colors, true));
    cJSON_AddNumberToObject(summary, "amount", cJSON_IsNumber(amount) ? amount->valueint : 1);
    cJSON_AddItemToArray(summaries, summary);
  }

  char* result = cJSON_PrintUnformatted(summaries);
  cJSON_Delete(summaries);
  return result;
}

char* dataGetCollectionJson() {
  return collectionDocument == nullptr ? nullptr : cJSON_PrintUnformatted(collectionDocument);
}

void dataFreeJson(char* json) {
  cJSON_free(json);
}
