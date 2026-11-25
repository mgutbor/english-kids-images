
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

// Imagenes originales de respaldo
#include "ugly.h"
#include "beautiful.h"
#include "happy.h"
#include "sad.h"
#include "old.h"
#include "young.h"

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
#define ORANGE     0xFD20

// =====================================
// WIFI
// =====================================

bool wifiConnected = false;
bool littleFSReady = false;

// =====================================
// ACTUALIZACIONES GITHUB
// =====================================

const char* MANIFEST_URL =
    "https://raw.githubusercontent.com/mgutbor/english-kids-images/main/version.json";

const char* GITHUB_RAW =
    "https://raw.githubusercontent.com/mgutbor/english-kids-images/main/";

const char* IMAGE_FILES[] = {
    "ugly.rgb565",
    "beautiful.rgb565",
    "happy.rgb565",
    "sad.rgb565",
    "old.rgb565",
    "young.rgb565"
};

const int TOTAL_IMAGES = 6;

unsigned long bothButtonsPressedAt = 0;
bool updateCombinationHandled = false;

// =====================================
// IMAGENES
// =====================================

#define IMAGE_WIDTH  120
#define IMAGE_HEIGHT 120

const size_t IMAGE_BYTES =
    IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(uint16_t);

uint16_t imageBuffer[IMAGE_WIDTH * IMAGE_HEIGHT];

// =====================================
// VOCABULARIO
// =====================================

struct Word {
    const char* spanish;
    const char* english;
    uint8_t illustration;
};

Word words[] = {
    {"FEO",       "UGLY",      0},
    {"GUAPO",     "BEAUTIFUL", 1},
    {"ALEGRE",    "HAPPY",     2},
    {"TRISTE",    "SAD",       3},
    {"VIEJO",     "OLD",       4},
    {"JOVEN",     "YOUNG",     5}
};

const int TOTAL_WORDS = 6;

int questionOrder[TOTAL_WORDS];
int currentQuestion = 0;
int score = 0;

int leftOption;
int rightOption;
bool leftIsCorrect;

bool gameFinished = false;

bool lastRedState = HIGH;
bool lastYellowState = HIGH;

unsigned long lastPress = 0;
const unsigned long debounceDelay = 180;

// =====================================
// PROTOTIPOS
// =====================================

void drawEndScreen();
void drawQuestion();
void updateImagesFromGitHub();

// =====================================
// CONEXION WIFI
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

    littleFSReady = LittleFS.begin(false);

    if (!littleFSReady) {

        Serial.println("LittleFS no monta. Intentando formatear...");

        if (LittleFS.format()) {

            Serial.println("Formateo correcto.");

            littleFSReady = LittleFS.begin(false);

        } else {

            Serial.println("ERROR: No se pudo formatear LittleFS.");
        }
    }

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

    } else {

        Serial.println("ERROR: LittleFS sigue sin estar disponible.");
    }
}

// =====================================
// RECUPERACION DE ARCHIVOS TEMPORALES
// =====================================

void recoverImageFiles() {

    Serial.println("\nComprobando archivos temporales...");

    for (int i = 0; i < TOTAL_IMAGES; i++) {

        String target = "/" + String(IMAGE_FILES[i]);
        String backup = target + ".bak";
        String temp = target + ".tmp";

        // Si no existe la imagen original,
        // recuperamos la copia de seguridad.

        if (!LittleFS.exists(target) &&
            LittleFS.exists(backup)) {

            Serial.println("Recuperando copia de seguridad");

            LittleFS.rename(backup, target);

        } else if (LittleFS.exists(target) &&
                   LittleFS.exists(backup)) {

            LittleFS.remove(backup);
        }

        // Eliminar descargas incompletas.

        if (LittleFS.exists(temp)) {
            LittleFS.remove(temp);
        }
    }
}

// =====================================
// GUARDAR IMAGEN EN LITTLEFS
// =====================================

bool saveImageIfNeeded(
    const char* path,
    const uint16_t* bitmap
) {

    if (LittleFS.exists(path)) {

        File existingFile = LittleFS.open(path, "r");

        if (existingFile) {

            bool validSize = existingFile.size() == IMAGE_BYTES;

            existingFile.close();

            if (validSize) {

                Serial.print("Imagen ya instalada: ");
                Serial.println(path);

                return true;
            }
        }
    }

    Serial.print("Instalando imagen: ");
    Serial.println(path);

    File file = LittleFS.open(path, "w");

    if (!file) {

        Serial.println("Error creando archivo");

        return false;
    }

    size_t written = file.write(
        (const uint8_t*)bitmap,
        IMAGE_BYTES
    );

    file.close();

    if (written != IMAGE_BYTES) {

        Serial.println("Error escribiendo imagen");

        return false;
    }

    Serial.println("Imagen guardada correctamente");

    return true;
}

