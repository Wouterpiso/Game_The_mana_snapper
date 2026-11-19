/**
 * @file API.hpp
 * @brief Scryfall lookups and card-server import/export for the Master.
 */
#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @brief Looks a card up on Scryfall by (fuzzy) name.
 *
 * The returned JSON has the same shape as the objects in collection.json.
 *
 * @param cardName Name typed on the screen or read by OCR.
 * @return Reduced collection JSON, or nullptr when there is no network, no
 *         match, or the answer is invalid.
 * @note Release the result with apiFreeJson().
 */
char* apiFetchCardByName(const char* cardName);

/**
 * @brief Releases JSON returned by the API module.
 * @param json String returned by one of the API functions.
 */
void apiFreeJson(char* json);

/**
 * @brief One printing of a card: set code, collector number and set name.
 */
struct ApiPrinting {
  char set[12];     /**< Set code, for example "lea". */
  char number[12];  /**< Collector number inside the set. */
  char setName[48]; /**< Readable set name shown on the screen. */
};

/**
 * @brief Finds every set a card was printed in.
 *
 * The Scryfall response is hundreds of KB, so it is scanned while streaming
 * instead of being parsed into memory.
 *
 * @param cardName Name of the card.
 * @param printings Array that receives the printings.
 * @param maxPrintings Capacity of @p printings.
 * @return Number of printings written to @p printings.
 */
int apiFetchPrintings(const char* cardName, ApiPrinting* printings, int maxPrintings);

/**
 * @brief Fetches one printing of a card.
 * @param set Set code.
 * @param collectorNumber Collector number inside the set.
 * @return Reduced collection JSON, or nullptr on failure.
 * @note Release the result with apiFreeJson().
 */
char* apiFetchCardByPrinting(const char* set, const char* collectorNumber);

/**
 * @brief Downloads a binary resource, such as a card image.
 * @param url Address to download.
 * @param maxBytes Maximum accepted response size.
 * @param length Receives the number of bytes returned.
 * @return Heap buffer with the bytes, or nullptr on failure.
 * @note Release the result with apiFreeBinary().
 */
uint8_t* apiFetchBinary(const char* url, size_t maxBytes, size_t* length);

/**
 * @brief Releases a binary buffer returned by apiFetchBinary().
 * @param data Buffer returned by apiFetchBinary().
 */
void apiFreeBinary(uint8_t* data);

/**
 * @brief Sends the collection to the card server, which saves it as JSON.
 * @param collectionJson Collection document to export.
 * @return True when the server accepted the export.
 */
bool apiExportCollection(const char* collectionJson);

/**
 * @brief Fetches the collection saved on the card server.
 * @return Collection JSON, or nullptr on failure.
 * @note Release the result with apiFreeJson().
 */
char* apiFetchCollectionJson();
