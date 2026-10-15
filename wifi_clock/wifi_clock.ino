#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include "secrets.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define SDA_PIN 23
#define SCL_PIN 21

#define NTP_SERVER "pool.ntp.org"
#define TZ_RECHECK_MS (24UL * 60 * 60 * 1000)  // daily, catches DST changes
#define TZ_RETRY_MS (60UL * 1000)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

String tzName = "UTC";
unsigned long lastTzCheck = 0;
unsigned long tzInterval = 0;

void printCentered(const char *text, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, y);
  display.print(text);
}

void showMessage(const char *msg) {
  display.clearDisplay();
  display.setTextSize(1);
  printCentered(msg, 28);
  display.display();
}

// Ask ip-api.com for this network's timezone; offset already includes DST.
bool lookupTimezone() {
  HTTPClient http;
  http.begin("http://ip-api.com/json?fields=timezone,offset");
  String body = http.GET() == 200 ? http.getString() : "";
  http.end();

  int t = body.indexOf("\"timezone\":\"");
  int o = body.indexOf("\"offset\":");
  if (t < 0 || o < 0) return false;

  t += 12;
  tzName = body.substring(t, body.indexOf('"', t));
  configTime(body.substring(o + 9).toInt(), 0, NTP_SERVER);
  return true;
}

void updateTimezone() {
  if (lookupTimezone()) {
    tzInterval = TZ_RECHECK_MS;
  } else {
    tzName = "UTC";  // shown on screen so a wrong time is never silent
    configTime(0, 0, NTP_SERVER);
    tzInterval = TZ_RETRY_MS;
  }
  lastTzCheck = millis();
  Serial.println("Timezone: " + tzName);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 not found");
    while (true);
  }
  display.setTextColor(SSD1306_WHITE);

  showMessage("Connecting WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (int i = 0; WiFi.status() != WL_CONNECTED; i++) {
    if (i == 40) {  // 20 s
      showMessage("WiFi failed");
      delay(10000);
      ESP.restart();
    }
    delay(500);
  }

  showMessage("Finding timezone...");
  updateTimezone();
}

void loop() {
  if (millis() - lastTzCheck >= tzInterval) updateTimezone();

  struct tm now;
  if (!getLocalTime(&now, 0)) {
    showMessage("Syncing time...");
    delay(200);
    return;
  }

  char hm[6], sec[3], ampm[3], date[20];
  strftime(hm, sizeof hm, "%I:%M", &now);
  strftime(sec, sizeof sec, "%S", &now);
  strftime(ampm, sizeof ampm, "%p", &now);
  strftime(date, sizeof date, "%a, %d %b %Y", &now);

  display.clearDisplay();

  display.setTextSize(3);
  display.setCursor(0, 4);
  display.print(hm);

  display.setTextSize(2);
  display.setCursor(100, 4);
  display.print(ampm);
  display.setTextSize(1);
  display.setCursor(106, 22);
  display.print(sec);

  printCentered(date, 38);
  printCentered(tzName.c_str(), 54);

  display.display();
  delay(200);
}