// =====================================
// INSTALAR LAS SEIS IMAGENES
// =====================================

void installDefaultImages() {

    if (!littleFSReady) {

        Serial.println("LittleFS no disponible");

        return;
    }

    Serial.println();
    Serial.println("Instalando imagenes...");

    saveImageIfNeeded("/ugly.rgb565", ugly);
    saveImageIfNeeded("/beautiful.rgb565", beautiful);
    saveImageIfNeeded("/happy.rgb565", happy);
    saveImageIfNeeded("/sad.rgb565", sad);
    saveImageIfNeeded("/old.rgb565", old);
    saveImageIfNeeded("/young.rgb565", young);

    Serial.println("Instalacion de imagenes finalizada");
}

// =====================================
// CALCULAR SHA-256 DE UN ARCHIVO
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
// COMPROBAR SI UNA IMAGEN NECESITA UPDATE
// =====================================

bool imageNeedsUpdate(
    const char* path,
    const char* expectedHash
) {

    char localHash[65];

    if (!LittleFS.exists(path)) {
        return true;
    }

    if (!calculateFileSHA256(
            path,
            localHash,
            sizeof(localHash)
        )) {

        return true;
    }

    return strcmp(localHash, expectedHash) != 0;
}

// =====================================
// DESCARGAR Y SUSTITUIR UNA IMAGEN
// =====================================



bool downloadImage(
    const char* imagePath,
    const char* expectedHash
) {
    String filename = String(imagePath);
    filename.replace("images/", "");

    String target = "/" + filename;
    String temp = target + ".tmp";
    String backup = target + ".bak";

    String url = String(GITHUB_RAW) + imagePath;

    Serial.println();
    Serial.print("Descargando: ");
    Serial.println(imagePath);

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(30000);

    HTTPClient http;
    http.setTimeout(30000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    if (!http.begin(client, url)) {
        Serial.println("No se pudo iniciar conexion HTTP");
        return false;
    }

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("Error HTTP: %d\n", httpCode);
        http.end();
        return false;
    }

    int contentLength = http.getSize();

    Serial.printf("Tamano esperado: %d bytes\n", contentLength);

    if (contentLength != (int)IMAGE_BYTES) {
        Serial.println("Tamano de descarga incorrecto");
        http.end();
        return false;
    }

    LittleFS.remove(temp);

    File output = LittleFS.open(temp, "w");

    if (!output) {
        Serial.println("No se pudo crear archivo temporal");
        http.end();
        return false;
    }

    // HTTPClient gestiona la lectura del flujo.
    int downloaded = http.writeToStream(&output);

    output.flush();
    output.close();
    http.end();

    Serial.printf(
        "Bytes descargados: %d de %u\n",
        downloaded,
        (unsigned int)IMAGE_BYTES
    );

    if (downloaded != (int)IMAGE_BYTES) {
        Serial.println("Descarga incompleta");
        LittleFS.remove(temp);
        return false;
    }

    // Verificar SHA-256 del archivo descargado.
    char downloadedHash[65];

    if (!calculateFileSHA256(
            temp.c_str(),
            downloadedHash,
            sizeof(downloadedHash)
        )) {
        Serial.println("No se pudo calcular SHA-256");
        LittleFS.remove(temp);
        return false;
    }

    if (strcmp(downloadedHash, expectedHash) != 0) {
        Serial.println("ERROR: SHA-256 incorrecto");
        LittleFS.remove(temp);
        return false;
    }

    Serial.println("Imagen descargada y verificada");

    // Sustitucion con copia de seguridad.
    if (LittleFS.exists(backup)) {
        LittleFS.remove(backup);
    }

    if (LittleFS.exists(target)) {
        if (!LittleFS.rename(target, backup)) {
            LittleFS.remove(temp);
            Serial.println("No se pudo crear copia de seguridad");
            return false;
        }
    }

    if (!LittleFS.rename(temp, target)) {
        if (LittleFS.exists(backup)) {
            LittleFS.rename(backup, target);
        }

        Serial.println("Error sustituyendo imagen");
        return false;
    }

    if (LittleFS.exists(backup)) {
        LittleFS.remove(backup);
    }

    Serial.println("Imagen actualizada correctamente");

    return true;
}


// =====================================
// COMPROBAR ACTUALIZACIONES EN GITHUB
// =====================================


