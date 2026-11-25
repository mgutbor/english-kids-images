#pragma once

#include <Arduino.h>

#define MAX_WORDS 30
#define MAX_COLLECTIONS 20

#define MAX_COLLECTION_ID 48
#define MAX_WORD_ID 40
#define MAX_WORD_TEXT 32
#define MAX_IMAGE_FILE 64

#define IMAGE_WIDTH 120
#define IMAGE_HEIGHT 120
#define IMAGE_BUFFER_SIZE (IMAGE_WIDTH * IMAGE_HEIGHT)

constexpr size_t CONTENT_JSON_CAPACITY = 16384;
constexpr size_t CONFIG_JSON_CAPACITY = 1024;
constexpr size_t CATALOG_JSON_CAPACITY = 4096;

constexpr unsigned long DEBOUNCE_DELAY = 180;
constexpr unsigned long LONG_PRESS_DELAY = 2000;

extern const char* GITHUB_RAW;
extern const char* CONFIG_URL;
extern const char* CATALOG_URL;