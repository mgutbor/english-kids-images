#pragma once

#include <Arduino.h>

void drawHeader(const char* title);
void drawQuestion();
void drawOption(int x, int y, const char* text, bool selected = false);

void drawCharacter();
void drawEndScreen();
void drawFatalError(const char* message);

void drawCollectionSelector(
    const char* title,
    int index,
    int total
);

void drawSpanishTextCentered(
    const String& text,
    int centerX,
    int y,
    uint16_t color,
    uint8_t textSize
);