void updateImagesFromGitHub() {

    if (!wifiConnected ||
        WiFi.status() != WL_CONNECTED) {

        Serial.println(
            "Sin Wi-Fi. No se buscan actualizaciones."
        );

        return;
    }

    if (!littleFSReady) {

        Serial.println("LittleFS no disponible");

        return;
    }

    Serial.println();
    Serial.println("===== BUSCANDO ACTUALIZACIONES =====");

    WiFiClientSecure client;

    // Configuracion temporal para pruebas.
    // Pendiente de sustituir por validacion TLS.

    client.setInsecure();
    client.setTimeout(30000);

    HTTPClient http;

    http.setTimeout(30000);

    http.setFollowRedirects(
        HTTPC_STRICT_FOLLOW_REDIRECTS
    );

    Serial.println(
        "Conectando con GitHub para obtener manifest..."
    );

    if (!http.begin(client, MANIFEST_URL)) {

        Serial.println(
            "No se pudo iniciar conexion HTTP con GitHub"
        );

        return;
    }

    Serial.println("Conexion HTTP iniciada");

    Serial.println("Solicitando manifest...");

    int httpCode = http.GET();

    Serial.printf(
        "Respuesta HTTP: %d\n",
        httpCode
    );

    if (httpCode != HTTP_CODE_OK) {

        Serial.printf(
            "Error obteniendo manifiesto: %d\n",
            httpCode
        );

        http.end();

        return;
    }

    Serial.println("Descargando contenido del manifest...");

    String payload = http.getString();

    Serial.printf(
        "Manifest recibido: %u bytes\n",
        (unsigned int)payload.length()
    );

    http.end();

    StaticJsonDocument<2048> doc;

    DeserializationError error =
        deserializeJson(doc, payload);

    if (error) {

        Serial.print("Error leyendo manifiesto: ");
        Serial.println(error.c_str());

        return;
    }

    const char* version =
        doc["version"] | "desconocida";

    Serial.print("Version publicada: ");
    Serial.println(version);

    JsonArray images = doc["images"].as<JsonArray>();

    if (images.isNull() ||
        images.size() != TOTAL_IMAGES) {

        Serial.println(
            "Manifiesto incorrecto: numero de imagenes inesperado"
        );

        return;
    }

    bool allSuccessful = true;
    bool seen[TOTAL_IMAGES] = {false};

    for (JsonObject image : images) {

        const char* filename =
            image["file"] | "";

        const char* path =
            image["path"] | "";

        const char* expectedHash =
            image["sha256"] | "";

        size_t size =
            image["size"] | 0;

        int imageIndex = -1;

        for (int i = 0; i < TOTAL_IMAGES; i++) {

            if (strcmp(filename, IMAGE_FILES[i]) == 0) {

                imageIndex = i;
                break;
            }
        }

        // Validar que la imagen pertenece a la lista permitida.

        if (imageIndex < 0 ||
            seen[imageIndex] ||
            strlen(expectedHash) != 64 ||
            size != IMAGE_BYTES ||
            String(path) != String("images/") + filename) {

            Serial.println(
                "Entrada de manifiesto no valida"
            );

            allSuccessful = false;

            continue;
        }

        seen[imageIndex] = true;

        // Validar caracteres del SHA-256.

        bool validHash = true;

        for (int i = 0; i < 64; i++) {

            char c = expectedHash[i];

            if (!((c >= '0' && c <= '9') ||
                  (c >= 'a' && c <= 'f'))) {

                validHash = false;
                break;
            }
        }

        if (!validHash) {

            Serial.println("Hash no valido");

            allSuccessful = false;

            continue;
        }

        String localPath = "/" + String(filename);

        // =====================================
        // DIAGNOSTICO DE HASHES
        // =====================================

        char localHash[65] = {0};

        bool hashCalculated = calculateFileSHA256(
            localPath.c_str(),
            localHash,
            sizeof(localHash)
        );

        Serial.println();
        Serial.printf("Imagen: %s\n", filename);

        Serial.printf(
            "Hash GitHub: %s\n",
            expectedHash
        );

        if (hashCalculated) {

            Serial.printf(
                "Hash local:  %s\n",
                localHash
            );

        } else {

            Serial.println(
                "Hash local: no disponible"
            );
        }

        // =====================================
        // COMPROBAR SI NECESITA ACTUALIZACION
        // =====================================

        if (hashCalculated &&
            strcmp(localHash, expectedHash) == 0) {

            Serial.println(
                "Resultado: imagen ya actualizada"
            );

            continue;
        }

        Serial.println(
            "Resultado: es necesario descargar"
        );

        // =====================================
        // DESCARGAR IMAGEN
        // =====================================

        if (!downloadImage(path, expectedHash)) {

            Serial.println(
                "ERROR actualizando imagen"
            );

            allSuccessful = false;

        } else {

            Serial.println(
                "Descarga y actualizacion completadas"
            );
        }
    }

    // Comprobar que aparecen las seis imagenes.

    for (int i = 0; i < TOTAL_IMAGES; i++) {

        if (!seen[i]) {

            Serial.print(
                "Falta imagen en manifiesto: "
            );

            Serial.println(IMAGE_FILES[i]);

            allSuccessful = false;
        }
    }

    Serial.println();

    if (allSuccessful) {

        Serial.println(
            "===== ACTUALIZACION COMPLETADA ====="
        );

    } else {

        Serial.println(
            "===== ACTUALIZACION PARCIAL O FALLIDA ====="
        );
    }
}


