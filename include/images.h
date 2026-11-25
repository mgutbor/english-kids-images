#pragma once

#include <Arduino.h>

extern uint16_t imageBuffer[120 * 120];

bool updateImagesForActiveCollection();
bool downloadImage(const char* filename, size_t expectedSize, const char* expectedHash);
bool loadImage(const char* filename);

bool calculateFileSHA256(
    const char* path,
    char* outputHash,
    size_t outputSize
);

bool localImageIsValid(
    const char* path,
    size_t expectedSize,
    const char* expectedHash
);