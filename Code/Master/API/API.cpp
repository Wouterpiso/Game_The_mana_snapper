#include "API.hpp"

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
constexpr char Tag[] = "API";
constexpr char ScryfallNamedUrl[] = "https://api.scryfall.com/cards/named?fuzzy=";
constexpr char ImageProxyPrefix[] = "https://wsrv.nl/?url=";
constexpr char ImageProxySuffix[] = "&output=jpg&w=146&h=204&q=85";
constexpr size_t InitialResponseSize = 8 * 1024;
constexpr size_t MaxResponseSize = 48 * 1024;

std::string urlEncode(const char* text)
{
    static const char hex[] = "0123456789ABCDEF";
    std::string encoded;
    for (const char* c = text; *c != '\0'; ++c) {
        const unsigned char byte = static_cast<unsigned char>(*c);
        const bool unreserved = (byte >= 'A' && byte <= 'Z') ||
                                (byte >= 'a' && byte <= 'z') ||
                                (byte >= '0' && byte <= '9') || byte == '-' ||
                                byte == '_' || byte == '.' || byte == '~';
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

// Downloads the response body into a heap buffer (NUL terminated).
// Returns nullptr on any failure or when the HTTP status is not 200.
char* httpGet(const char* url)
{
    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = 15000;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGE(Tag, "Failed to create HTTP client");
        return nullptr;
    }

    // Scryfall rejects requests without these two headers.
    esp_http_client_set_header(client, "User-Agent", "ManaSnapper/1.0");
    esp_http_client_set_header(client, "Accept", "application/json");

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(Tag, "Failed to open request: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return nullptr;
    }

    // Returns -1 for chunked responses, which is fine: we read until done.
    esp_http_client_fetch_headers(client);
    const int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGW(Tag, "Scryfall answered HTTP %d", status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return nullptr;
    }

    size_t capacity = InitialResponseSize;
    size_t length = 0;
    char* body = static_cast<char*>(std::malloc(capacity));
    bool ok = body != nullptr;

    while (ok) {
        if (length + 1 >= capacity) {
            if (capacity >= MaxResponseSize) {
                ESP_LOGE(Tag, "Response larger than %u bytes", (unsigned)MaxResponseSize);
                ok = false;
                break;
            }
            capacity *= 2;
            char* grown = static_cast<char*>(std::realloc(body, capacity));
            if (grown == nullptr) {
                ESP_LOGE(Tag, "Out of memory while reading response");
                ok = false;
                break;
            }
            body = grown;
        }

        const int bytesRead = esp_http_client_read(
            client, body + length, static_cast<int>(capacity - length - 1));
        if (bytesRead < 0) {
            ESP_LOGE(Tag, "Failed to read response");
            ok = false;
        } else if (bytesRead == 0) {
            break;
        } else {
            length += static_cast<size_t>(bytesRead);
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (!ok) {
        std::free(body);
        return nullptr;
    }
    body[length] = '\0';
    return body;
}

// Copies `key` from the card, or from its first face for double-faced cards.
void copyField(cJSON* destination, cJSON* card, cJSON* face, const char* key)
{
    cJSON* value = cJSON_GetObjectItemCaseSensitive(card, key);
    if (value == nullptr && face != nullptr) {
        value = cJSON_GetObjectItemCaseSensitive(face, key);
    }
    if (value != nullptr) {
        cJSON_AddItemToObject(destination, key, cJSON_Duplicate(value, true));
    }
}

// Reduces the (large) Scryfall card to the fields the collection stores.
cJSON* buildCollectionCard(cJSON* scryfallCard)
{
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
        "name",       "mana_cost",   "cmc",           "type_line",
        "oracle_text", "colors",     "color_identity", "keywords",
        "legalities", "reserved",    "game_changer",  "set",
        "rarity",     "flavor_text",
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
        const std::string thumb = std::string(ImageProxyPrefix) +
                                  urlEncode(small->valuestring) + ImageProxySuffix;
        cJSON* image = cJSON_CreateObject();
        cJSON_AddStringToObject(image, "thumb", thumb.c_str());
        cJSON_AddItemToObject(card, "image", image);
    }
    return card;
}
}

char* apiFetchCardByName(const char* cardName)
{
    if (cardName == nullptr || cardName[0] == '\0') {
        return nullptr;
    }

    const std::string url = std::string(ScryfallNamedUrl) + urlEncode(cardName);
    ESP_LOGI(Tag, "Scryfall lookup: %s", cardName);

    char* body = httpGet(url.c_str());
    if (body == nullptr) {
        return nullptr;
    }

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

void apiFreeJson(char* json)
{
    cJSON_free(json);
}