// =====================================
// CARGAR IMAGEN DESDE LITTLEFS
// =====================================

bool loadImage(const char* path) {

    if (!littleFSReady) {
        return false;
    }

    File file = LittleFS.open(path, "r");

    if (!file) {

        Serial.print("No se encuentra imagen: ");
        Serial.println(path);

        return false;
    }

    if (file.size() != IMAGE_BYTES) {

        Serial.print("Tamano incorrecto: ");
        Serial.println(path);

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
    tft.setCursor(257, 10);

    tft.print(score);
    tft.print("/6");
}

// =====================================
// ILUSTRACIONES
// =====================================

void drawCharacter(int type) {

    const char* imagePath = nullptr;

    switch (type) {

        case 0:
            imagePath = "/ugly.rgb565";
            break;

        case 1:
            imagePath = "/beautiful.rgb565";
            break;

        case 2:
            imagePath = "/happy.rgb565";
            break;

        case 3:
            imagePath = "/sad.rgb565";
            break;

        case 4:
            imagePath = "/old.rgb565";
            break;

        case 5:
            imagePath = "/young.rgb565";
            break;
    }

    if (imagePath != nullptr &&
        loadImage(imagePath)) {

        tft.drawRGBBitmap(
            100,
            70,
            imageBuffer,
            IMAGE_WIDTH,
            IMAGE_HEIGHT
        );

        return;
    }

    // =================================
    // DIBUJO GEOMETRICO DE RESPALDO
    // =================================

    Serial.println("Usando ilustracion geometrica");

    int cx = 160;
    int cy = 130;

    uint16_t skin = 0xFEA0;
    uint16_t hair = 0x39E7;

    tft.fillCircle(cx, cy, 30, 0xDFFB);

    if (type == 4) hair = 0xBDF7;
    if (type == 5) hair = 0x7BEF;
    if (type == 0) hair = 0x2104;

    if (type != 4) {

        tft.fillCircle(cx, cy - 24, 21, hair);

        tft.fillRect(
            cx - 20,
            cy - 25,
            40,
            13,
            skin
        );
    }

    tft.fillCircle(cx, cy, 27, skin);

    tft.fillCircle(cx - 27, cy, 6, skin);
    tft.fillCircle(cx + 27, cy, 6, skin);

    if (type == 0) {

        tft.fillTriangle(
            cx - 24, cy - 17,
            cx - 13, cy - 39,
            cx - 5, cy - 20,
            hair
        );

        tft.fillTriangle(
            cx + 4, cy - 20,
            cx + 20, cy - 39,
            cx + 24, cy - 15,
            hair
        );
    }

    if (type == 1) {

        tft.fillCircle(cx, cy - 25, 20, hair);

        tft.fillRect(
            cx - 19,
            cy - 23,
            38,
            9,
            skin
        );
    }

    int eyeY = cy - 5;

    if (type == 0) {

        tft.fillCircle(
            cx - 10,
            eyeY - 2,
            4,
            ILI9341_BLACK
        );

        tft.fillCircle(
            cx + 11,
            eyeY + 2,
            3,
            ILI9341_BLACK
        );

    } else {

        tft.fillCircle(
            cx - 10,
            eyeY,
            3,
            ILI9341_BLACK
        );

        tft.fillCircle(
            cx + 10,
            eyeY,
            3,
            ILI9341_BLACK
        );
    }

    if (type == 4) {

        tft.drawCircle(cx - 10, eyeY, 8, DARK_BLUE);
        tft.drawCircle(cx + 10, eyeY, 8, DARK_BLUE);

        tft.drawLine(
            cx - 2,
            eyeY,
            cx + 2,
            eyeY,
            DARK_BLUE
        );

        tft.drawLine(
            cx - 20,
            cy + 7,
            cx - 12,
            cy + 9,
            DARK_BLUE
        );

        tft.drawLine(
            cx + 12,
            cy + 9,
            cx + 20,
            cy + 7,
            DARK_BLUE
        );
    }

    if (type == 1 || type == 2 || type == 5) {

        tft.drawLine(
            cx - 9, cy + 12,
            cx - 4, cy + 17,
            ILI9341_RED
        );

        tft.drawLine(
            cx - 4, cy + 17,
            cx + 4, cy + 17,
            ILI9341_RED
        );

        tft.drawLine(
            cx + 4, cy + 17,
            cx + 9, cy + 12,
            ILI9341_RED
        );
    }

    if (type == 3) {

        tft.drawLine(
            cx - 9, cy + 17,
            cx - 4, cy + 12,
            ILI9341_RED
        );

        tft.drawLine(
            cx - 4, cy + 12,
            cx + 4, cy + 12,
            ILI9341_RED
        );

        tft.drawLine(
            cx + 4, cy + 12,
            cx + 9, cy + 17,
            ILI9341_RED
        );
    }

    if (type == 0) {

        tft.drawLine(
            cx - 9, cy + 13,
            cx, cy + 18,
            ILI9341_RED
        );

        tft.drawLine(
            cx, cy + 18,
            cx + 9, cy + 11,
            ILI9341_RED
        );
    }

    if (type == 4) {

        tft.drawLine(
            cx - 7,
            cy + 15,
            cx + 7,
            cy + 15,
            ILI9341_RED
        );
    }

    if (type == 1 || type == 2 || type == 5) {

        tft.fillCircle(cx - 18, cy + 8, 4, 0xF986);
        tft.fillCircle(cx + 18, cy + 8, 4, 0xF986);
    }
}

// =====================================
// DIBUJAR OPCION
// =====================================

void drawOption(
    int x,
    const char* label,
    bool red
) {

    uint16_t bg =
        red ? ILI9341_RED : ILI9341_YELLOW;

    uint16_t fg =
        red ? ILI9341_WHITE : ILI9341_BLACK;

    tft.fillRoundRect(
        x,
        193,
        142,
        42,
        8,
        bg
    );

    tft.setTextSize(2);
    tft.setTextColor(fg);

    int textWidth = strlen(label) * 12;

    int textX = x + (142 - textWidth) / 2;

    tft.setCursor(textX, 207);

    tft.print(label);
}

// =====================================
// MOSTRAR PREGUNTA
// =====================================

void drawQuestion() {

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
    tft.print(" DE 6");

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

    // Ilustracion.

    drawCharacter(words[wordIndex].illustration);

    // Opciones.

    drawOption(10, words[leftOption].english, true);

    drawOption(168, words[rightOption].english, false);
}

// =====================================
// CREAR PARTIDA
// =====================================

void startGame() {

    score = 0;
    currentQuestion = 0;
    gameFinished = false;

    for (int i = 0; i < TOTAL_WORDS; i++) {
        questionOrder[i] = i;
    }

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

    if (gameFinished) return;

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
    tft.setCursor(125, 145);

    tft.print(score);
    tft.print("/6");

    tft.setTextSize(1);
    tft.setTextColor(DARK_BLUE);
    tft.setCursor(55, 210);

    tft.print("Pulsa cualquier boton para jugar");
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

    Serial.println("=== INICIO SETUP ===");

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

    // Conexion Wi-Fi.

    connectWiFi();

    // Sistema de archivos.

    initLittleFS();

    if (littleFSReady) {

        // Recuperar imagenes si hubo
        // una interrupcion durante una actualizacion.

        recoverImageFiles();

        // Instalar imagenes originales
        // si todavia no existen.

        installDefaultImages();

        // Comprobar actualizaciones en GitHub.

        updateImagesFromGitHub();
    }

    // Iniciar juego.

    startGame();

    Serial.println();
    Serial.println("English Kids iniciado");
}

// =====================================
// LOOP
// =====================================

void loop() {

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
            Serial.println(
                "Actualizacion manual solicitada"
            );

            updateImagesFromGitHub();

            // Volver a mostrar la pantalla actual.

            if (gameFinished) {

                drawEndScreen();

            } else {

                drawQuestion();
            }
        }

        lastRedState = redState;
        lastYellowState = yellowState;

        return;
    }

    // Reiniciar deteccion de combinacion.

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
