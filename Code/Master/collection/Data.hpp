/**
 * @file Data.hpp
 * @brief In-memory card collection with SPIFFS persistence for the Master.
 */
#pragma once

#include "cJSON.h"

#include <cstddef>

/**
 * @brief Result of a collection operation.
 */
enum class DataResult {
  Ok,              /**< Operation completed successfully. */
  InvalidArgument, /**< A caller-supplied argument was invalid. */
  InvalidJson,     /**< The supplied JSON could not be parsed. */
  NotInitialized,  /**< The collection has not been loaded yet. */
  NotFound,        /**< The requested card does not exist. */
  AlreadyExists,   /**< The card is already present. */
  StorageError     /**< The collection could not be saved or loaded. */
};

/**
 * @brief Loads a collection document into memory.
 * @param collectionJson Valid collection JSON.
 * @return Result of the operation.
 */
DataResult dataInitialize(const char* collectionJson);

/**
 * @brief Replaces the whole collection and saves it to SPIFFS.
 * @param collectionJson Imported collection document.
 * @return Result of the operation.
 */
DataResult dataReplaceCollection(const char* collectionJson);

/**
 * @brief Loads the saved collection, or the built-in default on first boot.
 * @return Result of the operation.
 */
DataResult dataInitializeDefault();

/**
 * @brief Adds a card to the collection or increases its amount.
 * @param cardJson Reduced Scryfall card JSON.
 * @return Result of the operation.
 */
DataResult dataAddCard(const char* cardJson);

/**
 * @brief Deletes every copy of a card by name.
 * @param cardName Name of the card to delete.
 * @return Result of the operation.
 */
DataResult dataDeleteCard(const char* cardName);

/**
 * @brief Deletes a card by its position in the collection list.
 * @param index Zero-based position in the collection list.
 * @return Result of the operation.
 */
DataResult dataDeleteCardByIndex(int index);

/**
 * @brief Removes one copy of a card by its position in the list.
 *
 * The card itself is removed when its last copy goes.
 *
 * @param index Zero-based position in the collection list.
 * @return Result of the operation.
 */
DataResult dataRemoveOneCardByIndex(int index);

/**
 * @brief Returns the detail view of a card, without its image link.
 * @param index Zero-based position in the collection list.
 * @return Card detail JSON, or nullptr on failure.
 * @note Release the result with dataFreeJson().
 */
char* dataGetCardDetail(int index);

/**
 * @brief Returns the thumbnail URL of a card.
 * @param index Zero-based position in the collection list.
 * @return URL string, or nullptr when the card has no image.
 * @note Release the result with dataFreeJson().
 */
char* dataGetCardThumbUrl(int index);

/**
 * @brief Returns name, colors and amount of every card as a JSON array.
 * @return JSON array used by the collection screen, or nullptr on failure.
 * @note Release the result with dataFreeJson().
 */
char* dataGetCardNamesAndColors();

/**
 * @brief Returns the complete collection document.
 * @return Collection JSON, or nullptr when no collection is loaded.
 * @note Release the result with dataFreeJson().
 */
char* dataGetCollectionJson();

/**
 * @brief Releases JSON returned by the data module.
 * @param json String returned by one of the data functions.
 */
void dataFreeJson(char* json);
