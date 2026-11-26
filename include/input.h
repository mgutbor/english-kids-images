#pragma once

enum InputAction {
    INPUT_NONE,
    INPUT_RED,
    INPUT_YELLOW,
    INPUT_RED_LONG,
    INPUT_BOTH_SHORT,
    INPUT_BOTH_LONG
};

void initInput();
InputAction readInput();
void resetInput();