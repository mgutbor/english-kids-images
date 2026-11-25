#include <Arduino.h>
#include "game.h"
#include "collections.h"
#include "display.h"
#include "hardware.h"
#include "config.h"

bool gameFinished = false;
int score = 0;
int currentQuestion = 0;

static int questionOrder[MAX_WORDS];
int leftOption = 0;
int rightOption = 1;
static bool leftIsCorrect = true;

static void shuffleQuestions() {
    for (int i = 0; i < totalWords; i++) {
        questionOrder[i] = i;
    }

    for (int i = totalWords - 1; i > 0; i--) {
        int j = random(i + 1);

        int temp = questionOrder[i];
        questionOrder[i] = questionOrder[j];
        questionOrder[j] = temp;
    }
}

static void chooseOptions() {
    int correct = questionOrder[currentQuestion];

    int incorrect;

    do {
        incorrect = random(totalWords);
    } while (incorrect == correct);

    leftIsCorrect = random(2) == 0;

    if (leftIsCorrect) {
        leftOption = correct;
        rightOption = incorrect;
    } else {
        leftOption = incorrect;
        rightOption = correct;
    }
}

void startGame() {
    if (totalWords < 2) {
        gameFinished = true;
        drawFatalError("La coleccion necesita al menos dos palabras");
        return;
    }

    score = 0;
    currentQuestion = 0;
    gameFinished = false;

    shuffleQuestions();
    chooseOptions();
    drawQuestion();
}

void nextQuestion() {
    currentQuestion++;

    if (currentQuestion >= totalWords) {
        finishGame();
        return;
    }

    chooseOptions();
    drawQuestion();
}

void checkAnswer(bool redAnswer) {
    if (gameFinished) return;

    bool correct = redAnswer
        ? leftIsCorrect
        : !leftIsCorrect;

    tft.fillRect(0, 145, 320, 30, ILI9341_BLACK);

    tft.setTextSize(2);
    tft.setCursor(95, 150);

    if (correct) {
        score++;
        tft.setTextColor(ILI9341_GREEN);
        tft.print("CORRECTO");
    } else {
        tft.setTextColor(ILI9341_RED);
        tft.print("INCORRECTO");
    }

    delay(1100);
    nextQuestion();
}

void finishGame() {
    gameFinished = true;
    drawEndScreen();
}

bool isGameFinished() {
    return gameFinished;
}