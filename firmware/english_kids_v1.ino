#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <WiFi.h>
#include <LittleFS.h>

#include "secrets.h"

#include "ugly.h"
#include "beautiful.h"
#include "happy.h"
#include "sad.h"
#include "old.h"
#include "young.h"

// =====================================
// CONFIGURACION HARDWARE
// =====================================

#define TFT_CS    4
#define TFT_RST   1
#define TFT_DC    2
#define TFT_MOSI  35
#define TFT_SCK   36
#define TFT_MISO  37

#define BUTTON_RED     6
#define BUTTON_YELLOW  7

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

// =====================================
// COLORES
// =====================================

#define DARK_BLUE  0x192F
#define LIGHT_BG   0xEF7D
#define GREEN      0x2666
#define ORANGE     0xFD20

// =====================================
// WIFI
// =====================================

bool wifiConnected = false;
bool littleFSReady = false;

// =====================================
// IMAGENES
// =====================================

#define IMAGE_WIDTH  120
#define IMAGE_HEIGHT 120

const size_t IMAGE_BYTES =
  IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(uint16_t);

// Buffer para cargar una imagen desde LittleFS.
// 28.800 bytes de RAM.
uint16_t imageBuffer[IMAGE_WIDTH * IMAGE_HEIGHT];

// =====================================
// VOCABULARIO
// =====================================

struct Word {
  const char* spanish;
  const char* english;
  uint8_t illustration;
};

Word words[] = {
  {"FEO",       "UGLY",      0},
  {"GUAPO",     "BEAUTIFUL", 1},
  {"ALEGRE",    "HAPPY",     2},
  {"TRISTE",    "SAD",       3},
  {"VIEJO",     "OLD",       4},
  {"JOVEN",     "YOUNG",     5}
};

const int TOTAL_WORDS = 6;

int questionOrder[TOTAL_WORDS];
int currentQuestion = 0;
int score = 0;

int leftOption;
int rightOption;
bool leftIsCorrect;

bool gameFinished = false;

bool lastRedState = HIGH;
bool lastYellowState = HIGH;

unsigned long lastPress = 0;
const unsigned long debounceDelay = 180;

// =====================================
// WIFI
// =====================================

