#include "Data.hpp"

#include <cstring>

namespace {
cJSON* collectionDocument = nullptr;
cJSON* backupDocument = nullptr;

constexpr char DefaultCollectionJson[] = R"json({
  "object": "list",
  "total_cards": 1,
  "has_more": false,
  "next_page": null,
  "data": [
    {
      "name": "Test Card",
      "colors": ["W"]
    }
  ]
})json";

cJSON* cards(cJSON* document)
{
    return cJSON_GetObjectItemCaseSensitive(document, "data");
}

int findCardIndex(const char* cardName)
{
    cJSON* cardList = cards(collectionDocument);
    if (!cJSON_IsArray(cardList)) {
        return -1;
    }

    const int count = cJSON_GetArraySize(cardList);
    for (int index = 0; index < count; ++index) {
        cJSON* card = cJSON_GetArrayItem(cardList, index);
        cJSON* name = cJSON_GetObjectItemCaseSensitive(card, "name");
        if (cJSON_IsString(name) && std::strcmp(name->valuestring, cardName) == 0) {
            return index;
        }
    }
    return -1;
}

DataResult updateBackup()
{
    cJSON* newBackup = cJSON_Duplicate(collectionDocument, true);
    if (newBackup == nullptr) {
        return DataResult::StorageError;
    }
    cJSON_Delete(backupDocument);
    backupDocument = newBackup;
    return DataResult::Ok;
}

bool validCardName(const char* cardName)
{
    return cardName != nullptr && cardName[0] != '\0';
}

void updateTotalCards()
{
    cJSON* totalCards = cJSON_GetObjectItemCaseSensitive(
        collectionDocument, "total_cards");
    const int count = cJSON_GetArraySize(cards(collectionDocument));
    if (cJSON_IsNumber(totalCards)) {
        cJSON_SetNumberValue(totalCards, count);
    } else {
        cJSON_AddNumberToObject(collectionDocument, "total_cards", count);
    }
}
}

DataResult dataInitialize(const char* collectionJson)
{
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

    cJSON* newBackup = cJSON_Duplicate(document, true);
    if (newBackup == nullptr) {
        cJSON_Delete(document);
        return DataResult::StorageError;
    }

    cJSON_Delete(collectionDocument);
    cJSON_Delete(backupDocument);
    collectionDocument = document;
    backupDocument = newBackup;
    return DataResult::Ok;
}

DataResult dataInitializeDefault()
{
    return dataInitialize(DefaultCollectionJson);
}

void dataDeinitialize()
{
    cJSON_Delete(collectionDocument);
    cJSON_Delete(backupDocument);
    collectionDocument = nullptr;
    backupDocument = nullptr;
}

DataResult dataAddCard(const char* cardJson)
{
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
    if (findCardIndex(name->valuestring) >= 0) {
        cJSON_Delete(card);
        return DataResult::AlreadyExists;
    }

    cJSON* cardList = cards(collectionDocument);
    cJSON_AddItemToArray(cardList, card);
    updateTotalCards();
    return updateBackup();
}

DataResult dataChangeCardPart(const char* cardName, const char* field,
                              const char* valueJson)
{
    if (collectionDocument == nullptr) {
        return DataResult::NotInitialized;
    }
    if (!validCardName(cardName) || field == nullptr || field[0] == '\0' ||
        valueJson == nullptr) {
        return DataResult::InvalidArgument;
    }

    const int cardIndex = findCardIndex(cardName);
    if (cardIndex < 0) {
        return DataResult::NotFound;
    }

    cJSON* value = cJSON_Parse(valueJson);
    if (value == nullptr) {
        return DataResult::InvalidJson;
    }

    cJSON* card = cJSON_GetArrayItem(cards(collectionDocument), cardIndex);
    if (std::strcmp(field, "name") == 0 &&
        (!cJSON_IsString(value) || !validCardName(value->valuestring))) {
        cJSON_Delete(value);
        return DataResult::InvalidArgument;
    }
    if (std::strcmp(field, "name") == 0 &&
        std::strcmp(value->valuestring, cardName) != 0 &&
        findCardIndex(value->valuestring) >= 0) {
        cJSON_Delete(value);
        return DataResult::AlreadyExists;
    }

    if (!cJSON_ReplaceItemInObjectCaseSensitive(card, field, value)) {
        cJSON_AddItemToObject(card, field, value);
    }
    return updateBackup();
}

DataResult dataDeleteCard(const char* cardName)
{
    if (collectionDocument == nullptr) {
        return DataResult::NotInitialized;
    }
    if (!validCardName(cardName)) {
        return DataResult::InvalidArgument;
    }

    const int cardIndex = findCardIndex(cardName);
    if (cardIndex < 0) {
        return DataResult::NotFound;
    }

    cJSON_DeleteItemFromArray(cards(collectionDocument), cardIndex);
    updateTotalCards();
    return updateBackup();
}

char* dataGetCard(const char* cardName)
{
    if (collectionDocument == nullptr || !validCardName(cardName)) {
        return nullptr;
    }

    const int cardIndex = findCardIndex(cardName);
    if (cardIndex < 0) {
        return nullptr;
    }

    cJSON* card = cJSON_GetArrayItem(cards(collectionDocument), cardIndex);
    return cJSON_PrintUnformatted(card);
}

char* dataGetCardNames()
{
    if (collectionDocument == nullptr) {
        return nullptr;
    }

    cJSON* names = cJSON_CreateArray();
    cJSON* cardList = cards(collectionDocument);
    const int count = cJSON_GetArraySize(cardList);
    for (int index = 0; index < count; ++index) {
        cJSON* card = cJSON_GetArrayItem(cardList, index);
        cJSON* name = cJSON_GetObjectItemCaseSensitive(card, "name");
        if (!cJSON_IsString(name)) {
            cJSON_Delete(names);
            return nullptr;
        }
        cJSON_AddItemToArray(names, cJSON_CreateString(name->valuestring));
    }

    char* result = cJSON_PrintUnformatted(names);
    cJSON_Delete(names);
    return result;
}

char* dataGetCardNamesAndColors()
{
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
        cJSON_AddStringToObject(summary, "name", name->valuestring);
        cJSON_AddItemToObject(summary, "colors", cJSON_Duplicate(colors, true));
        cJSON_AddItemToArray(summaries, summary);
    }

    char* result = cJSON_PrintUnformatted(summaries);
    cJSON_Delete(summaries);
    return result;
}

char* dataGetCollectionJson()
{
    return collectionDocument == nullptr
               ? nullptr
               : cJSON_PrintUnformatted(collectionDocument);
}

char* dataGetBackupJson()
{
    return backupDocument == nullptr ? nullptr
                                     : cJSON_PrintUnformatted(backupDocument);
}

void dataFreeJson(char* json)
{
    cJSON_free(json);
}