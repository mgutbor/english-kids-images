
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>
#include <string.h>

// Credenciales Wi-Fi
#include "secrets.h"

// =====================================
// CONFIGURACION HARDWARE
// =====================================

#define TFT_CS    4
#define TFT_RST   1
#define TFT_DC    2
#define TFT_MOSI  11
#define TFT_SCK   12
#define TFT_MISO  13

#define BUTTON_RED     6
#define BUTTON_YELLOW  7

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

// =====================================
// COLORES
// =====================================

#define DARK_BLUE  0x192F
#define LIGHT_BG   0xEF7D
#define GREEN      0x2666

// =====================================
// CONFIGURACION GENERAL
// =====================================

const char* GITHUB_RAW =
    "https://raw.githubusercontent.com/mgutbor/english-kids-images/main/";

const char* CONFIG_URL =
    "https://raw.githubusercontent.com/mgutbor/english-kids-images/main/config.json";

#define MAX_WORDS 30

#define MAX_COLLECTION_ID 48
#define MAX_WORD_ID       40
#define MAX_WORD_TEXT     32
#define MAX_IMAGE_FILE    64

#define IMAGE_WIDTH  120
#define IMAGE_HEIGHT 120

const size_t IMAGE_BYTES =
    IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(uint16_t);

const size_t CONTENT_JSON_CAPACITY = 16384;
const size_t CONFIG_JSON_CAPACITY = 1024;

bool wifiConnected = false;
bool littleFSReady = false;
bool collectionLoaded = false;

char activeCollection[MAX_COLLECTION_ID] = "";
char collectionTitle[64] = "";

unsigned long bothButtonsPressedAt = 0;
bool updateCombinationHandled = false;

// =====================================
// VOCABULARIO DINAMICO
// =====================================

struct Word {
    char id[MAX_WORD_ID];
    char spanish[MAX_WORD_TEXT];
    char english[MAX_WORD_TEXT];

    char imageFile[MAX_IMAGE_FILE];
    char imageHash[65];

    bool hasImage;
    bool imageAvailable;
};

Word words[MAX_WORDS];

int TOTAL_WORDS = 0;

int questionOrder[MAX_WORDS];

int currentQuestion = 0;
int score = 0;

int leftOption = 0;
int rightOption = 0;

bool leftIsCorrect = false;
bool gameFinished = false;

bool lastRedState = HIGH;
bool lastYellowState = HIGH;

unsigned long lastPress = 0;
const unsigned long debounceDelay = 180;

// =====================================
// BUFFER DE IMAGEN
// =====================================

uint16_t imageBuffer[IMAGE_WIDTH * IMAGE_HEIGHT];

// =====================================
// PROTOTIPOS
// =====================================

void drawEndScreen();
void drawQuestion();
void startGame();

bool loadActiveCollectionFromLocal();
bool updateCollectionFromGitHub();
void updateImagesForActiveCollection();

void connectWiFi();
void initLittleFS();

// =====================================
// UTILIDADES
// =====================================

bool isValidIdentifier(const char* value) {

    if (!value || !value[0]) {
        return false;
    }

    for (int i = 0; value[i]; i++) {

        char c = value[i];

        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '-' ||
              c == '_')) {

            return false;
        }
    }

    return true;
}

bool isValidImageFilename(const char* filename) {

    if (!filename || !filename[0]) {
        return false;
    }

    if (strchr(filename, '/') ||
        strchr(filename, '\\')) {

        return false;
    }

    size_t length = strlen(filename);

    if (length < 7 || length >= MAX_IMAGE_FILE) {
        return false;
    }

    return strcmp(
        filename + length - 7,
        ".rgb565"
    ) == 0;
}

bool isValidSHA256(const char* hash) {

    if (!hash || strlen(hash) != 64) {
        return false;
    }

    for (int i = 0; i < 64; i++) {

        char c = hash[i];

        if (!((c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'f'))) {

            return false;
        }
    }

    return true;
}

bool copyJsonString(
    JsonVariantConst value,
    char* destination,
    size_t destinationSize
) {

    if (value.isNull()) {
        return false;
    }

    const char* source = value.as<const char*>();

    if (!source || !source[0]) {
        return false;
    }

    size_t length = strlen(source);

    if (length >= destinationSize) {
        return false;
    }

    memcpy(destination, source, length + 1);

    return true;
}

String collectionDirectory() {

    return "/collections/" + String(activeCollection);
}

