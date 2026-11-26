
#include <Arduino.h>
#include "hardware.h"
#include "display.h"
#include "collections.h"
#include "images.h"
#include "game.h"
#include "config.h"

#include <U8g2_for_Adafruit_GFX.h>

// =====================================================
// MOTOR DE TEXTO UTF-8
// =====================================================

static U8G2_FOR_ADAFRUIT_GFX u8g2;
static bool u8g2Initialized = false;

static void initU8g2() {
    if (u8g2Initialized) return;

    // Se conecta a la pantalla Adafruit GFX ya inicializada.
    u8g2.begin(tft);

    // Fuente Unicode con caracteres latinos.
    u8g2.setFont(u8g2_font_unifont_t_latin);
    u8g2.setFontMode(1);
    u8g2.setFontDirection(0);

    u8g2Initialized = true;
}

// =====================================================
// CABECERA
// =====================================================

void drawHeader(const char* title) {
    tft.fillRect(0, 0, 320, 32, ILI9341_NAVY);

    // Título de la colección
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(8, 8);
    tft.print(title);

    // Contador de preguntas
    tft.setTextSize(2);
    char counter[12];
    snprintf(counter, sizeof(counter), "%d/%d",
             currentQuestion + 1, totalWords);

    int16_t x1, y1;
    uint16_t w, h;

    tft.getTextBounds(counter, 0, 0, &x1, &y1, &w, &h);

    tft.setCursor(312 - w - x1, 8);
    tft.print(counter);
}

// =====================================================
// ÁREA DE JUEGO
// =====================================================

void drawCharacter() {
    tft.fillRect(0, 32, 320, 208, ILI9341_BLACK);
}

// =====================================================
// BOTONES DE RESPUESTA
// =====================================================

void drawOption(
    int x,
    int y,
    const char* text,
    bool selected
) {
    uint16_t background =
        (x < 160) ? ILI9341_RED : ILI9341_YELLOW;

    tft.fillRoundRect(x, y, 140, 48, 8, background);
    tft.drawRoundRect(x, y, 140, 48, 8, ILI9341_WHITE);

    tft.setTextColor(
        x < 160 ? ILI9341_WHITE : ILI9341_BLACK
    );

    tft.setTextSize(2);

    int16_t x1, y1;
    uint16_t w, h;

    tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

    tft.setCursor(
        x + (140 - w) / 2 - x1,
        y + (48 - h) / 2
    );

    tft.print(text);
}

// =====================================================
// PREGUNTA
// =====================================================

void drawQuestion() {
    if (!collectionLoaded || totalWords == 0) return;

    drawHeader(collectionTitle);
    drawCharacter();

    int wordIndex = getQuestionOrder()[currentQuestion];

    drawSpanishTextCentered(
        String(words[wordIndex].spanish),
        160,
        42,
        ILI9341_WHITE,
        2
    );

    if (words[wordIndex].hasImage &&
        words[wordIndex].imageAvailable &&
        loadImage(words[wordIndex].imageFile)) {

        tft.drawRGBBitmap(
            100,
            65,
            imageBuffer,
            IMAGE_WIDTH,
            IMAGE_HEIGHT
        );
    }

    drawOption(10, 190, words[leftOption].english);
    drawOption(170, 190, words[rightOption].english);
}

// =====================================================
// PANTALLA FINAL
// =====================================================

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

// =====================================================
// SELECTOR DE COLECCIONES
// =====================================================

void drawCollectionSelector(
    const char* title,
    int index,
    int total
) {
    tft.fillScreen(ILI9341_BLACK);

    // Cabecera
    tft.fillRect(0, 0, 320, 32, ILI9341_NAVY);

    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(8, 8);
    tft.print("ENGLISH KIDS");

    // Contador de colecciones
    tft.setTextSize(1);
    tft.setCursor(270, 12);
    tft.printf("%d/%d", index + 1, total);

    // Nombre de colección
    tft.setTextSize(2);

    int16_t x1, y1;
    uint16_t w, h;

    tft.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);

    tft.setCursor((320 - w) / 2 - x1, 75);
    tft.print(title);

    // Botones
    drawOption(10, 125, "ANTERIOR");
    drawOption(170, 125, "SIGUIENTE");

    // Instrucción
    const char* instruction = "MANTENER ROJO 2s: ACEPTAR";

    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);

    tft.getTextBounds(instruction, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((320 - w) / 2 - x1, 205);
    tft.print(instruction);
}
// =====================================================
// TEXTO ESPAÑOL UTF-8 CENTRADO
// =====================================================

void drawSpanishTextCentered(
    const String& text,
    int centerX,
    int y,
    uint16_t color,
    uint8_t textSize
) {
    initU8g2();

    // La fuente Unifont tiene un tamaño fijo.
    // textSize se conserva por compatibilidad con
    // las llamadas existentes de display.h.
    (void)textSize;

    u8g2.setFont(u8g2_font_helvB18_tf);
    u8g2.setFontMode(1);
    u8g2.setForegroundColor(color);

    const char* utf8Text = text.c_str();

    // Medir el texto interpretando UTF-8.
    int16_t textWidth = u8g2.getUTF8Width(utf8Text);

    // U8g2 utiliza la línea base como coordenada Y.
    int16_t baseline =
        y + u8g2.getFontAscent() - 5;

    int16_t x = centerX - textWidth / 2;

    u8g2.drawUTF8(
        x,
        baseline,
        utf8Text
    );
}

// =====================================================
// ERROR
// =====================================================

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
