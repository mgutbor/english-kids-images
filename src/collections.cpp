#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "config.h"
#include "storage.h"
#include "collections.h"
#include "game.h"

Collection collections[MAX_COLLECTIONS];
int totalCollections = 0;
int selectedCollectionIndex = 0;

Word words[MAX_WORDS];
int totalWords = 0;

char activeCollection[MAX_COLLECTION_ID] = "";
char collectionTitle[64] = "";

bool collectionLoaded = false;

static bool validIdentifier(const char* value) {
    if (!value || !value[0]) return false;

    for (int i = 0; value[i]; i++) {
        char c = value[i];

        if (!(
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-'
        )) {
            return false;
        }
    }

    return true;
}

static bool downloadText(const String& url, String& payload) {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;

    String requestUrl = url;
    requestUrl += "?_ts=" + String(millis());

    if (!http.begin(client, requestUrl)) {
        return false;
    }

    http.addHeader("Cache-Control", "no-cache");

    int status = http.GET();

    if (status != HTTP_CODE_OK) {
        Serial.printf("HTTP error: %d\n", status);
        http.end();
        return false;
    }

    payload = http.getString();
    http.end();

    return payload.length() > 0;
}

static String collectionContentPath(const char* id) {
    return String("/collections/") + id + "/content.json";
}

static bool parseCatalog(const String& json) {
    DynamicJsonDocument doc(CATALOG_JSON_CAPACITY);

    if (deserializeJson(doc, json)) {
        Serial.println("Catálogo JSON inválido");
        return false;
    }

    JsonArray array = doc["collections"].as<JsonArray>();

    if (array.isNull() || array.size() == 0 ||
        array.size() > MAX_COLLECTIONS) {
        return false;
    }

    Collection parsed[MAX_COLLECTIONS];
    int count = 0;

    for (JsonObject item : array) {
        const char* id = item["id"];
        const char* title = item["title"];

        if (!validIdentifier(id) || !title || !title[0]) {
            continue;
        }

        strlcpy(parsed[count].id, id, sizeof(parsed[count].id));
        strlcpy(parsed[count].title, title, sizeof(parsed[count].title));

        count++;
    }

    if (count == 0) return false;

    memcpy(collections, parsed, sizeof(Collection) * count);
    totalCollections = count;

    return true;
}

bool saveCollectionCatalog(const String& json) {
    if (!parseCatalog(json)) return false;

    return writeFileAtomic("/collections.json", json);
}

bool loadCollectionCatalog() {
    String json;

    if (!readTextFile("/collections.json", json)) {
        return false;
    }

    return parseCatalog(json);
}

bool downloadCollectionCatalog() {
    String json;

    if (!downloadText(CATALOG_URL, json)) {
        return false;
    }

    return saveCollectionCatalog(json);
}

int findCollectionIndex(const char* collectionId) {
    for (int i = 0; i < totalCollections; i++) {
        if (strcmp(collections[i].id, collectionId) == 0) {
            return i;
        }
    }

    return -1;
}

bool isCollectionAvailableOffline(const char* collectionId) {
    String path = collectionContentPath(collectionId);
    return LittleFS.exists(path);
}

bool loadSelectedCollection() {
    String json;

    if (!readTextFile("/selected_collection.json", json)) {
        return false;
    }

    DynamicJsonDocument doc(CONFIG_JSON_CAPACITY);

    if (deserializeJson(doc, json)) {
        return false;
    }

    const char* id = doc["activeCollection"];

    if (!validIdentifier(id)) {
        return false;
    }

    strlcpy(activeCollection, id, sizeof(activeCollection));
    return true;
}

bool saveSelectedCollection(const char* collectionId) {
    if (!validIdentifier(collectionId)) return false;

    DynamicJsonDocument doc(CONFIG_JSON_CAPACITY);
    doc["activeCollection"] = collectionId;

    String json;
    serializeJson(doc, json);

    if (!writeFileAtomic("/selected_collection.json", json)) {
        return false;
    }

    strlcpy(activeCollection, collectionId, sizeof(activeCollection));

    return true;
}

