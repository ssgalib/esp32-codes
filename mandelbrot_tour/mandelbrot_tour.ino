#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <algorithm>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define SDA_PIN 23
#define SCL_PIN 21

// Q6.57 fixed point: 6 integer bits covers |z| up to ~6.5 before the escape test.
#define FRAC 57
#define ZOOM_PER_FRAME 1.2
#define HOLD_MS 3000
#define MAX_FRAME_MS 1500  // past this the zoom gets too slow, move on

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

struct Target {
  const char *name;
  double cx, cy, maxZoom;
};

// Each checked on the laptop to still show detail at maxZoom.
const Target targets[] = {
  {"Seahorse", -0.7436438870371587, 0.1318259042053120, 1e5},  // deeper needs stage 2 (perturbation)
  {"Misiurewicz", -0.10109636384562, 0.95628651080914, 1e12},
  {"Minibrot", -1.7548776662466927, 0.0, 60},
  {"c = i", 0.0, 1.0, 1e12},
  {"Spiral", -0.761574, -0.0847596, 1e5},
};
const int NUM_TARGETS = sizeof targets / sizeof targets[0];

struct View {
  int64_t left, top, step;
  int maxIter;
};

View view;
float vals[SCREEN_HEIGHT][SCREEN_WIDTH];  // smooth escape value, -1 = inside the set
float sorted[SCREEN_WIDTH * SCREEN_HEIGHT];
SemaphoreHandle_t startSem, doneSem;

const uint8_t bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

int64_t toFix(double d) { return (int64_t)llround(ldexp(d, FRAC)); }

// (a * b) >> FRAC, built from 32-bit products since Xtensa has no 128-bit multiply.
static inline int64_t mulfix(int64_t a, int64_t b) {
  bool neg = (a < 0) != (b < 0);
  uint64_t ua = a < 0 ? -(uint64_t)a : a, ub = b < 0 ? -(uint64_t)b : b;
  uint32_t a0 = ua, a1 = ua >> 32, b0 = ub, b1 = ub >> 32;
  uint64_t p00 = (uint64_t)a0 * b0, p01 = (uint64_t)a0 * b1;
  uint64_t p10 = (uint64_t)a1 * b0, p11 = (uint64_t)a1 * b1;
  uint64_t mid = (p00 >> 32) + (uint32_t)p01 + (uint32_t)p10;
  uint64_t hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
  uint64_t lo = (mid << 32) | (uint32_t)p00;
  uint64_t r = (hi << (64 - FRAC)) | (lo >> FRAC);
  return neg ? -(int64_t)r : (int64_t)r;
}

// Returns the escape iteration, or maxIter if inside. mag2 gets |z|^2 at escape.
// eps: how close a revisit must be to count as a cycle (rounding rarely repeats exactly).
int iterate(int64_t cx, int64_t cy, int maxIter, int64_t eps, float &mag2) {
  const uint64_t BAILOUT = 4ULL << FRAC;
  int64_t x = 0, y = 0, rx = 0, ry = 0;
  int checkAt = 8;
  for (int n = 0; n < maxIter; n++) {
    int64_t x2 = mulfix(x, x), y2 = mulfix(y, y);
    if ((uint64_t)x2 + (uint64_t)y2 > BAILOUT) {
      mag2 = ldexpf((float)x2 + (float)y2, -FRAC);
      return n;
    }
    y = 2 * mulfix(x, y) + cy;
    x = x2 - y2 + cx;
    // Brent-style cycle check: an orbit that returns to a saved point is inside the set.
    if (llabs(x - rx) <= eps && llabs(y - ry) <= eps) return maxIter;
    if (n == checkAt) { rx = x; ry = y; checkAt *= 2; }
  }
  return maxIter;
}

void renderRows(int firstRow) {
  for (int py = firstRow; py < SCREEN_HEIGHT; py += 2) {
    vTaskDelay(1);  // lets core 0's idle task run so the task watchdog stays quiet
    int64_t cy = view.top + py * view.step;
    for (int px = 0; px < SCREEN_WIDTH; px++) {
      float mag2;
      int n = iterate(view.left + px * view.step, cy, view.maxIter, view.step >> 4, mag2);
      vals[py][px] = n == view.maxIter ? -1 : n + 1 - log2f(0.5f * log2f(mag2));
    }
  }
}

