#include <Arduino.h>
#include "hardware.h"
#include "display.h"
#include "collections.h"
#include "images.h"
#include "game.h"
#include "config.h"

void drawHeader(const char* title) {
    tft.fillRect(0, 0, 320, 32, ILI9341_NAVY);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(8, 8);
    tft.print(title);
}

void drawCharacter() {
    tft.fillRect(0, 32, 320, 208, ILI9341_BLACK);
}

void drawOption(
    int x,
    int y,
    const char* text,
    bool selected
) {
    uint16_t background = selected
        ? ILI9341_GREEN
        : ILI9341_DARKCYAN;

    tft.fillRoundRect(x, y, 140, 48, 8, background);
    tft.drawRoundRect(x, y, 140, 48, 8, ILI9341_WHITE);

    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);

    int16_t x1, y1;
    uint16_t w, h;

    tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

    tft.setCursor(x + (140 - w) / 2, y + 15);
    tft.print(text);
}

void drawQuestion() {
    if (!collectionLoaded || totalWords == 0) return;

    drawHeader(collectionTitle);
    drawCharacter();

    int wordIndex = currentQuestion;

    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 42);
    tft.print(words[wordIndex].spanish);

    if (words[wordIndex].hasImage &&
        words[wordIndex].imageAvailable &&
        loadImage(words[wordIndex].imageFile)) {

        tft.drawRGBBitmap(
            100, 65, imageBuffer,
            IMAGE_WIDTH, IMAGE_HEIGHT
        );
    }

    drawOption(10, 185, words[leftOption].english);
    drawOption(170, 185, words[rightOption].english);
}

void drawEndScreen() {
    tft.fillScreen(ILI9341_BLACK);

    tft.setTextColor(ILI9341_GREEN);
    tft.setTextSize(3);
    tft.setCursor(70, 30);
    tft.print("FIN");

    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(55, 85);
    tft.print("Resultado");

    tft.setTextSize(3);
    tft.setCursor(130, 115);
    tft.printf("%d/%d", score, totalWords);

    drawOption(10, 175, "REPETIR");
    drawOption(170, 175, "CAMBIAR");
}

void drawCollectionSelector(
    const char* title,
    int index,
    int total
) {
    tft.fillScreen(ILI9341_BLACK);

    drawHeader("COLECCIONES");

    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);

    int16_t x1, y1;
    uint16_t w, h;

    tft.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);

    tft.setCursor((320 - w) / 2, 85);
    tft.print(title);

    tft.setTextSize(1);
    tft.setCursor(135, 125);
    tft.printf("%d / %d", index + 1, total);

    drawOption(10, 175, "ANTERIOR");
    drawOption(170, 175, "SIGUIENTE");

    tft.setTextColor(ILI9341_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(60, 230);
    tft.print("Pulsa ambos botones para elegir");
}

void drawFatalError(const char* message) {
    tft.fillScreen(ILI9341_BLACK);

    tft.setTextColor(ILI9341_RED);
    tft.setTextSize(2);
    tft.setCursor(10, 30);
    tft.println("ERROR");

    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(1);
    tft.setCursor(10, 70);
    tft.println(message);
}

// Estas funciones se mantienen como punto de entrada para el
// renderizado de texto español del firmware original.
void drawSpanishTextCentered(
    const String& text,
    int centerX,
    int y,
    uint16_t color,
    uint8_t textSize
) {
    tft.setTextColor(color);
    tft.setTextSize(textSize);

    int16_t x1, y1;
    uint16_t w, h;

    tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

    tft.setCursor(centerX - (int)w / 2, y);
    tft.print(text);
}