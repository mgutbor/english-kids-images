#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#define TFT_CS    4
#define TFT_RST   1
#define TFT_DC    2
#define TFT_MOSI  11
#define TFT_SCK   12
#define TFT_MISO  13

#define BUTTON_RED     6
#define BUTTON_YELLOW  7

extern Adafruit_ILI9341 tft;

void initHardware();
bool redPressed();
bool yellowPressed();
bool bothPressed();