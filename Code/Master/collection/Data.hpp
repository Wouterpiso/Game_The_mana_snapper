#pragma once

#include "cJSON.h"

#include <cstddef>

enum class DataResult {
    Ok,
    InvalidArgument,
    InvalidJson,
    NotInitialized,
    NotFound,
    AlreadyExists,
    StorageError
};

DataResult dataInitialize(const char* collectionJson);
// Replaces the whole collection (an imported collection file) and saves it to
// /storage/collection.json.
DataResult dataReplaceCollection(const char* collectionJson);
// Loads the collection saved on the board, or the built-in default on first
// boot. Every change is written back to /storage/collection.json.
DataResult dataInitializeDefault();
void dataDeinitialize();

DataResult dataAddCard(const char* cardJson);
DataResult dataChangeCardPart(const char* cardName, const char* field,
                              const char* valueJson);
DataResult dataDeleteCard(const char* cardName);
// Removes card number `index` (same order as the collection list).
DataResult dataDeleteCardByIndex(int index);
// Removes one copy of card number `index`; the card itself is removed when
// its last copy goes.
DataResult dataRemoveOneCardByIndex(int index);

char* dataGetCard(const char* cardName);
// Card number `index` (same order as the collection list) as JSON, without
// the image link. Free with dataFreeJson().
char* dataGetCardDetail(int index);
// The thumbnail URL of card number `index`, or nullptr. Free with dataFreeJson().
char* dataGetCardThumbUrl(int index);
char* dataGetCardNames();
char* dataGetCardNamesAndColors();
char* dataGetCollectionJson();
char* dataGetBackupJson();
void dataFreeJson(char* json);
