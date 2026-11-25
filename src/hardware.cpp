#include "hardware.h"

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

void initHardware() {
    pinMode(BUTTON_RED, INPUT_PULLUP);
    pinMode(BUTTON_YELLOW, INPUT_PULLUP);

    SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);

    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(ILI9341_BLACK);
}

bool redPressed() {
    return digitalRead(BUTTON_RED) == LOW;
}

bool yellowPressed() {
    return digitalRead(BUTTON_YELLOW) == LOW;
}

bool bothPressed() {
    return redPressed() && yellowPressed();
}