void connectWiFi() {

  Serial.println();
  Serial.println("Conectando a Wi-Fi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startAttempt < 15000) {

    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {

    wifiConnected = true;

    Serial.println();
    Serial.println("Wi-Fi conectado");

    Serial.print("Direccion IP: ");
    Serial.println(WiFi.localIP());

  } else {

    wifiConnected = false;

    Serial.println();
    Serial.println("No se pudo conectar a Wi-Fi");
    Serial.println("Continuamos en modo offline");

    WiFi.disconnect();
  }
}

// =====================================
// LITTLEFS
// =====================================

void initLittleFS() {

  Serial.println();
  Serial.println("Iniciando LittleFS...");

  littleFSReady = LittleFS.begin(true);

  if (littleFSReady) {

    Serial.println("LittleFS iniciado correctamente");

    Serial.print("Espacio total: ");
    Serial.println(LittleFS.totalBytes());

    Serial.print("Espacio usado: ");
    Serial.println(LittleFS.usedBytes());

  } else {

    Serial.println("Error iniciando LittleFS");
  }
}

// =====================================
// GUARDAR IMAGEN EN LITTLEFS
// =====================================

bool saveImageIfNeeded(
  const char* path,
  const uint16_t* bitmap
) {

  // Comprobar si ya existe y tiene el tamaño correcto.

  if (LittleFS.exists(path)) {

    File existingFile = LittleFS.open(path, "r");

    if (existingFile) {

      bool validSize = existingFile.size() == IMAGE_BYTES;

      existingFile.close();

      if (validSize) {
        Serial.print("Imagen ya instalada: ");
        Serial.println(path);
        return true;
      }
    }
  }

  Serial.print("Instalando imagen: ");
  Serial.println(path);

  File file = LittleFS.open(path, "w");

  if (!file) {
    Serial.println("Error creando archivo");
    return false;
  }

  size_t written = file.write(
    (const uint8_t*)bitmap,
    IMAGE_BYTES
  );

  file.close();

  if (written != IMAGE_BYTES) {
    Serial.println("Error escribiendo imagen");
    return false;
  }

  Serial.println("Imagen guardada correctamente");

  return true;
}

// =====================================
// INSTALAR LAS SEIS IMAGENES
// =====================================

void installDefaultImages() {

  if (!littleFSReady) {
    Serial.println("LittleFS no disponible");
    return;
  }

  Serial.println();
  Serial.println("Instalando imagenes...");

  saveImageIfNeeded("/ugly.rgb565", ugly);
  saveImageIfNeeded("/beautiful.rgb565", beautiful);
  saveImageIfNeeded("/happy.rgb565", happy);
  saveImageIfNeeded("/sad.rgb565", sad);
  saveImageIfNeeded("/old.rgb565", old);
  saveImageIfNeeded("/young.rgb565", young);

  Serial.println("Instalacion de imagenes finalizada");
}

// =====================================
// CARGAR IMAGEN DESDE LITTLEFS
// =====================================

bool loadImage(const char* path) {

  if (!littleFSReady) {
    return false;
  }

  File file = LittleFS.open(path, "r");

  if (!file) {
    Serial.print("No se encuentra imagen: ");
    Serial.println(path);
    return false;
  }

  if (file.size() != IMAGE_BYTES) {
    Serial.print("Tamano incorrecto: ");
    Serial.println(path);
    file.close();
    return false;
  }

  size_t bytesRead = file.read(
    (uint8_t*)imageBuffer,
    IMAGE_BYTES
  );

  file.close();

  return bytesRead == IMAGE_BYTES;
}

// =====================================
// DIBUJAR CABECERA
// =====================================

void drawHeader() {

  tft.fillRect(0, 0, 320, 29, DARK_BLUE);

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(9, 7);
  tft.print("ENGLISH KIDS");

  tft.setTextSize(1);
  tft.setCursor(257, 10);
  tft.print(score);
  tft.print("/6");
}

// =====================================
// ILUSTRACIONES
// =====================================

void drawCharacter(int type) {

  const char* imagePath = nullptr;

  switch (type) {

    case 0:
      imagePath = "/ugly.rgb565";
      break;

    case 1:
      imagePath = "/beautiful.rgb565";
      break;

    case 2:
      imagePath = "/happy.rgb565";
      break;

    case 3:
      imagePath = "/sad.rgb565";
      break;

    case 4:
      imagePath = "/old.rgb565";
      break;

    case 5:
      imagePath = "/young.rgb565";
      break;
  }

  // Intentar cargar y dibujar desde LittleFS.

  if (imagePath != nullptr && loadImage(imagePath)) {

    tft.drawRGBBitmap(
      100,
      70,
      imageBuffer,
      IMAGE_WIDTH,
      IMAGE_HEIGHT
    );

    return;
  }

  // =================================
  // DIBUJO GEOMETRICO DE RESPALDO
  // =================================

  Serial.println("Usando ilustracion geometrica");

  int cx = 160;
  int cy = 130;

  uint16_t skin = 0xFEA0;
  uint16_t hair = 0x39E7;

  tft.fillCircle(cx, cy, 30, 0xDFFB);

  if (type == 4) hair = 0xBDF7;
  if (type == 5) hair = 0x7BEF;
  if (type == 0) hair = 0x2104;

  if (type != 4) {
    tft.fillCircle(cx, cy - 24, 21, hair);
    tft.fillRect(cx - 20, cy - 25, 40, 13, skin);
  }

  tft.fillCircle(cx, cy, 27, skin);

  tft.fillCircle(cx - 27, cy, 6, skin);
  tft.fillCircle(cx + 27, cy, 6, skin);

  if (type == 0) {
    tft.fillTriangle(cx - 24, cy - 17,
                     cx - 13, cy - 39,
                     cx - 5, cy - 20, hair);

    tft.fillTriangle(cx + 4, cy - 20,
                     cx + 20, cy - 39,
                     cx + 24, cy - 15, hair);
  }

  if (type == 1) {
    tft.fillCircle(cx, cy - 25, 20, hair);
    tft.fillRect(cx - 19, cy - 23, 38, 9, skin);
  }

  int eyeY = cy - 5;

  if (type == 0) {
    tft.fillCircle(cx - 10, eyeY - 2, 4, ILI9341_BLACK);
    tft.fillCircle(cx + 11, eyeY + 2, 3, ILI9341_BLACK);
  } else {
    tft.fillCircle(cx - 10, eyeY, 3, ILI9341_BLACK);
    tft.fillCircle(cx + 10, eyeY, 3, ILI9341_BLACK);
  }

  if (type == 4) {
    tft.drawCircle(cx - 10, eyeY, 8, DARK_BLUE);
    tft.drawCircle(cx + 10, eyeY, 8, DARK_BLUE);
    tft.drawLine(cx - 2, eyeY, cx + 2, eyeY, DARK_BLUE);

    tft.drawLine(cx - 20, cy + 7, cx - 12, cy + 9, DARK_BLUE);
    tft.drawLine(cx + 12, cy + 9, cx + 20, cy + 7, DARK_BLUE);
  }

  if (type == 1 || type == 2 || type == 5) {
    tft.drawLine(cx - 9, cy + 12, cx - 4, cy + 17, ILI9341_RED);
    tft.drawLine(cx - 4, cy + 17, cx + 4, cy + 17, ILI9341_RED);
    tft.drawLine(cx + 4, cy + 17, cx + 9, cy + 12, ILI9341_RED);
  }

  if (type == 3) {
    tft.drawLine(cx - 9, cy + 17, cx - 4, cy + 12, ILI9341_RED);
    tft.drawLine(cx - 4, cy + 12, cx + 4, cy + 12, ILI9341_RED);
    tft.drawLine(cx + 4, cy + 12, cx + 9, cy + 17, ILI9341_RED);
  }

  if (type == 0) {
    tft.drawLine(cx - 9, cy + 13, cx, cy + 18, ILI9341_RED);
    tft.drawLine(cx, cy + 18, cx + 9, cy + 11, ILI9341_RED);
  }

  if (type == 4) {
    tft.drawLine(cx - 7, cy + 15, cx + 7, cy + 15, ILI9341_RED);
  }

  if (type == 1 || type == 2 || type == 5) {
    tft.fillCircle(cx - 18, cy + 8, 4, 0xF986);
    tft.fillCircle(cx + 18, cy + 8, 4, 0xF986);
  }
}

// =====================================
// DIBUJAR OPCION
// =====================================

void drawOption(int x, const char* label, bool red) {

  uint16_t bg = red ? ILI9341_RED : ILI9341_YELLOW;
  uint16_t fg = red ? ILI9341_WHITE : ILI9341_BLACK;

  tft.fillRoundRect(x, 193, 142, 42, 8, bg);

  tft.setTextSize(2);
  tft.setTextColor(fg);

  int textWidth = strlen(label) * 12;
  int textX = x + (142 - textWidth) / 2;

  tft.setCursor(textX, 207);
  tft.print(label);
}

// =====================================
// MOSTRAR PREGUNTA
// =====================================

void drawQuestion() {

  int wordIndex = questionOrder[currentQuestion];

  int wrongIndex;

  do {
    wrongIndex = random(TOTAL_WORDS);
  } while (wrongIndex == wordIndex);

  leftIsCorrect = random(2) == 0;

  if (leftIsCorrect) {
    leftOption = wordIndex;
    rightOption = wrongIndex;
  } else {
    leftOption = wrongIndex;
    rightOption = wordIndex;
  }

  tft.fillScreen(LIGHT_BG);

  drawHeader();

  tft.setTextColor(DARK_BLUE);
  tft.setTextSize(1);
  tft.setCursor(10, 35);

  tft.print("PREGUNTA ");
  tft.print(currentQuestion + 1);
  tft.print(" DE 6");

  // Palabra en español.

  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(2);

  int wordWidth = strlen(words[wordIndex].spanish) * 12;

  tft.setCursor((320 - wordWidth) / 2, 49);
  tft.print(words[wordIndex].spanish);

  // Ilustración.

  drawCharacter(words[wordIndex].illustration);

  // Opciones.

  drawOption(10, words[leftOption].english, true);
  drawOption(168, words[rightOption].english, false);
}

// =====================================
// CREAR PARTIDA
// =====================================

void startGame() {

  score = 0;
  currentQuestion = 0;
  gameFinished = false;

  for (int i = 0; i < TOTAL_WORDS; i++) {
    questionOrder[i] = i;
  }

  for (int i = TOTAL_WORDS - 1; i > 0; i--) {

    int j = random(i + 1);

    int temp = questionOrder[i];
    questionOrder[i] = questionOrder[j];
    questionOrder[j] = temp;
  }

  drawQuestion();
}

// =====================================
// PREPARAR SIGUIENTE PREGUNTA
// =====================================

void nextQuestion() {

  currentQuestion++;

  if (currentQuestion >= TOTAL_WORDS) {

    gameFinished = true;
    drawEndScreen();

    return;
  }

  drawQuestion();
}

// =====================================
// COMPROBAR RESPUESTA
// =====================================

void checkAnswer(bool selectedLeft) {

  if (gameFinished) return;

  bool correct = (selectedLeft == leftIsCorrect);

  if (correct) {
    score++;
  }

  tft.fillRect(0, 29, 320, 19, LIGHT_BG);

  tft.setTextSize(2);
  tft.setCursor(90, 31);

  if (correct) {

    tft.setTextColor(GREEN);
    tft.print("CORRECTO!");

  } else {

    tft.setTextColor(ILI9341_RED);
    tft.print("INCORRECTO");
  }

  delay(1100);

  nextQuestion();
}

// =====================================
// FINAL DE PARTIDA
// =====================================

void drawEndScreen() {

  tft.fillScreen(LIGHT_BG);

  drawHeader();

  tft.setTextColor(DARK_BLUE);
  tft.setTextSize(3);
  tft.setCursor(62, 55);
  tft.print("FIN DEL JUEGO");

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_BLACK);
  tft.setCursor(75, 110);
  tft.print("RESULTADO");

  tft.setTextSize(4);
  tft.setTextColor(GREEN);
  tft.setCursor(125, 145);
  tft.print(score);
  tft.print("/6");

  tft.setTextSize(1);
  tft.setTextColor(DARK_BLUE);
  tft.setCursor(55, 210);
  tft.print("Pulsa cualquier boton para jugar");
}

