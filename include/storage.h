#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

bool initStorage();

bool readTextFile(
    const char* path,
    String& content
);

bool writeFileAtomic(
    const char* path,
    const String& content
);

bool fileExists(const char* path);

bool ensureDirectory(const char* path);

bool recoverAtomicFile(const char* path);