String collectionContentPath() {

    return collectionDirectory() + "/content.json";
}

String collectionImagesDirectory() {

    return collectionDirectory() + "/images";
}

String getWordImagePath(int index) {

    return collectionImagesDirectory() + "/" +
           String(words[index].imageFile);
}

// =====================================
// CREAR DIRECTORIOS
// =====================================

bool ensureDirectory(const String& path) {

    if (LittleFS.exists(path)) {
        return true;
    }

    String current = "";

    for (int i = 0; i < path.length(); i++) {

        if (path[i] == '/') {

            if (current.length() > 0 &&
                !LittleFS.exists(current)) {

                if (!LittleFS.mkdir(current)) {
                    return false;
                }
            }
        }

        current += path[i];
    }

    if (!LittleFS.exists(path)) {
        return LittleFS.mkdir(path);
    }

    return true;
}

// =====================================
// RECUPERACION DE ARCHIVOS ATOMICOS
// =====================================

void recoverAtomicFile(const String& target) {

    String backup = target + ".bak";
    String temp = target + ".tmp";

    if (!LittleFS.exists(target) &&
        LittleFS.exists(backup)) {

        LittleFS.rename(backup, target);

    } else if (LittleFS.exists(target) &&
               LittleFS.exists(backup)) {

        LittleFS.remove(backup);
    }

    if (LittleFS.exists(temp)) {
        LittleFS.remove(temp);
    }
}

// =====================================
// ESCRITURA ATOMICA
// =====================================

bool writeFileAtomic(
    const String& target,
    const String& content
) {

    String temp = target + ".tmp";
    String backup = target + ".bak";

    int separator = target.lastIndexOf('/');

    if (separator > 0) {

        String directory = target.substring(0, separator);

        if (!ensureDirectory(directory)) {
            return false;
        }
    }

    LittleFS.remove(temp);

    File file = LittleFS.open(temp, "w");

    if (!file) {
        return false;
    }

    size_t written = file.print(content);

    file.flush();
    file.close();

    if (written != content.length()) {

        LittleFS.remove(temp);
        return false;
    }

    LittleFS.remove(backup);

    if (LittleFS.exists(target)) {

        if (!LittleFS.rename(target, backup)) {

            LittleFS.remove(temp);
            return false;
        }
    }

    if (!LittleFS.rename(temp, target)) {

        if (LittleFS.exists(backup)) {
            LittleFS.rename(backup, target);
        }

        return false;
    }

    LittleFS.remove(backup);

    return true;
}

// =====================================
// WIFI
// =====================================

void connectWiFi() {

    Serial.println();
    Serial.println("Conectando a Wi-Fi...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long startAttempt = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - startAttempt < 15000) {

        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {

        wifiConnected = true;

        Serial.println();
        Serial.println("Wi-Fi conectado");

        Serial.print("Direccion IP: ");
        Serial.println(WiFi.localIP());

    } else {

        wifiConnected = false;

        Serial.println();
        Serial.println("No se pudo conectar a Wi-Fi");
        Serial.println("Continuamos en modo offline");

        WiFi.disconnect();
    }
}

// =====================================
// LITTLEFS
// =====================================

void initLittleFS() {

    Serial.println("\n===== INICIALIZANDO LITTLEFS =====");

    // No formateamos automaticamente:
    // un formateo podria borrar la ultima
    // coleccion valida almacenada.

    littleFSReady = LittleFS.begin(false);

    if (littleFSReady) {

        Serial.println("LittleFS montado correctamente.");

        Serial.printf(
            "Total: %u bytes\n",
            LittleFS.totalBytes()
        );

        Serial.printf(
            "Usado: %u bytes\n",
            LittleFS.usedBytes()
        );

        recoverAtomicFile("/config.json");

    } else {

        Serial.println("ERROR: LittleFS no disponible.");
    }
}

// =====================================
// DESCARGAR TEXTO DESDE GITHUB
// =====================================


bool downloadText(
    const String& url,
    String& payload
) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(30000);

    HTTPClient http;
    http.setTimeout(30000);

    http.setFollowRedirects(
        HTTPC_STRICT_FOLLOW_REDIRECTS
    );

    // Evitar respuestas almacenadas en caché.
    String requestUrl = url;
    requestUrl += (url.indexOf('?') >= 0) ? "&" : "?";
    requestUrl += "_ts=" + String(millis());

    Serial.println();
    Serial.println("===== DESCARGA HTTP =====");
    Serial.print("URL: ");
    Serial.println(requestUrl);

    if (!http.begin(client, requestUrl)) {
        Serial.println("No se pudo iniciar HTTP");
        return false;
    }

    http.addHeader("Cache-Control", "no-cache, no-store");
    http.addHeader("Pragma", "no-cache");

    int httpCode = http.GET();

    Serial.printf("HTTP: %d\n", httpCode);

    if (httpCode != HTTP_CODE_OK) {
        Serial.println("Error en la descarga");
        http.end();
        return false;
    }

    payload = http.getString();

    Serial.printf(
        "Bytes recibidos: %u\n",
        (unsigned int)payload.length()
    );

    Serial.println("Contenido recibido:");
    Serial.println(payload);
    Serial.println("========================");

    http.end();

    return payload.length() > 0;
}

