#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <mbedtls/sha256.h>

#include "config.h"
#include "collections.h"
#include "images.h"
#include "storage.h"

uint16_t imageBuffer[IMAGE_BUFFER_SIZE];

static String getImagePath(const char* filename) {
    return String("/collections/") +
        activeCollection + "/images/" + filename;
}

bool calculateFileSHA256(
    const char* path,
    char* outputHash,
    size_t outputSize
) {
    if (outputSize < 65) return false;

    File file = LittleFS.open(path, FILE_READ);
    if (!file) return false;

    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    mbedtls_sha256_starts(&context, 0);

    uint8_t buffer[512];

    while (file.available()) {
        size_t count = file.read(buffer, sizeof(buffer));
        mbedtls_sha256_update(&context, buffer, count);
    }

    file.close();

    uint8_t hash[32];
    mbedtls_sha256_finish(&context, hash);
    mbedtls_sha256_free(&context);

    for (int i = 0; i < 32; i++) {
        sprintf(outputHash + i * 2, "%02x", hash[i]);
    }

    outputHash[64] = '\0';
    return true;
}

bool localImageIsValid(
    const char* path,
    size_t expectedSize,
    const char* expectedHash
) {
    if (!LittleFS.exists(path)) return false;

    File file = LittleFS.open(path, FILE_READ);

    if (!file) return false;

    size_t size = file.size();
    file.close();

    if (size != expectedSize) return false;

    if (!expectedHash || strlen(expectedHash) != 64) {
        return false;
    }

    char localHash[65];

    if (!calculateFileSHA256(path, localHash, sizeof(localHash))) {
        return false;
    }

    return strcasecmp(localHash, expectedHash) == 0;
}

bool downloadImage(
    const char* filename,
    size_t expectedSize,
    const char* expectedHash
) {
    if (!filename || !filename[0] || expectedSize == 0) {
        return false;
    }

    String directory = String("/collections/") +
        activeCollection + "/images";

    if (!ensureDirectory("/collections")) return false;

    String collectionDirectory = String("/collections/") + activeCollection;

    if (!ensureDirectory(collectionDirectory.c_str())) return false;
    if (!ensureDirectory(directory.c_str())) return false;

    String path = getImagePath(filename);

    if (localImageIsValid(
        path.c_str(),
        expectedSize,
        expectedHash
    )) {
        return true;
    }

    if (WiFi.status() != WL_CONNECTED) return false;

    String url = String(GITHUB_RAW) +
        "collections/" + activeCollection +
        "/images/" + filename;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;

    if (!http.begin(client, url)) return false;

    int status = http.GET();

    if (status != HTTP_CODE_OK) {
        Serial.printf("ERROR: imagen %s HTTP %d\n", filename, status);
        http.end();
        return false;
    }

    int contentLength = http.getSize();

    if (contentLength > 0 &&
        (size_t)contentLength != expectedSize) {
        Serial.printf(
            "ERROR: imagen %s tamaño remoto %d (esperado %u)\n",
            filename,
            contentLength,
            (unsigned)expectedSize
        );
        http.end();
        return false;
    }

    String temporaryPath = path + ".tmp";

    if (LittleFS.exists(temporaryPath)) {
        LittleFS.remove(temporaryPath);
    }

    File output = LittleFS.open(temporaryPath, FILE_WRITE);

    if (!output) {
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();

    uint8_t buffer[1024];
    size_t total = 0;
    unsigned long lastData = millis();

    while (http.connected() &&
           (contentLength > 0 || contentLength == -1)) {

        size_t available = stream->available();

        if (available) {
            int count = stream->readBytes(
                buffer,
                min(available, sizeof(buffer))
            );

            if (count > 0) {
                output.write(buffer, count);
                total += count;
                lastData = millis();
            }
        } else {
            if (millis() - lastData > 10000) break;
            delay(1);
        }

        if (contentLength > 0) {
            contentLength -= min(
                (int)available,
                (int)sizeof(buffer)
            );
        }

        if (total > expectedSize) break;
    }

    output.flush();
    output.close();
    http.end();

    if (total != expectedSize) {
        Serial.printf(
            "ERROR: imagen %s tamaño descargado %u\n",
            filename,
            (unsigned)total
        );
        LittleFS.remove(temporaryPath);
        return false;
    }

    if (!localImageIsValid(
        temporaryPath.c_str(),
        expectedSize,
        expectedHash
    )) {
        Serial.printf("ERROR: imagen %s hash inválido\n", filename);
        LittleFS.remove(temporaryPath);
        return false;
    }

    String backupPath = path + ".bak";

    if (LittleFS.exists(backupPath)) {
        LittleFS.remove(backupPath);
    }

    bool hadOriginal = LittleFS.exists(path);

    if (hadOriginal && !LittleFS.rename(path, backupPath)) {
        LittleFS.remove(temporaryPath);
        return false;
    }

    if (!LittleFS.rename(temporaryPath, path)) {
        if (hadOriginal) LittleFS.rename(backupPath, path);
        return false;
    }

    if (LittleFS.exists(backupPath)) {
        LittleFS.remove(backupPath);
    }

    return true;
}

bool updateImagesForActiveCollection() {
    if (!collectionLoaded) return false;

    bool allOk = true;

    for (int i = 0; i < totalWords; i++) {
        Word& word = words[i];

        if (!word.hasImage || !word.imageAvailable) {
            continue;
        }

        // El tamaño indicado en el contenido debe ser el tamaño real.
        // Las imágenes RGB565 de 120x120 ocupan 28.800 bytes.
        const size_t expectedSize = IMAGE_BUFFER_SIZE * 2;

        String path = getImagePath(word.imageFile);

        if (!localImageIsValid(
            path.c_str(),
            expectedSize,
            word.imageHash
        )) {
            if (!downloadImage(
                word.imageFile,
                expectedSize,
                word.imageHash
            )) {
                allOk = false;
            }
        }
    }

    if (!allOk) {
        Serial.println("ERROR: imágenes sin actualizar");
    }

    return allOk;
}

bool loadImage(const char* filename) {
    String path = getImagePath(filename);

    File file = LittleFS.open(path, FILE_READ);

    if (!file) return false;

    if (file.size() != sizeof(imageBuffer)) {
        file.close();
        return false;
    }

    size_t read = file.read(
        (uint8_t*)imageBuffer,
        sizeof(imageBuffer)
    );

    file.close();

    return read == sizeof(imageBuffer);
}