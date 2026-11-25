#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "config.h"
#include "hardware.h"
#include "storage.h"
#include "collections.h"
#include "images.h"
#include "display.h"
#include "game.h"
#include "input.h"
#include "secrets.h"

const char* GITHUB_RAW =
    "https://raw.githubusercontent.com/mgutbor/english-kids-images/main/";

const char* CONFIG_URL =
    "https://raw.githubusercontent.com/mgutbor/english-kids-images/main/config.json";

const char* CATALOG_URL =
    "https://raw.githubusercontent.com/mgutbor/english-kids-images/main/collections.json";

enum AppState {
    STATE_GAME,
    STATE_END,
    STATE_COLLECTION_SELECTOR,
    STATE_ERROR
};

static AppState appState = STATE_GAME;

static unsigned long lastBothPress = 0;
static bool selectorOpenedByLongPress = false;

static void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Conectando WiFi");

    unsigned long started = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - started < 15000) {
        Serial.print(".");
        delay(300);
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WiFi conectado: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("Sin conexión WiFi");
    }
}

static bool loadDefaultCollection() {
    String json;

    if (!readTextFile("/config.json", json)) {
        return false;
    }

    DynamicJsonDocument doc(CONFIG_JSON_CAPACITY);

    if (deserializeJson(doc, json)) {
        return false;
    }

    const char* id = doc["activeCollection"];

    if (!id || !id[0]) return false;

    return saveSelectedCollection(id);
}

static bool initializeCollections() {
    // Intentar actualizar el catálogo remoto.
    if (WiFi.status() == WL_CONNECTED) {
        downloadCollectionCatalog();
    }

    // Si no hay catálogo local, no podemos mostrar el selector.
    if (!loadCollectionCatalog()) {
        return false;
    }

    // La selección local tiene prioridad sobre config.json.
    if (!loadSelectedCollection()) {
        if (!loadDefaultCollection()) {
            if (!saveSelectedCollection(collections[0].id)) {
                return false;
            }
        }
    }

    // Si la selección ya no existe en el catálogo,
    // elegir la primera colección disponible.
    int index = findCollectionIndex(activeCollection);

    if (index < 0) {
        index = 0;

        if (!saveSelectedCollection(collections[index].id)) {
            return false;
        }
    }

    selectedCollectionIndex = index;

    // Actualizar contenido de la colección seleccionada.
    if (WiFi.status() == WL_CONNECTED) {
        downloadCollectionContent(activeCollection);
    }

    if (!loadActiveCollectionFromLocal()) {
        return false;
    }

    return true;
}

static void openCollectionSelector() {
    appState = STATE_COLLECTION_SELECTOR;

    drawCollectionSelector(
        collections[selectedCollectionIndex].title,
        selectedCollectionIndex,
        totalCollections
    );

    resetInput();
}

static void confirmCollection() {
    int index = selectedCollectionIndex;

    if (!selectCollection(index)) {
        drawFatalError("No se pudo cargar la coleccion");
        appState = STATE_ERROR;
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        updateImagesForActiveCollection();
    }

    startGame();
    appState = STATE_GAME;
    resetInput();
}

static void handleGameInput(InputAction action) {
    switch (action) {
        case INPUT_BOTH_LONG:
            openCollectionSelector();
            break;

        case INPUT_RED:
            if (gameFinished) {
                startGame();
                appState = STATE_GAME;
            } else {
                checkAnswer(true);
                if (gameFinished) appState = STATE_END;
            }
            break;

        case INPUT_YELLOW:
            if (gameFinished) {
                openCollectionSelector();
            } else {
                checkAnswer(false);
                if (gameFinished) appState = STATE_END;
            }
            break;

        default:
            break;
    }
}

static void handleEndInput(InputAction action) {
    switch (action) {
        case INPUT_RED:
            startGame();
            appState = STATE_GAME;
            break;

        case INPUT_YELLOW:
            openCollectionSelector();
            break;

        case INPUT_BOTH_LONG:
            openCollectionSelector();
            break;

        default:
            break;
    }
}

static void handleSelectorInput(InputAction action) {
    switch (action) {
        case INPUT_RED:
            previousCollection();

            drawCollectionSelector(
                collections[selectedCollectionIndex].title,
                selectedCollectionIndex,
                totalCollections
            );
            break;

        case INPUT_YELLOW:
            nextCollection();

            drawCollectionSelector(
                collections[selectedCollectionIndex].title,
                selectedCollectionIndex,
                totalCollections
            );
            break;

        case INPUT_BOTH_SHORT:
            confirmCollection();
            break;

        case INPUT_BOTH_LONG:
            confirmCollection();
            break;

        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(300);

    initHardware();
    initInput();

    randomSeed(esp_random());

    tft.fillScreen(ILI9341_BLACK);
    drawHeader("ENGLISH KIDS");

    connectWiFi();

    if (!initStorage()) {
        drawFatalError("Error al montar LittleFS");
        appState = STATE_ERROR;
        return;
    }

    if (!initializeCollections()) {
        drawFatalError("No se pudo cargar ninguna coleccion");
        appState = STATE_ERROR;
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        updateImagesForActiveCollection();
    }

    startGame();
    appState = STATE_GAME;
}

void loop() {
    if (appState == STATE_ERROR) {
        delay(1000);
        return;
    }

    InputAction action = readInput();

    switch (appState) {
        case STATE_GAME:
            handleGameInput(action);
            break;

        case STATE_END:
            handleEndInput(action);
            break;

        case STATE_COLLECTION_SELECTOR:
            handleSelectorInput(action);
            break;

        default:
            break;
    }
}