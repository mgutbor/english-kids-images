#include <Arduino.h>
#include "hardware.h"
#include "input.h"
#include "config.h"

static bool previousRed = false;
static bool previousYellow = false;

static unsigned long bothPressedAt = 0;
static unsigned long lastActionAt = 0;

static bool longPressHandled = false;
static bool shortPressPending = false;

void initInput() {
    previousRed = false;
    previousYellow = false;
    bothPressedAt = 0;
    lastActionAt = 0;
    longPressHandled = false;
    shortPressPending = false;
}

void resetInput() {
    initInput();
}

InputAction readInput() {
    bool red = redPressed();
    bool yellow = yellowPressed();
    unsigned long now = millis();

    if (red && yellow) {
        if (bothPressedAt == 0) {
            bothPressedAt = now;
            longPressHandled = false;
            shortPressPending = false;
        }

        if (!longPressHandled &&
            now - bothPressedAt >= LONG_PRESS_DELAY) {
            longPressHandled = true;
            shortPressPending = false;
            return INPUT_BOTH_LONG;
        }

        return INPUT_NONE;
    }

    if (bothPressedAt != 0) {
        unsigned long duration = now - bothPressedAt;
        bothPressedAt = 0;

        if (!longPressHandled && duration >= DEBOUNCE_DELAY) {
            lastActionAt = now;
            return INPUT_BOTH_SHORT;
        }

        longPressHandled = false;
        return INPUT_NONE;
    }

    if (now - lastActionAt < DEBOUNCE_DELAY) {
        return INPUT_NONE;
    }

    if (red && !previousRed && !yellow) {
        previousRed = true;
        lastActionAt = now;
        return INPUT_RED;
    }

    if (yellow && !previousYellow && !red) {
        previousYellow = true;
        lastActionAt = now;
        return INPUT_YELLOW;
    }

    if (!red) previousRed = false;
    if (!yellow) previousYellow = false;

    return INPUT_NONE;
}