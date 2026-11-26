#include <Arduino.h>
#include "hardware.h"
#include "input.h"
#include "config.h"

static bool previousRed = false;
static bool previousYellow = false;

static unsigned long redPressedAt = 0;
static unsigned long yellowPressedAt = 0;
static unsigned long bothPressedAt = 0;
static unsigned long lastActionAt = 0;

static bool redConsumed = false;
static bool yellowConsumed = false;

static bool bothTracking = false;
static bool bothLongHandled = false;

void initInput() {
    previousRed = false;
    previousYellow = false;

    redPressedAt = 0;
    yellowPressedAt = 0;
    bothPressedAt = 0;
    lastActionAt = 0;

    redConsumed = false;
    yellowConsumed = false;

    bothTracking = false;
    bothLongHandled = false;
}

void resetInput() {
    initInput();
}

InputAction readInput() {
    bool red = redPressed();
    bool yellow = yellowPressed();
    unsigned long now = millis();

    bool redRising = red && !previousRed;
    bool yellowRising = yellow && !previousYellow;

    bool redFalling = !red && previousRed;
    bool yellowFalling = !yellow && previousYellow;

    previousRed = red;
    previousYellow = yellow;

    if (redRising) {
        redPressedAt = now;
        redConsumed = false;
    }

    if (yellowRising) {
        yellowPressedAt = now;
        yellowConsumed = false;
    }

    // Ambos botones pulsados: abrir selector con pulsación larga.
    if (red && yellow) {
        if (!bothTracking) {
            bothTracking = true;
            bothPressedAt = now;

            redConsumed = true;
            yellowConsumed = true;
        }

        if (!bothLongHandled &&
            now - bothPressedAt >= LONG_PRESS_DELAY) {

            bothLongHandled = true;
            lastActionAt = now;

            return INPUT_BOTH_LONG;
        }

        return INPUT_NONE;
    }

    // Finalizar gesto de ambos botones.
    if (bothTracking) {
        bool wasLong = bothLongHandled;

        bothTracking = false;
        bothLongHandled = false;

        if (!wasLong &&
            now - lastActionAt >= DEBOUNCE_DELAY) {

            lastActionAt = now;
            return INPUT_BOTH_SHORT;
        }
    }

    // Rojo mantenido durante 2 segundos.
    if (red && !yellow && !redConsumed &&
        now - redPressedAt >= LONG_PRESS_DELAY) {

        redConsumed = true;
        lastActionAt = now;

        return INPUT_RED_LONG;
    }

    // Pulsación corta del rojo.
    if (redFalling) {
        bool validPress = !redConsumed;

        redConsumed = false;

        if (validPress &&
            now - lastActionAt >= DEBOUNCE_DELAY) {

            lastActionAt = now;
            return INPUT_RED;
        }
    }

    // Pulsación corta del amarillo.
    if (yellowFalling) {
        bool validPress = !yellowConsumed;

        yellowConsumed = false;

        if (validPress &&
            now - lastActionAt >= DEBOUNCE_DELAY) {

            lastActionAt = now;
            return INPUT_YELLOW;
        }
    }

    return INPUT_NONE;
}