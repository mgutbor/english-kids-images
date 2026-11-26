
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
    STATE_WAIT_RELEASE,
    STATE_ERROR
};

static AppState appState = STATE_GAME;

static unsigned long buttonsReleasedAt = 0;

// =====================================================
// WIFI
// =====================================================

static void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.println("WiFi: conectando");

    unsigned long started = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - started < 15000) {
        delay(300);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WiFi: conectado ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("WiFi: sin conexión");
    }
}

// =====================================================
// CONFIGURACIÓN
// =====================================================

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

// =====================================================
// COLECCIONES
// =====================================================

static bool initializeCollections() {
    if (WiFi.status() == WL_CONNECTED) {
        downloadCollectionCatalog();
    }

    if (!loadCollectionCatalog()) {
        return false;
    }

    if (!loadSelectedCollection()) {
        if (!loadDefaultCollection()) {
            if (!saveSelectedCollection(collections[0].id)) {
                return false;
            }
        }
    }

    int index = findCollectionIndex(activeCollection);

    if (index < 0) {
        index = 0;

        if (!saveSelectedCollection(collections[index].id)) {
            return false;
        }
    }

    selectedCollectionIndex = index;

    if (WiFi.status() == WL_CONNECTED) {
        downloadCollectionContent(activeCollection);
    }

    if (!loadActiveCollectionFromLocal()) {
        return false;
    }

    Serial.printf(
        "Colección activa: %s (%d palabras)\n",
        activeCollection,
        totalWords
    );

    return true;
}

// =====================================================
// SELECTOR
// =====================================================

static void openCollectionSelector() {
    appState = STATE_COLLECTION_SELECTOR;

    drawCollectionSelector(
        collections[selectedCollectionIndex].title,
        selectedCollectionIndex,
        totalCollections
    );

    resetInput();
}

// =====================================================
// CONFIRMAR COLECCIÓN
// =====================================================

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

    // No iniciar todavía la partida.
    // Primero esperamos a que se suelten los botones.
    buttonsReleasedAt = 0;
    appState = STATE_WAIT_RELEASE;
}

// =====================================================
// ESPERAR LIBERACIÓN DE BOTONES
// =====================================================

static void handleWaitRelease() {

    // Si algún botón sigue pulsado, reiniciar espera.
    if (redPressed() || yellowPressed()) {
        buttonsReleasedAt = 0;
        return;
    }

    // Empezar a contar cuando ambos estén sueltos.
    if (buttonsReleasedAt == 0) {
        buttonsReleasedAt = millis();
        return;
    }

    // Esperar 300 ms adicionales.
    if (millis() - buttonsReleasedAt >= 300) {

        resetInput();

        startGame();

        appState = STATE_GAME;
        buttonsReleasedAt = 0;
    }
}

// =====================================================
// ENTRADAS DURANTE LA PARTIDA
// =====================================================

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

                if (gameFinished) {
                    appState = STATE_END;
                }
            }
            break;

        case INPUT_YELLOW:
            if (gameFinished) {
                openCollectionSelector();
            } else {
                checkAnswer(false);

                if (gameFinished) {
                    appState = STATE_END;
                }
            }
            break;

        default:
            break;
    }
}

// =====================================================
// ENTRADAS EN PANTALLA FINAL
// =====================================================

static void handleEndInput(InputAction action) {
    switch (action) {

        case INPUT_RED:
            startGame();
            appState = STATE_GAME;
            break;

        case INPUT_YELLOW:
        case INPUT_BOTH_LONG:
            openCollectionSelector();
            break;

        default:
            break;
    }
}

// =====================================================
// ENTRADAS EN SELECTOR
// =====================================================

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

        case INPUT_RED_LONG:
            confirmCollection();
            break;

        default:
            break;
    }
}

// =====================================================
// SETUP
// =====================================================

void setup() {
    Serial.begin(115200);
    delay(300);

    Serial.println("English Kids: inicio");

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

    // Al arrancar, mostrar siempre el selector.
    // Se conserva la última colección seleccionada.
    openCollectionSelector();
}

// =====================================================
// LOOP
// =====================================================

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

        case STATE_WAIT_RELEASE:
            handleWaitRelease();
            break;

        default:
            break;
    }
}
