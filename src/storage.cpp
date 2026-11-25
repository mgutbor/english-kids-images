#include <Arduino.h>
#include <LittleFS.h>
#include "storage.h"

bool initStorage() {
    // Nunca formatear automáticamente: podemos perder contenido.
    if (!LittleFS.begin(false)) {
        Serial.println("ERROR: No se pudo montar LittleFS");
        return false;
    }

    Serial.println("LittleFS montado correctamente");
    return true;
}

bool fileExists(const char* path) {
    return LittleFS.exists(path);
}

bool ensureDirectory(const char* path) {
    if (LittleFS.exists(path)) {
        return true;
    }

    return LittleFS.mkdir(path);
}

bool readTextFile(const char* path, String& content) {
    File file = LittleFS.open(path, FILE_READ);

    if (!file) {
        return false;
    }

    content = file.readString();
    file.close();

    return true;
}

bool recoverAtomicFile(const char* path) {
    String temporaryPath = String(path) + ".tmp";
    String backupPath = String(path) + ".bak";

    if (LittleFS.exists(path)) {
        if (LittleFS.exists(temporaryPath)) {
            LittleFS.remove(temporaryPath);
        }

        if (LittleFS.exists(backupPath)) {
            LittleFS.remove(backupPath);
        }

        return true;
    }

    if (LittleFS.exists(backupPath)) {
        if (LittleFS.rename(backupPath, path)) {
            if (LittleFS.exists(temporaryPath)) {
                LittleFS.remove(temporaryPath);
            }
            return true;
        }
    }

    return false;
}

bool writeFileAtomic(const char* path, const String& content) {
    String temporaryPath = String(path) + ".tmp";
    String backupPath = String(path) + ".bak";

    if (LittleFS.exists(temporaryPath)) {
        LittleFS.remove(temporaryPath);
    }

    File file = LittleFS.open(temporaryPath, FILE_WRITE);

    if (!file) {
        Serial.printf("No se pudo crear %s\n", temporaryPath.c_str());
        return false;
    }

    size_t written = file.print(content);
    file.flush();
    file.close();

    if (written != content.length()) {
        LittleFS.remove(temporaryPath);
        return false;
    }

    if (LittleFS.exists(backupPath)) {
        LittleFS.remove(backupPath);
    }

    bool hadOriginal = LittleFS.exists(path);

    if (hadOriginal && !LittleFS.rename(path, backupPath)) {
        LittleFS.remove(temporaryPath);
        return false;
    }

    if (!LittleFS.rename(temporaryPath, path)) {
        if (hadOriginal) {
            LittleFS.rename(backupPath, path);
        }
        return false;
    }

    if (LittleFS.exists(backupPath)) {
        LittleFS.remove(backupPath);
    }

    return true;
}