bool downloadCollectionContent(const char* collectionId) {
    if (!validIdentifier(collectionId)) return false;

    String url = String(GITHUB_RAW) +
        "collections/" + collectionId + "/content.json";

    String json;

    if (!downloadText(url, json)) {
        return false;
    }

    DynamicJsonDocument doc(CONTENT_JSON_CAPACITY);

    if (deserializeJson(doc, json)) {
        return false;
    }

    JsonArray array = doc["words"].as<JsonArray>();

    if (array.isNull() || array.size() < 2 ||
        array.size() > MAX_WORDS) {
        return false;
    }

    for (JsonObject item : array) {
        const char* id = item["id"];
        const char* spanish = item["spanish"];
        const char* english = item["english"];

        if (!id || !spanish || !english ||
            !id[0] || !spanish[0] || !english[0]) {
            return false;
        }
    }

    String directory = String("/collections/") + collectionId;

    if (!ensureDirectory("/collections")) return false;
    if (!ensureDirectory(directory.c_str())) return false;

    return writeFileAtomic(
        collectionContentPath(collectionId).c_str(),
        json
    );
}

bool loadActiveCollectionFromLocal() {
    if (!activeCollection[0]) return false;

    String path = collectionContentPath(activeCollection);
    String json;

    if (!readTextFile(path.c_str(), json)) {
        return false;
    }

    DynamicJsonDocument doc(CONTENT_JSON_CAPACITY);

    if (deserializeJson(doc, json)) {
        return false;
    }

    JsonArray array = doc["words"].as<JsonArray>();

    if (array.isNull() || array.size() < 2 ||
        array.size() > MAX_WORDS) {
        return false;
    }

    totalWords = 0;

    for (JsonObject item : array) {
        Word& word = words[totalWords];

        memset(&word, 0, sizeof(Word));

        strlcpy(word.id, item["id"] | "", sizeof(word.id));
        strlcpy(word.spanish, item["spanish"] | "", sizeof(word.spanish));
        strlcpy(word.english, item["english"] | "", sizeof(word.english));

        word.hasImage = false;
        word.imageAvailable = false;

        JsonObject image = item["image"];

        if (!image.isNull()) {
            strlcpy(
                word.imageFile,
                image["file"] | "",
                sizeof(word.imageFile)
            );

            strlcpy(
                word.imageHash,
                image["sha256"] | "",
                sizeof(word.imageHash)
            );

            word.hasImage = word.imageFile[0] != '\0';
            word.imageAvailable = image["available"] | false;
        }

        totalWords++;
    }

    int index = findCollectionIndex(activeCollection);

    if (index >= 0) {
        selectedCollectionIndex = index;
        strlcpy(
            collectionTitle,
            collections[index].title,
            sizeof(collectionTitle)
        );
    } else {
        strlcpy(collectionTitle, activeCollection, sizeof(collectionTitle));
    }

    collectionLoaded = true;
    return true;
}

bool selectCollection(int index) {
    if (index < 0 || index >= totalCollections) return false;

    const char* id = collections[index].id;

    if (WiFi.status() == WL_CONNECTED) {
        downloadCollectionContent(id);
    }

    if (!isCollectionAvailableOffline(id)) {
        return false;
    }

    if (!saveSelectedCollection(id)) {
        return false;
    }

    selectedCollectionIndex = index;

    if (!loadActiveCollectionFromLocal()) {
        return false;
    }

    return true;
}

bool nextCollection() {
    if (totalCollections == 0) return false;

    int next = (selectedCollectionIndex + 1) % totalCollections;

    while (next != selectedCollectionIndex &&
           !isCollectionAvailableOffline(collections[next].id)) {
        next = (next + 1) % totalCollections;
    }

    if (!isCollectionAvailableOffline(collections[next].id)) {
        return false;
    }

    selectedCollectionIndex = next;
    return true;
}

bool previousCollection() {
    if (totalCollections == 0) return false;

    int previous = selectedCollectionIndex - 1;

    if (previous < 0) previous = totalCollections - 1;

    int start = previous;

    while (!isCollectionAvailableOffline(collections[previous].id)) {
        previous--;

        if (previous < 0) previous = totalCollections - 1;
        if (previous == start) return false;
    }

    selectedCollectionIndex = previous;
    return true;
}