// =====================================
// VALIDAR CONFIG.JSON
// =====================================

bool parseConfig(
    const String& payload,
    char* collectionId,
    size_t collectionIdSize
) {

    DynamicJsonDocument doc(CONFIG_JSON_CAPACITY);

    DeserializationError error =
        deserializeJson(doc, payload);

    if (error) {

        Serial.print("Error en config.json: ");
        Serial.println(error.c_str());

        return false;
    }

    if (!doc["activeCollection"].is<const char*>()) {
        return false;
    }

    if (!copyJsonString(
            doc["activeCollection"],
            collectionId,
            collectionIdSize
        )) {

        return false;
    }

    if (!isValidIdentifier(collectionId)) {
        return false;
    }

    return true;
}

// =====================================
// VALIDAR CONTENT.JSON
// =====================================

bool validateContentJson(
    const String& payload,
    String& errorMessage
) {

    DynamicJsonDocument doc(CONTENT_JSON_CAPACITY);

    DeserializationError error =
        deserializeJson(doc, payload);

    if (error) {

        errorMessage = String("JSON invalido: ") +
                       error.c_str();

        return false;
    }

    if (!doc["words"].is<JsonArray>()) {

        errorMessage = "No existe una lista words valida";

        return false;
    }

    JsonArray wordsArray = doc["words"].as<JsonArray>();

    if (wordsArray.size() < 2) {

        errorMessage = "La coleccion necesita al menos dos palabras";

        return false;
    }

    if (wordsArray.size() > MAX_WORDS) {

        errorMessage = "La coleccion supera el maximo de palabras";

        return false;
    }

    for (JsonObject word : wordsArray) {

        const char* id = word["id"] | "";
        const char* spanish = word["spanish"] | "";
        const char* english = word["english"] | "";

        if (!isValidIdentifier(id) ||
            !spanish[0] ||
            !english[0] ||
            strlen(id) >= MAX_WORD_ID ||
            strlen(spanish) >= MAX_WORD_TEXT ||
            strlen(english) >= MAX_WORD_TEXT) {

            errorMessage = "Una palabra tiene campos invalidos";

            return false;
        }
    }

    errorMessage = "";

    return true;
}

// =====================================
// DESCARGAR CONFIG Y COLECCION
// =====================================

bool updateCollectionFromGitHub() {

    if (!wifiConnected ||
        WiFi.status() != WL_CONNECTED) {

        Serial.println("Sin Wi-Fi. No se buscan actualizaciones.");

        return false;
    }

    if (!littleFSReady) {

        Serial.println("LittleFS no disponible");

        return false;
    }

    Serial.println();
    Serial.println("===== ACTUALIZANDO COLECCION =====");

    // ---------------------------------
    // 1. Descargar config.json
    // ---------------------------------

    String configPayload;

    if (!downloadText(CONFIG_URL, configPayload)) {

        Serial.println("No se pudo descargar config.json");

        return false;
    }

    char remoteCollection[MAX_COLLECTION_ID] = "";

    if (!parseConfig(
            configPayload,
            remoteCollection,
            sizeof(remoteCollection)
        )) {

        Serial.println("config.json no valido");

        return false;
    }

    Serial.print("Coleccion publicada: ");
    Serial.println(remoteCollection);

    // ---------------------------------
    // 2. Descargar content.json
    // ---------------------------------

    String contentUrl =
        String(GITHUB_RAW) +
        "collections/" +
        remoteCollection +
        "/content.json";

    String contentPayload;

    if (!downloadText(contentUrl, contentPayload)) {

        Serial.println("No se pudo descargar content.json");

        return false;
    }

    // ---------------------------------
    // 3. Validar contenido
    // ---------------------------------

    String validationError;

    if (!validateContentJson(
            contentPayload,
            validationError
        )) {

        Serial.print("Coleccion rechazada: ");
        Serial.println(validationError);

        return false;
    }

    // ---------------------------------
    // 4. Guardar content.json
    // ---------------------------------

    String contentPath =
        "/collections/" +
        String(remoteCollection) +
        "/content.json";

    Serial.println("Guardando contenido validado...");

    if (!writeFileAtomic(
            contentPath,
            contentPayload
        )) {

        Serial.println("Error guardando content.json");

        return false;
    }

    // ---------------------------------
    // 5. Guardar config.json
    // ---------------------------------

    Serial.println("Activando nueva configuracion...");

    if (!writeFileAtomic(
            "/config.json",
            configPayload
        )) {

        Serial.println("Error guardando config.json");

        return false;
    }

    Serial.println("Coleccion actualizada correctamente");

    return true;
}

