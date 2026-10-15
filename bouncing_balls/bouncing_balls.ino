#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define SDA_PIN 23
#define SCL_PIN 21

#define NUM_BALLS 6
#define FRAME_MS 16  // ~60 fps target

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

struct Ball {
  float x, y, vx, vy;
  int r;
};

Ball balls[NUM_BALLS];

float randomSpeed() {
  float s = random(5, 20) / 10.0;  // 0.5–2.0 px per frame
  return random(2) ? s : -s;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 not found");
    while (true);
  }

  randomSeed(esp_random());
  for (Ball &b : balls) {
    b.r = random(2, 6);
    b.x = random(b.r, SCREEN_WIDTH - b.r);
    b.y = random(b.r, SCREEN_HEIGHT - b.r);
    b.vx = randomSpeed();
    b.vy = randomSpeed();
  }
}

// Flip direction and clamp back inside so a ball never sticks in a wall.
void bounce(float &pos, float &vel, int r, int limit) {
  if (pos < r) { pos = r; vel = -vel; }
  if (pos > limit - 1 - r) { pos = limit - 1 - r; vel = -vel; }
}

void loop() {
  static unsigned long lastFps = millis();
  static int frames = 0;
  unsigned long start = millis();

  display.clearDisplay();
  for (Ball &b : balls) {
    b.x += b.vx;
    b.y += b.vy;
    bounce(b.x, b.vx, b.r, SCREEN_WIDTH);
    bounce(b.y, b.vy, b.r, SCREEN_HEIGHT);
    display.fillCircle(b.x, b.y, b.r, SSD1306_WHITE);
  }
  display.display();

  frames++;
  if (millis() - lastFps >= 5000) {
    Serial.printf("fps: %.1f\n", frames * 1000.0 / (millis() - lastFps));
    frames = 0;
    lastFps = millis();
  }

  unsigned long spent = millis() - start;
  if (spent < FRAME_MS) delay(FRAME_MS - spent);
}