// Core 0 renders odd rows while loop() on core 1 renders even rows.
void worker(void *) {
  for (;;) {
    xSemaphoreTake(startSem, portMAX_DELAY);
    renderRows(1);
    xSemaphoreGive(doneSem);
  }
}

void setView(const Target &t, double zoom) {
  double step = 3.0 / (zoom * SCREEN_WIDTH);
  view.step = toFix(step);
  view.left = toFix(t.cx) - (SCREEN_WIDTH / 2) * view.step;
  view.top = toFix(t.cy) - (SCREEN_HEIGHT / 2) * view.step;
  view.maxIter = 200 + 600 * log10(zoom);  // deep views need thousands, see check_targets
}

// Histogram-equalize the escape values, then Bayer-dither to 1 bit.
void shade() {
  int n = 0;
  for (int y = 0; y < SCREEN_HEIGHT; y++)
    for (int x = 0; x < SCREEN_WIDTH; x++)
      if (vals[y][x] >= 0) sorted[n++] = vals[y][x];
  std::sort(sorted, sorted + n);

  display.clearDisplay();
  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      if (vals[y][x] < 0) continue;
      float gray = (float)(std::upper_bound(sorted, sorted + n, vals[y][x]) - sorted) / n;
      if (gray * 16 > bayer[y & 3][x & 3] + 0.5f) display.drawPixel(x, y, SSD1306_WHITE);
    }
  }
}

void drawLabel(const char *text) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 56, &x1, &y1, &w, &h);
  display.fillRect(0, 55, w + 2, 9, SSD1306_BLACK);
  display.setCursor(1, 56);
  display.print(text);
}

// Fixed-point vs plain double at a shallow view; counts should almost always agree.
bool selfTest() {
  Target t = {"test", -0.6, 0.0, 1};
  setView(t, 1);
  view.maxIter = 100;
  double step = 3.0 / SCREEN_WIDTH;
  int match = 0;
  for (int py = 0; py < SCREEN_HEIGHT; py++) {
    for (int px = 0; px < SCREEN_WIDTH; px++) {
      float mag2;
      int nf = iterate(view.left + px * view.step, view.top + py * view.step, 100, view.step >> 4, mag2);
      double cx = -0.6 + (px - SCREEN_WIDTH / 2) * step, cy = (py - SCREEN_HEIGHT / 2) * step;
      double x = 0, y = 0;
      int nd = 0;
      while (nd < 100 && x * x + y * y <= 4) {
        double xt = x * x - y * y + cx;
        y = 2 * x * y + cy;
        x = xt;
        nd++;
      }
      match += nd == nf;
    }
  }
  Serial.printf("Mandelbrot self-test: %s (%d/%d pixels match)\n",
                match >= SCREEN_WIDTH * SCREEN_HEIGHT * 99 / 100 ? "OK" : "FAIL",
                match, SCREEN_WIDTH * SCREEN_HEIGHT);
  return match >= SCREEN_WIDTH * SCREEN_HEIGHT * 99 / 100;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 not found");
    while (true);
  }
  display.setTextColor(SSD1306_WHITE);

  selfTest();

  startSem = xSemaphoreCreateBinary();
  doneSem = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(worker, "render", 4096, NULL, 1, NULL, 0);
}

void loop() {
  static int target = 0;
  static double zoom = 1;
  const Target &t = targets[target];

  unsigned long start = millis();
  setView(t, zoom);
  xSemaphoreGive(startSem);
  renderRows(0);
  xSemaphoreTake(doneSem, portMAX_DELAY);

  shade();
  char label[24];
  snprintf(label, sizeof label, "1e%d", (int)floor(log10(zoom)));
  drawLabel(zoom < 10 ? t.name : label);
  display.display();
  Serial.printf("%s zoom %.1e iter %d frame %lu ms\n", t.name, zoom, view.maxIter, millis() - start);

  bool tooSlow = millis() - start > MAX_FRAME_MS;
  zoom *= ZOOM_PER_FRAME;
  if (zoom > t.maxZoom || tooSlow) {
    delay(HOLD_MS);
    target = (target + 1) % NUM_TARGETS;
    zoom = 1;
  }
}