// =====================================
// CARGAR COLECCION DESDE LITTLEFS
// =====================================

bool loadActiveCollectionFromLocal() {

    if (!littleFSReady) {
        return false;
    }

    Serial.println();
    Serial.println("===== CARGANDO COLECCION LOCAL =====");

    recoverAtomicFile("/config.json");

    File configFile = LittleFS.open("/config.json", "r");

    if (!configFile) {

        Serial.println("No existe config.json local");

        return false;
    }

    String configPayload = configFile.readString();

    configFile.close();

    char collectionId[MAX_COLLECTION_ID] = "";

    if (!parseConfig(
            configPayload,
            collectionId,
            sizeof(collectionId)
        )) {

        Serial.println("Configuracion local no valida");

        return false;
    }

    String contentPath =
        "/collections/" +
        String(collectionId) +
        "/content.json";

    recoverAtomicFile(contentPath);

    File contentFile = LittleFS.open(contentPath, "r");

    if (!contentFile) {

        Serial.println("No existe content.json local");

        return false;
    }

    DynamicJsonDocument doc(CONTENT_JSON_CAPACITY);

    DeserializationError error =
        deserializeJson(doc, contentFile);

    contentFile.close();

    if (error) {

        Serial.print("Error leyendo coleccion local: ");
        Serial.println(error.c_str());

        return false;
    }

    JsonArray wordsArray = doc["words"].as<JsonArray>();

    if (wordsArray.isNull() ||
        wordsArray.size() < 2 ||
        wordsArray.size() > MAX_WORDS) {

        Serial.println("Numero de palabras no valido");

        return false;
    }

    // ---------------------------------
    // Construir vocabulario en memoria
    // ---------------------------------

    TOTAL_WORDS = 0;

    memset(words, 0, sizeof(words));

    for (JsonObject item : wordsArray) {

        Word& word = words[TOTAL_WORDS];

        if (!copyJsonString(
                item["id"],
                word.id,
                sizeof(word.id)
            ) ||
            !copyJsonString(
                item["spanish"],
                word.spanish,
                sizeof(word.spanish)
            ) ||
            !copyJsonString(
                item["english"],
                word.english,
                sizeof(word.english)
            )) {

            Serial.println("Palabra invalida en coleccion local");

            TOTAL_WORDS = 0;

            return false;
        }

        JsonObject image = item["image"].as<JsonObject>();

        word.hasImage = false;
        word.imageAvailable = false;

        if (!image.isNull()) {

            const char* filename = image["file"] | "";
            const char* hash = image["sha256"] | "";

            bool available = image["available"] | false;
            size_t size = image["size"] | 0;

            if (available &&
                isValidImageFilename(filename) &&
                isValidSHA256(hash) &&
                size == IMAGE_BYTES) {

                snprintf(
                    word.imageFile,
                    sizeof(word.imageFile),
                    "%s",
                    filename
                );

                snprintf(
                    word.imageHash,
                    sizeof(word.imageHash),
                    "%s",
                    hash
                );

                word.hasImage = true;
            }
        }

        TOTAL_WORDS++;
    }

    snprintf(
        activeCollection,
        sizeof(activeCollection),
        "%s",
        collectionId
    );

    const char* title = doc["title"] | "English Kids";

    snprintf(
        collectionTitle,
        sizeof(collectionTitle),
        "%s",
        title
    );

    collectionLoaded = true;

    Serial.println("Coleccion cargada correctamente");

    Serial.print("Identificador: ");
    Serial.println(activeCollection);

    Serial.print("Titulo: ");
    Serial.println(collectionTitle);

    Serial.print("Palabras: ");
    Serial.println(TOTAL_WORDS);

    return true;
}

