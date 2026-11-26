#pragma once

#include <Arduino.h>

extern bool gameFinished;
extern int score;
extern int currentQuestion;

extern int leftOption;
extern int rightOption;

extern int* getQuestionOrder();

void startGame();
void nextQuestion();
void checkAnswer(bool redAnswer);
void finishGame();

bool isGameFinished();