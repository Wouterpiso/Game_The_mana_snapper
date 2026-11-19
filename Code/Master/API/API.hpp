#pragma once

#include <cstddef>
#include <cstdint>

// Looks a card up on Scryfall by (fuzzy) name and returns it as a JSON string
// in the same shape as the objects in collection.json.
//
// Returns nullptr when there is no network, no match, or the answer is invalid.
// The returned string must be released with apiFreeJson().
char* apiFetchCardByName(const char* cardName);
void apiFreeJson(char* json);

// One printing of a card: set code, collector number and readable set name.
struct ApiPrinting {
    char set[12];
    char number[12];
    char setName[48];
};

// Every set a card was printed in, at most `maxPrintings` of them. The
// response is hundreds of KB, so it is scanned while streaming instead of
// being parsed into memory. Returns the number of printings found.
int apiFetchPrintings(const char* cardName, ApiPrinting* printings,
                      int maxPrintings);

// The card of one printing (`set` + `collectorNumber`) as the reduced
// collection JSON. Release with apiFreeJson().
char* apiFetchCardByPrinting(const char* set, const char* collectorNumber);

// Downloads `url` (at most `maxBytes`) and returns the raw bytes, or nullptr.
// Release with apiFreeBinary().
uint8_t* apiFetchBinary(const char* url, size_t maxBytes, size_t* length);
void apiFreeBinary(uint8_t* data);

// Sends the collection to the card server, which saves it as a JSON file.
bool apiExportCollection(const char* collectionJson);
// The collection saved on the server, as JSON. Release with apiFreeJson().
char* apiFetchCollectionJson();