// =====================================
// CALCULAR SHA-256 DE ARCHIVO
// =====================================

bool calculateFileSHA256(
    const char* path,
    char* output,
    size_t outputSize
) {

    if (outputSize < 65) {
        return false;
    }

    File file = LittleFS.open(path, "r");

    if (!file) {
        return false;
    }

    mbedtls_sha256_context ctx;

    mbedtls_sha256_init(&ctx);

    bool success =
        mbedtls_sha256_starts(&ctx, 0) == 0;

    uint8_t buffer[1024];

    while (success && file.available()) {

        size_t bytesRead = file.read(
            buffer,
            sizeof(buffer)
        );

        if (bytesRead == 0) {

            success = false;
            break;
        }

        success =
            mbedtls_sha256_update(
                &ctx,
                buffer,
                bytesRead
            ) == 0;
    }

    uint8_t hash[32] = {0};

    if (success) {

        success =
            mbedtls_sha256_finish(
                &ctx,
                hash
            ) == 0;
    }

    mbedtls_sha256_free(&ctx);

    file.close();

    if (!success) {
        return false;
    }

    for (int i = 0; i < 32; i++) {

        sprintf(
            output + (i * 2),
            "%02x",
            hash[i]
        );
    }

    output[64] = '\0';

    return true;
}

// =====================================
// COMPROBAR IMAGEN LOCAL
// =====================================

bool localImageIsValid(int index) {

    if (!words[index].hasImage) {
        return false;
    }

    String path = getWordImagePath(index);

    File file = LittleFS.open(path, "r");

    if (!file) {
        return false;
    }

    bool validSize = file.size() == IMAGE_BYTES;

    file.close();

    if (!validSize) {
        return false;
    }

    char localHash[65];

    if (!calculateFileSHA256(
            path.c_str(),
            localHash,
            sizeof(localHash)
        )) {

        return false;
    }

    return strcmp(
        localHash,
        words[index].imageHash
    ) == 0;
}

// =====================================
// DESCARGAR UNA IMAGEN
// =====================================

bool downloadImage(int index) {

    if (!words[index].hasImage) {
        return false;
    }

    String filename = words[index].imageFile;

    String relativePath =
        "collections/" +
        String(activeCollection) +
        "/images/" +
        filename;

    String url = String(GITHUB_RAW) + relativePath;

    String directory = collectionImagesDirectory();

    if (!ensureDirectory(directory)) {
        return false;
    }

    String target = getWordImagePath(index);
    String temp = target + ".tmp";
    String backup = target + ".bak";

    Serial.println();
    Serial.print("Descargando imagen: ");
    Serial.println(filename);

    WiFiClientSecure client;

    client.setInsecure();
    client.setTimeout(30000);

    HTTPClient http;

    http.setTimeout(30000);

    http.setFollowRedirects(
        HTTPC_STRICT_FOLLOW_REDIRECTS
    );

    if (!http.begin(client, url)) {
        return false;
    }

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {

        Serial.printf("Error HTTP imagen: %d\n", httpCode);

        http.end();

        return false;
    }

    int contentLength = http.getSize();

    if (contentLength != (int)IMAGE_BYTES) {

        Serial.println("Tamano de imagen incorrecto");

        http.end();

        return false;
    }

    LittleFS.remove(temp);

    File output = LittleFS.open(temp, "w");

    if (!output) {

        http.end();

        return false;
    }

    int downloaded = http.writeToStream(&output);

    output.flush();
    output.close();

    http.end();

    if (downloaded != (int)IMAGE_BYTES) {

        Serial.println("Descarga incompleta");

        LittleFS.remove(temp);

        return false;
    }

    // Verificar hash del temporal.

    char downloadedHash[65];

    if (!calculateFileSHA256(
            temp.c_str(),
            downloadedHash,
            sizeof(downloadedHash)
        )) {

        LittleFS.remove(temp);

        return false;
    }

    if (strcmp(
            downloadedHash,
            words[index].imageHash
        ) != 0) {

        Serial.println("SHA-256 incorrecto");

        LittleFS.remove(temp);

        return false;
    }

    // Sustitucion segura.

    LittleFS.remove(backup);

    if (LittleFS.exists(target)) {

        if (!LittleFS.rename(target, backup)) {

            LittleFS.remove(temp);

            return false;
        }
    }

    if (!LittleFS.rename(temp, target)) {

        if (LittleFS.exists(backup)) {
            LittleFS.rename(backup, target);
        }

        return false;
    }

    LittleFS.remove(backup);

    Serial.println("Imagen instalada correctamente");

    return true;
}

