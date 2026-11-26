#include <Arduino.h>

#include "config.h"
#include "game.h"
#include "collections.h"
#include "display.h"
#include "hardware.h"

bool gameFinished = false;
int score = 0;
int currentQuestion = 0;

int leftOption = -1;
int rightOption = -1;

static int questionOrder[MAX_WORDS];

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
    // Índice real de la palabra que hay que adivinar.
    int correctOption = questionOrder[currentQuestion];

    // Elegir una palabra incorrecta diferente.
    int incorrectOption;

    do {
        incorrectOption = random(totalWords);
    } while (incorrectOption == correctOption);

    // Decidir aleatoriamente dónde aparece la correcta.
    if (random(2) == 0) {
        leftOption = correctOption;
        rightOption = incorrectOption;
    } else {
        leftOption = incorrectOption;
        rightOption = correctOption;
    }
}

void startGame() {
    if (totalWords < 2) {
        gameFinished = true;
        drawFatalError("Necesitas al menos dos palabras");
        return;
    }

    score = 0;
    currentQuestion = 0;
    gameFinished = false;

    Serial.printf(
        "Partida iniciada: %s (%d palabras)\n",
        collectionTitle,
        totalWords
    );

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

    // El botón rojo selecciona izquierda.
    // El botón amarillo selecciona derecha.
    int selectedOption = redAnswer
        ? leftOption
        : rightOption;

    // La respuesta correcta siempre es questionOrder[currentQuestion].
    int correctOption = questionOrder[currentQuestion];

    bool correct = selectedOption == correctOption;

    if (correct) {
        score++;
    }

    tft.fillRect(0, 145, 320, 30, ILI9341_BLACK);

    tft.setTextSize(2);
    tft.setCursor(95, 150);

    if (correct) {
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

int* getQuestionOrder() {
    return questionOrder;
}