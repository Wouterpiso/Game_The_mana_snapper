#pragma once

// Looks a card up on Scryfall by (fuzzy) name and returns it as a JSON string
// in the same shape as the objects in collection.json.
//
// Returns nullptr when there is no network, no match, or the answer is invalid.
// The returned string must be released with apiFreeJson().
char* apiFetchCardByName(const char* cardName);
void apiFreeJson(char* json);