// =====================================
// ACTUALIZAR IMAGENES DE LA COLECCION
// =====================================

void updateImagesForActiveCollection() {

    if (!collectionLoaded || !littleFSReady) {
        return;
    }

    Serial.println();
    Serial.println("===== COMPROBANDO IMAGENES =====");

    for (int i = 0; i < TOTAL_WORDS; i++) {

        words[i].imageAvailable = false;

        if (!words[i].hasImage) {

            Serial.print(words[i].id);
            Serial.println(": sin imagen configurada");

            continue;
        }

        if (localImageIsValid(i)) {

            words[i].imageAvailable = true;

            Serial.print(words[i].id);
            Serial.println(": imagen local valida");

            continue;
        }

        if (!wifiConnected ||
            WiFi.status() != WL_CONNECTED) {

            Serial.print(words[i].id);
            Serial.println(": imagen no disponible offline");

            continue;
        }

        if (downloadImage(i)) {

            words[i].imageAvailable = localImageIsValid(i);
        }

        if (!words[i].imageAvailable) {

            Serial.print(words[i].id);
            Serial.println(": se jugara sin imagen");
        }
    }

    Serial.println("Comprobacion de imagenes finalizada");
}

// =====================================
// CARGAR IMAGEN EN BUFFER
// =====================================

bool loadImage(const char* path) {

    if (!littleFSReady) {
        return false;
    }

    File file = LittleFS.open(path, "r");

    if (!file) {
        return false;
    }

    if (file.size() != IMAGE_BYTES) {

        file.close();

        return false;
    }

    size_t bytesRead = file.read(
        (uint8_t*)imageBuffer,
        IMAGE_BYTES
    );

    file.close();

    return bytesRead == IMAGE_BYTES;
}

// =====================================
// DIBUJAR CABECERA
// =====================================

void drawHeader() {

    tft.fillRect(0, 0, 320, 29, DARK_BLUE);

    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(9, 7);

    tft.print("ENGLISH KIDS");

    tft.setTextSize(1);
    tft.setCursor(265, 10);

    tft.print(score);
    tft.print("/");
    tft.print(TOTAL_WORDS);
}

// =====================================
// DIBUJAR ILUSTRACION
// =====================================

bool drawCharacter(int wordIndex) {

    if (wordIndex < 0 || wordIndex >= TOTAL_WORDS) {
        return false;
    }

    if (!words[wordIndex].imageAvailable) {
        return false;
    }

    String imagePath = getWordImagePath(wordIndex);

    if (!loadImage(imagePath.c_str())) {

        words[wordIndex].imageAvailable = false;

        Serial.print("No se pudo cargar imagen: ");
        Serial.println(imagePath);

        return false;
    }

    tft.drawRGBBitmap(
        100,
        70,
        imageBuffer,
        IMAGE_WIDTH,
        IMAGE_HEIGHT
    );

    return true;
}

// =====================================
// DIBUJAR OPCION
// =====================================

void drawOption(
    int x,
    int y,
    const char* label,
    bool red
) {

    uint16_t bg =
        red ? ILI9341_RED : ILI9341_YELLOW;

    uint16_t fg =
        red ? ILI9341_WHITE : ILI9341_BLACK;

    tft.fillRoundRect(
        x,
        y,
        142,
        42,
        8,
        bg
    );

    // Reducir tamaño si la palabra es larga.

    int textSize = 2;

    if (strlen(label) > 11) {
        textSize = 1;
    }

    tft.setTextSize(textSize);
    tft.setTextColor(fg);

    int textWidth =
        strlen(label) * 6 * textSize;

    int textX = x + (142 - textWidth) / 2;

    int textY = y + (42 - 8 * textSize) / 2;

    tft.setCursor(textX, textY);

    tft.print(label);
}

// =====================================
// MOSTRAR PREGUNTA
// =====================================

