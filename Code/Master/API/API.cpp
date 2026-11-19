/**
 * @file API.cpp
 * @brief Implements Scryfall lookups and card-server import/export.
 */
#include "API.hpp"

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
constexpr char Tag[] = "API";
constexpr char ScryfallNamedUrl[] = "https://api.scryfall.com/cards/named?fuzzy=";
constexpr char ScryfallSearchUrl[] = "https://api.scryfall.com/cards/search?q=";
constexpr char ScryfallCardUrl[] = "https://api.scryfall.com/cards/";
constexpr char ImageProxyPrefix[] = "https://wsrv.nl/?url=";
constexpr char ImageProxySuffix[] = "&output=jpg&w=146&h=204&q=85";
constexpr size_t MaxCollectionImportSize = 256 * 1024;
constexpr size_t InitialResponseSize = 8 * 1024;
constexpr size_t MaxResponseSize = 48 * 1024;
// A printing search page is hundreds of KB; it is scanned while it streams in.
constexpr size_t MaxSearchResponseSize = 1024 * 1024;
constexpr int MaxPrintingPages = 3;

/**
 * @brief Percent-encodes a string for use in a URL.
 * @param text Text to encode.
 * @return Encoded text.
 */
std::string urlEncode(const char* text) {
  static const char hex[] = "0123456789ABCDEF";
  std::string encoded;
  for (const char* c = text; *c != '\0'; ++c) {
    const unsigned char byte = static_cast<unsigned char>(*c);
    const bool unreserved = (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
                            (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' ||
                            byte == '.' || byte == '~';
    if (unreserved) {
      encoded += static_cast<char>(byte);
    } else {
      encoded += '%';
      encoded += hex[byte >> 4];
      encoded += hex[byte & 0x0F];
    }
  }
  return encoded;
}

/**
 * @brief Downloads a response body in chunks and passes each chunk to a sink.
 *
 * Nothing of the body is kept in memory.
 *
 * @param url Address to request.
 * @param accept Value of the Accept header.
 * @param maxSize Maximum accepted response size.
 * @param sink Callback that receives each chunk.
 * @param context Passed to @p sink unchanged.
 * @return False on a transport error, a non-200 status, or a rejected chunk.
 */
bool streamGet(const char* url, const char* accept, size_t maxSize,
               bool (*sink)(void*, const char*, size_t), void* context) {
  esp_http_client_config_t config = {};
  config.url = url;
  config.method = HTTP_METHOD_GET;
  config.timeout_ms = 15000;
  config.crt_bundle_attach = esp_crt_bundle_attach;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == nullptr) {
    ESP_LOGE(Tag, "Failed to create HTTP client");
    return false;
  }

  // Scryfall rejects requests without these two headers.
  esp_http_client_set_header(client, "User-Agent", "ManaSnapper/1.0");
  esp_http_client_set_header(client, "Accept", accept);

  esp_err_t err = esp_http_client_open(client, 0);
  if (err != ESP_OK) {
    ESP_LOGE(Tag, "Failed to open request: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return false;
  }

  // Returns -1 for chunked responses, which is fine: we read until done.
  esp_http_client_fetch_headers(client);
  const int status = esp_http_client_get_status_code(client);
  if (status != 200) {
    ESP_LOGW(Tag, "Scryfall answered HTTP %d", status);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return false;
  }

  size_t total = 0;
  bool ok = true;
  char data[1024];
  for (;;) {
    const int bytesRead = esp_http_client_read(client, data, sizeof(data));
    if (bytesRead < 0) {
      ESP_LOGE(Tag, "Failed to read response");
      ok = false;
      break;
    }
    if (bytesRead == 0) {
      break;
    }
    total += static_cast<size_t>(bytesRead);
    if (total > maxSize) {
      ESP_LOGE(Tag, "Response larger than %u bytes", (unsigned)maxSize);
      ok = false;
      break;
    }
    if (!sink(context, data, static_cast<size_t>(bytesRead))) {
      ok = false;
      break;
    }
  }

  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  return ok;
}

/**
 * @brief Sink that collects the whole body into a heap buffer.
 */
struct BufferSink {
  char* body = nullptr;
  size_t capacity = 0;
  size_t length = 0;
};

/**
 * @brief Appends one chunk to a BufferSink, growing it when needed.
 * @param context BufferSink to append to.
 * @param data Chunk bytes.
 * @param length Number of bytes in @p data.
 * @return False when memory runs out.
 */
bool appendToBuffer(void* context, const char* data, size_t length) {
  BufferSink* sink = static_cast<BufferSink*>(context);
  if (sink->body == nullptr) {
    sink->capacity = InitialResponseSize;
    sink->body = static_cast<char*>(std::malloc(sink->capacity));
    if (sink->body == nullptr) {
      ESP_LOGE(Tag, "Out of memory while reading response");
      return false;
    }
  }
  if (sink->length + length + 1 > sink->capacity) {
    size_t capacity = sink->capacity;
    while (sink->length + length + 1 > capacity) {
      capacity *= 2;
    }
    char* grown = static_cast<char*>(std::realloc(sink->body, capacity));
    if (grown == nullptr) {
      ESP_LOGE(Tag, "Out of memory while reading response");
      return false;
    }
    sink->body = grown;
    sink->capacity = capacity;
  }
  std::memcpy(sink->body + sink->length, data, length);
  sink->length += length;
  return true;
}

/**
 * @brief Downloads a response body into a NUL-terminated heap buffer.
 * @param url Address to request.
 * @param accept Value of the Accept header.
 * @param maxSize Maximum accepted response size.
 * @param outLength Receives the body length, may be nullptr.
 * @return Body buffer, or nullptr on failure.
 */
char* httpGet(const char* url, const char* accept, size_t maxSize, size_t* outLength) {
  BufferSink sink;
  if (!streamGet(url, accept, maxSize, appendToBuffer, &sink)) {
    std::free(sink.body);
    return nullptr;
  }
  if (sink.body == nullptr) {
    sink.body = static_cast<char*>(std::malloc(1));
    if (sink.body == nullptr) {
      return nullptr;
    }
  }
  sink.body[sink.length] = '\0';
  if (outLength != nullptr) {
    *outLength = sink.length;
  }
  return sink.body;
}

/**
 * @brief Copies a field from a card, or from its first face.
 * @param destination Object that receives the field.
 * @param card Scryfall card object.
 * @param face First face of a double-faced card, may be nullptr.
 * @param key Field name.
 */
void copyField(cJSON* destination, cJSON* card, cJSON* face, const char* key) {
  cJSON* value = cJSON_GetObjectItemCaseSensitive(card, key);
  if (value == nullptr && face != nullptr) {
    value = cJSON_GetObjectItemCaseSensitive(face, key);
  }
  if (value != nullptr) {
    cJSON_AddItemToObject(destination, key, cJSON_Duplicate(value, true));
  }
}

/**
 * @brief Reduces a Scryfall card to the fields the collection stores.
 * @param scryfallCard Parsed Scryfall card object.
 * @return New collection card object, or nullptr when the card has no name.
 */
cJSON* buildCollectionCard(cJSON* scryfallCard) {
  cJSON* faces = cJSON_GetObjectItemCaseSensitive(scryfallCard, "card_faces");
  cJSON* firstFace = cJSON_IsArray(faces) ? cJSON_GetArrayItem(faces, 0) : nullptr;

  cJSON* name = cJSON_GetObjectItemCaseSensitive(scryfallCard, "name");
  if (!cJSON_IsString(name) || name->valuestring[0] == '\0') {
    return nullptr;
  }

  cJSON* card = cJSON_CreateObject();
  if (card == nullptr) {
    return nullptr;
  }

  static const char* const fields[] = {
      "name",           "mana_cost",   "cmc",        "type_line", "oracle_text",  "colors",
      "color_identity", "keywords",    "legalities", "reserved",  "game_changer", "set",
      "rarity",         "flavor_text",
  };
  for (const char* field : fields) {
    copyField(card, scryfallCard, firstFace, field);
  }

  // The collection list needs a "colors" array on every card.
  if (cJSON_GetObjectItemCaseSensitive(card, "colors") == nullptr) {
    cJSON_AddItemToObject(card, "colors", cJSON_CreateArray());
  }

  cJSON* imageUris = cJSON_GetObjectItemCaseSensitive(scryfallCard, "image_uris");
  if (imageUris == nullptr && firstFace != nullptr) {
    imageUris = cJSON_GetObjectItemCaseSensitive(firstFace, "image_uris");
  }
  cJSON* small = cJSON_GetObjectItemCaseSensitive(imageUris, "small");
  if (cJSON_IsString(small)) {
    const std::string thumb =
        std::string(ImageProxyPrefix) + urlEncode(small->valuestring) + ImageProxySuffix;
    cJSON* image = cJSON_CreateObject();
    cJSON_AddStringToObject(image, "thumb", thumb.c_str());
    cJSON_AddItemToObject(card, "image", image);
  }
  return card;
}

/**
 * @brief Converts a Scryfall card body to the reduced collection JSON.
 * @param body Response body; ownership is taken.
 * @return Collection card JSON, or nullptr on failure.
 */
char* collectionCardFromBody(char* body) {
  cJSON* scryfallCard = cJSON_Parse(body);
  std::free(body);
  if (scryfallCard == nullptr) {
    ESP_LOGE(Tag, "Scryfall returned invalid JSON");
    return nullptr;
  }

  cJSON* card = buildCollectionCard(scryfallCard);
  cJSON_Delete(scryfallCard);
  if (card == nullptr) {
    ESP_LOGE(Tag, "Scryfall card has no name");
    return nullptr;
  }

  char* json = cJSON_PrintUnformatted(card);
  cJSON_Delete(card);
  return json;
}

/**
 * @brief Copies a JSON string value and drops its escape sequences.
 * @param out Destination buffer.
 * @param outSize Size of @p out in bytes.
 * @param raw Escaped JSON string value.
 */
void decodeJsonString(char* out, size_t outSize, const std::string& raw) {
  size_t written = 0;
  for (size_t index = 0; index < raw.size() && written + 1 < outSize; ++index) {
    char character = raw[index];
    if (character == '\\' && index + 1 < raw.size()) {
      const char escape = raw[++index];
      if (escape == 'u') {
        index += 4;
        character = '?';
      } else {
        character = escape;
      }
    }
    out[written++] = character;
  }
  out[written] = '\0';
}

/**
 * @brief Finds the closing quote of a JSON string.
 * @param text Text to search.
 * @param start Position just after the opening quote.
 * @return Position of the closing quote, or std::string::npos.
 */
size_t jsonStringEnd(const std::string& text, size_t start) {
  for (size_t index = start; index < text.size(); ++index) {
    if (text[index] == '\\') {
      ++index;
      continue;
    }
    if (text[index] == '"') {
      return index;
    }
  }
  return std::string::npos;
}

/**
 * @brief Checks whether a Scryfall card name matches the requested name.
 *
 * Comparison ignores case and also matches on the front half of a double-faced
 * card ("Front // Back").
 *
 * @param name Name returned by Scryfall.
 * @param target Name that was requested.
 * @return True when both names refer to the same card.
 */
bool nameMatches(const char* name, const char* target) {
  auto equalsIgnoringCase = [target](const char* text, size_t length) {
    if (std::strlen(target) != length) {
      return false;
    }
    for (size_t index = 0; index < length; ++index) {
      if (std::tolower(static_cast<unsigned char>(text[index])) !=
          std::tolower(static_cast<unsigned char>(target[index]))) {
        return false;
      }
    }
    return true;
  };
  if (equalsIgnoringCase(name, std::strlen(name))) {
    return true;
  }
  size_t length = 0;
  while (name[length] != '\0' && !(name[length] == '/' && name[length + 1] == '/')) {
    ++length;
  }
  while (length > 0 && name[length - 1] == ' ') {
    --length;
  }
  return equalsIgnoringCase(name, length);
}

/**
 * @brief Extracts printings from a search response while it streams in.
 *
 * Only printings of the requested card are kept, so the (often hundreds of KB)
 * response never has to be stored completely.
 */
struct PrintingScanner {
  ApiPrinting* printings;
  int maxPrintings;
  const char* targetName;
  int count = 0;
  bool hasMore = false;
  bool full = false;
  int stage = 0; // 0 item start, 1 name, 2 set, 3 set_name, 4 collector_number
  char name[64] = {};
  ApiPrinting current = {};
  std::string buffer;
  std::string tail;

  void feed(const char* data, size_t length) {
    if (!hasMore) {
      tail.append(data, length);
      if (tail.find("\"has_more\":true") != std::string::npos) {
        hasMore = true;
      }
      if (tail.size() > 32) {
        tail.erase(0, tail.size() - 32);
      }
    }
    buffer.append(data, length);
    static const char* const keys[] = {"\"object\":\"card\"", "\"name\":\"", "\"set\":\"",
                                       "\"set_name\":\"", "\"collector_number\":\""};
    for (;;) {
      const char* key = keys[stage];
      const size_t keyLength = std::strlen(key);
      const size_t found = buffer.find(key);
      if (found == std::string::npos) {
        // Keep a possible partial key for the next chunk.
        if (buffer.size() > keyLength) {
          buffer.erase(0, buffer.size() - (keyLength - 1));
        }
        return;
      }
      if (stage == 0) {
        buffer.erase(0, found + keyLength);
        stage = 1;
        continue;
      }
      const size_t valueStart = found + keyLength;
      const size_t end = jsonStringEnd(buffer, valueStart);
      if (end == std::string::npos) {
        if (found > 0) {
          buffer.erase(0, found);
        }
        return;
      }
      const std::string value = buffer.substr(valueStart, end - valueStart);
      buffer.erase(0, end + 1);

      if (stage == 1) {
        decodeJsonString(name, sizeof(name), value);
        // Other cards in the list (e.g. the back face of a reprint) are
        // skipped; only this card's printings are wanted.
        stage = nameMatches(name, targetName) ? 2 : 0;
      } else if (stage == 2) {
        decodeJsonString(current.set, sizeof(current.set), value);
        stage = 3;
      } else if (stage == 3) {
        decodeJsonString(current.setName, sizeof(current.setName), value);
        stage = 4;
      } else {
        decodeJsonString(current.number, sizeof(current.number), value);
        if (current.set[0] != '\0' && current.number[0] != '\0') {
          if (count < maxPrintings) {
            printings[count] = current;
          }
          ++count;
        }
        if (count >= maxPrintings) {
          full = true;
          return;
        }
        current = ApiPrinting{};
        stage = 0;
      }
    }
  }
};

/**
 * @brief Feeds one response chunk to a PrintingScanner.
 * @param context PrintingScanner.
 * @param data Chunk bytes.
 * @param length Number of bytes in @p data.
 * @return False when the requested number of printings has been found.
 */
bool feedScanner(void* context, const char* data, size_t length) {
  PrintingScanner* scanner = static_cast<PrintingScanner*>(context);
  scanner->feed(data, length);
  return !scanner->full;
}

/**
 * @brief Resolves a typed name to the exact name Scryfall uses.
 *
 * The fuzzy lookup accepts typos, but the printing search needs the real name.
 *
 * @param cardName Name entered by the user.
 * @param out Buffer that receives the canonical name.
 * @param outSize Size of @p out in bytes.
 * @return True when the name was resolved.
 */
bool resolveCanonicalName(const char* cardName, char* out, size_t outSize) {
  const std::string url = std::string(ScryfallNamedUrl) + urlEncode(cardName);
  char* body = httpGet(url.c_str(), "application/json", MaxResponseSize, nullptr);
  if (body == nullptr) {
    return false;
  }
  cJSON* card = cJSON_Parse(body);
  std::free(body);
  if (card == nullptr) {
    return false;
  }
  cJSON* name = cJSON_GetObjectItemCaseSensitive(card, "name");
  const bool ok = cJSON_IsString(name) && name->valuestring[0] != '\0';
  if (ok) {
    std::snprintf(out, outSize, "%s", name->valuestring);
  }
  cJSON_Delete(card);
  return ok;
}
} // namespace

char* apiFetchCardByName(const char* cardName) {
  if (cardName == nullptr || cardName[0] == '\0') {
    return nullptr;
  }

  const std::string url = std::string(ScryfallNamedUrl) + urlEncode(cardName);

  char* body = httpGet(url.c_str(), "application/json", MaxResponseSize, nullptr);
  if (body == nullptr) {
    return nullptr;
  }
  return collectionCardFromBody(body);
}

void apiFreeJson(char* json) {
  cJSON_free(json);
}

int apiFetchPrintings(const char* cardName, ApiPrinting* printings, int maxPrintings) {
  if (cardName == nullptr || cardName[0] == '\0' || printings == nullptr || maxPrintings <= 0) {
    return 0;
  }

  char canonical[96];
  if (!resolveCanonicalName(cardName, canonical, sizeof(canonical))) {
    ESP_LOGE(Tag, "Scryfall has no card named \"%s\"", cardName);
    return 0;
  }

  PrintingScanner scanner;
  scanner.printings = printings;
  scanner.maxPrintings = maxPrintings;
  scanner.targetName = canonical;

  const std::string query = urlEncode((std::string("!\"") + canonical + "\"").c_str());
  for (int page = 1; page <= MaxPrintingPages && !scanner.full; ++page) {
    std::string url = std::string(ScryfallSearchUrl) + query + "&unique=prints";
    if (page > 1) {
      url += "&page=" + std::to_string(page);
    }
    const int before = scanner.count;
    streamGet(url.c_str(), "application/json", MaxSearchResponseSize, feedScanner, &scanner);
    if (!scanner.hasMore || scanner.count == before) {
      break;
    }
  }

  const int found = scanner.count < maxPrintings ? scanner.count : maxPrintings;
  return found;
}

char* apiFetchCardByPrinting(const char* set, const char* collectorNumber) {
  if (set == nullptr || set[0] == '\0' || collectorNumber == nullptr ||
      collectorNumber[0] == '\0') {
    return nullptr;
  }

  const std::string url =
      std::string(ScryfallCardUrl) + urlEncode(set) + "/" + urlEncode(collectorNumber);

  char* body = httpGet(url.c_str(), "application/json", MaxResponseSize, nullptr);
  if (body == nullptr) {
    return nullptr;
  }
  return collectionCardFromBody(body);
}

uint8_t* apiFetchBinary(const char* url, size_t maxBytes, size_t* length) {
  if (url == nullptr || url[0] == '\0' || length == nullptr) {
    return nullptr;
  }
  *length = 0;
  char* body = httpGet(url, "image/jpeg,*/*", maxBytes, length);
  if (body != nullptr && *length == 0) {
    std::free(body);
    return nullptr;
  }
  return reinterpret_cast<uint8_t*>(body);
}

void apiFreeBinary(uint8_t* data) {
  std::free(data);
}

bool apiExportCollection(const char* collectionJson) {
  if (collectionJson == nullptr) {
    return false;
  }

  char serverUrl[128];
  std::snprintf(serverUrl, sizeof(serverUrl), "%s/export", CONFIG_MANA_SNAPPER_SERVER_URL);

  esp_http_client_config_t config = {};
  config.url = serverUrl;
  config.method = HTTP_METHOD_POST;
  config.timeout_ms = 15000;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == nullptr) {
    ESP_LOGE(Tag, "Failed to create HTTP client");
    return false;
  }
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_post_field(client, collectionJson,
                                 static_cast<int>(std::strlen(collectionJson)));
  const esp_err_t err = esp_http_client_perform(client);
  const int status = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);
  return err == ESP_OK && status == 200;
}

char* apiFetchCollectionJson() {
  char serverUrl[128];
  std::snprintf(serverUrl, sizeof(serverUrl), "%s/import", CONFIG_MANA_SNAPPER_SERVER_URL);
  return httpGet(serverUrl, "application/json", MaxCollectionImportSize, nullptr);
}
