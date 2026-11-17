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
DataResult dataInitializeDefault();
void dataDeinitialize();

DataResult dataAddCard(const char* cardJson);
DataResult dataChangeCardPart(const char* cardName, const char* field,
                              const char* valueJson);
DataResult dataDeleteCard(const char* cardName);

char* dataGetCard(const char* cardName);
char* dataGetCardNames();
char* dataGetCardNamesAndColors();
char* dataGetCollectionJson();
char* dataGetBackupJson();
void dataFreeJson(char* json);