void drawQuestion() {

    if (!collectionLoaded || TOTAL_WORDS < 2) {
        return;
    }

    int wordIndex = questionOrder[currentQuestion];

    int wrongIndex;

    do {

        wrongIndex = random(TOTAL_WORDS);

    } while (wrongIndex == wordIndex);

    leftIsCorrect = random(2) == 0;

    if (leftIsCorrect) {

        leftOption = wordIndex;
        rightOption = wrongIndex;

    } else {

        leftOption = wrongIndex;
        rightOption = wordIndex;
    }

    tft.fillScreen(LIGHT_BG);

    drawHeader();

    tft.setTextColor(DARK_BLUE);
    tft.setTextSize(1);
    tft.setCursor(10, 35);

    tft.print("PREGUNTA ");
    tft.print(currentQuestion + 1);
    tft.print(" DE ");
    tft.print(TOTAL_WORDS);

    // Palabra en espanol.

    tft.setTextColor(ILI9341_BLACK);
    tft.setTextSize(2);

    int wordWidth =
        strlen(words[wordIndex].spanish) * 12;

    tft.setCursor(
        (320 - wordWidth) / 2,
        49
    );

    tft.print(words[wordIndex].spanish);

    // La imagen es opcional.
    // Si no existe, las opciones suben.

    bool hasImage = drawCharacter(wordIndex);

    int optionsY = hasImage ? 193 : 125;

    drawOption(
        10,
        optionsY,
        words[leftOption].english,
        true
    );

    drawOption(
        168,
        optionsY,
        words[rightOption].english,
        false
    );
}

// =====================================
// CREAR PARTIDA
// =====================================

void startGame() {

    if (!collectionLoaded || TOTAL_WORDS < 2) {
        return;
    }

    score = 0;
    currentQuestion = 0;
    gameFinished = false;

    for (int i = 0; i < TOTAL_WORDS; i++) {
        questionOrder[i] = i;
    }

    // Barajar sin repetir palabras.

    for (int i = TOTAL_WORDS - 1; i > 0; i--) {

        int j = random(i + 1);

        int temp = questionOrder[i];

        questionOrder[i] = questionOrder[j];
        questionOrder[j] = temp;
    }

    drawQuestion();
}

// =====================================
// PREPARAR SIGUIENTE PREGUNTA
// =====================================

void nextQuestion() {

    currentQuestion++;

    if (currentQuestion >= TOTAL_WORDS) {

        gameFinished = true;

        drawEndScreen();

        return;
    }

    drawQuestion();
}

// =====================================
// COMPROBAR RESPUESTA
// =====================================

void checkAnswer(bool selectedLeft) {

    if (gameFinished) {
        return;
    }

    bool correct = (selectedLeft == leftIsCorrect);

    if (correct) {
        score++;
    }

    tft.fillRect(
        0,
        29,
        320,
        19,
        LIGHT_BG
    );

    tft.setTextSize(2);
    tft.setCursor(90, 31);

    if (correct) {

        tft.setTextColor(GREEN);
        tft.print("CORRECTO!");

    } else {

        tft.setTextColor(ILI9341_RED);
        tft.print("INCORRECTO");
    }

    delay(1100);

    nextQuestion();
}

// =====================================
// FINAL DE PARTIDA
// =====================================

void drawEndScreen() {

    tft.fillScreen(LIGHT_BG);

    drawHeader();

    tft.setTextColor(DARK_BLUE);
    tft.setTextSize(3);
    tft.setCursor(62, 55);

    tft.print("FIN DEL JUEGO");

    tft.setTextSize(2);
    tft.setTextColor(ILI9341_BLACK);
    tft.setCursor(75, 110);

    tft.print("RESULTADO");

    tft.setTextSize(4);
    tft.setTextColor(GREEN);
    tft.setCursor(110, 145);

    tft.print(score);
    tft.print("/");
    tft.print(TOTAL_WORDS);

    tft.setTextSize(1);
    tft.setTextColor(DARK_BLUE);
    tft.setCursor(55, 210);

    tft.print("Pulsa cualquier boton para jugar");
}

// =====================================
// PANTALLA DE ERROR
// =====================================

