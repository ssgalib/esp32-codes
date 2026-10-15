#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define SDA_PIN 23
#define SCL_PIN 21

#define CELL 2  // each cell is a 2x2 pixel block
#define COLS (SCREEN_WIDTH / CELL)
#define ROWS (SCREEN_HEIGHT / CELL)
#define ALIVE_PERCENT 30
#define MAX_GENERATIONS 1000
#define GEN_MS 100  // ~10 generations per second

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

typedef uint8_t Grid[ROWS][COLS];

Grid cur, next, prev;
int generation = 0;

// One Conway step with wrap-around edges.
void step(Grid in, Grid out) {
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      int n = 0;
      for (int dr = -1; dr <= 1; dr++)
        for (int dc = -1; dc <= 1; dc++)
          if (dr || dc) n += in[(r + dr + ROWS) % ROWS][(c + dc + COLS) % COLS];
      out[r][c] = n == 3 || (n == 2 && in[r][c]);
    }
  }
}

void seed() {
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      cur[r][c] = random(100) < ALIVE_PERCENT;
  memset(prev, 0, sizeof prev);
  generation = 0;
}

// A vertical blinker must become horizontal after one step.
bool selfTest() {
  memset(cur, 0, sizeof cur);
  int r = ROWS / 2, c = COLS / 2;
  cur[r - 1][c] = cur[r][c] = cur[r + 1][c] = 1;
  step(cur, next);

  int population = 0;
  for (int i = 0; i < ROWS; i++)
    for (int j = 0; j < COLS; j++) population += next[i][j];
  return population == 3 && next[r][c - 1] && next[r][c] && next[r][c + 1];
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 not found");
    while (true);
  }

  Serial.println(selfTest() ? "Life self-test: OK" : "Life self-test: FAIL");
  randomSeed(esp_random());
  seed();
}

void loop() {
  unsigned long start = millis();

  display.clearDisplay();
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      if (cur[r][c]) display.fillRect(c * CELL, r * CELL, CELL, CELL, SSD1306_WHITE);
  display.display();

  step(cur, next);
  generation++;

  // Still life, period-2 oscillator, or ran too long: start over.
  if (!memcmp(next, cur, sizeof cur) || !memcmp(next, prev, sizeof prev) ||
      generation >= MAX_GENERATIONS) {
    Serial.printf("Reseed after %d generations\n", generation);
    delay(1000);  // let the final pattern show briefly
    seed();
  } else {
    memcpy(prev, cur, sizeof cur);
    memcpy(cur, next, sizeof next);
  }

  unsigned long spent = millis() - start;
  if (spent < GEN_MS) delay(GEN_MS - spent);
}