// =====================================
// SETUP
// =====================================

void setup() {

  Serial.begin(115200);

  pinMode(BUTTON_RED, INPUT_PULLUP);
  pinMode(BUTTON_YELLOW, INPUT_PULLUP);

  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);

  tft.begin();
  tft.setRotation(1);

  tft.fillScreen(ILI9341_BLACK);

  randomSeed(esp_random());

  // Conexión Wi-Fi.

  connectWiFi();

  // Sistema de archivos.

  initLittleFS();

  // Copiar imágenes si todavía no están instaladas.

  installDefaultImages();

  // Iniciar juego.

  startGame();

  Serial.println();
  Serial.println("English Kids iniciado");
}

// =====================================
// LOOP
// =====================================

void loop() {

  bool redState = digitalRead(BUTTON_RED);
  bool yellowState = digitalRead(BUTTON_YELLOW);

  if (millis() - lastPress >= debounceDelay) {

    if (redState == LOW && lastRedState == HIGH) {

      lastPress = millis();

      if (gameFinished) {
        startGame();
      } else {
        checkAnswer(true);
      }
    }

    else if (yellowState == LOW && lastYellowState == HIGH) {

      lastPress = millis();

      if (gameFinished) {
        startGame();
      } else {
        checkAnswer(false);
      }
    }
  }

  lastRedState = redState;
  lastYellowState = yellowState;
}