void drawFatalError() {

    tft.fillScreen(LIGHT_BG);

    tft.setTextColor(ILI9341_RED);
    tft.setTextSize(2);
    tft.setCursor(35, 70);

    tft.print("ERROR DE CONTENIDO");

    tft.setTextColor(DARK_BLUE);
    tft.setTextSize(1);
    tft.setCursor(30, 110);

    tft.print("No hay una coleccion valida");

    tft.setCursor(30, 130);
    tft.print("Comprueba Wi-Fi y LittleFS");
}

// =====================================
// SETUP
// =====================================

void setup() {

    Serial.begin(115200);

    delay(1500);

    Serial.println();
    Serial.println("=== ENGLISH KIDS ARRANCANDO ===");

    Serial.printf(
        "Flash: %u MB\n",
        ESP.getFlashChipSize() / (1024 * 1024)
    );

    Serial.printf(
        "PSRAM: %u MB\n",
        ESP.getPsramSize() / (1024 * 1024)
    );

    pinMode(BUTTON_RED, INPUT_PULLUP);
    pinMode(BUTTON_YELLOW, INPUT_PULLUP);

    SPI.begin(
        TFT_SCK,
        TFT_MISO,
        TFT_MOSI,
        TFT_CS
    );

    tft.begin();

    tft.setRotation(1);

    tft.fillScreen(ILI9341_BLACK);

    randomSeed(esp_random());

    // 1. Conectar Wi-Fi.

    connectWiFi();

    // 2. Montar LittleFS.

    initLittleFS();

    if (!littleFSReady) {

        drawFatalError();

        return;
    }

    // 3. Cargar primero la ultima coleccion local.

    bool localLoaded = loadActiveCollectionFromLocal();

    // 4. Intentar actualizar desde GitHub.

    if (wifiConnected) {

        updateCollectionFromGitHub();

        // Volver a cargar la coleccion activa.
        // Si la descarga fallo, seguira siendo
        // la ultima configuracion local valida.

        localLoaded = loadActiveCollectionFromLocal();
    }

    // 5. Comprobar y descargar imagenes.

    if (localLoaded) {

        updateImagesForActiveCollection();

        // Recalcular disponibilidad despues
        // de las descargas.

        for (int i = 0; i < TOTAL_WORDS; i++) {
            words[i].imageAvailable = localImageIsValid(i);
        }

        startGame();

    } else {

        Serial.println("No hay ninguna coleccion valida");

        drawFatalError();
    }

    Serial.println();
    Serial.println("English Kids iniciado");
}

// =====================================
// LOOP
// =====================================

void loop() {

    if (!collectionLoaded) {
        delay(100);
        return;
    }

    bool redState = digitalRead(BUTTON_RED);
    bool yellowState = digitalRead(BUTTON_YELLOW);

    // =================================
    // ACTUALIZACION MANUAL
    // Ambos botones durante 2 segundos.
    // =================================

    if (redState == LOW &&
        yellowState == LOW) {

        if (bothButtonsPressedAt == 0) {
            bothButtonsPressedAt = millis();
        }

        if (!updateCombinationHandled &&
            millis() - bothButtonsPressedAt >= 2000) {

            updateCombinationHandled = true;

            Serial.println();
            Serial.println("Actualizacion manual solicitada");

            bool updated = updateCollectionFromGitHub();

            if (updated) {

                if (loadActiveCollectionFromLocal()) {

                    updateImagesForActiveCollection();

                    for (int i = 0; i < TOTAL_WORDS; i++) {
                        words[i].imageAvailable =
                            localImageIsValid(i);
                    }

                    startGame();
                }

            } else {

                // No se ha activado ninguna coleccion nueva.
                // Se conserva la partida actual.

                if (gameFinished) {
                    drawEndScreen();
                } else {
                    drawQuestion();
                }
            }
        }

        lastRedState = redState;
        lastYellowState = yellowState;

        return;
    }

    bothButtonsPressedAt = 0;
    updateCombinationHandled = false;

    // =================================
    // FUNCIONAMIENTO NORMAL DEL JUEGO
    // =================================

    if (millis() - lastPress >= debounceDelay) {

        if (redState == LOW &&
            lastRedState == HIGH) {

            lastPress = millis();

            if (gameFinished) {
                startGame();
            } else {
                checkAnswer(true);
            }
        }

        else if (yellowState == LOW &&
                 lastYellowState == HIGH) {

            lastPress = millis();

            if (gameFinished) {
                startGame();
            } else {
                checkAnswer(false);
            }
        }
    }

    lastRedState = redState;
    lastYellowState = yellowState;
}
