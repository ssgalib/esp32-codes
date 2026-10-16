#include <DHT.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

const int IR_PIN = 4;  // obstacle sensor OUT, LOW = hand present
const int MODES = 4;

DHT dht(5, DHT22);  // data on GPIO5 (D5)
Adafruit_SSD1306 oled(128, 64, &Wire, -1);

float t = NAN, h = NAN;
int mode = 0;
unsigned long lastRead = 0, lastNear = 0;

void draw() {
  oled.clearDisplay();
  oled.setCursor(0, 0);
  if (isnan(t) || isnan(h)) {
    oled.setTextSize(1);
    oled.print("DHT22 read failed");
  } else if (mode == 0) {  // both
    oled.setTextSize(2);
    oled.printf("%.1f %cC\n\n%.1f %%", t, 247, h);
  } else if (mode == 1) {  // temperature, C and F
    oled.setTextSize(1);
    oled.println("Temperature");
    oled.setTextSize(3);
    oled.printf("%.1f%cC\n", t, 247);
    oled.setTextSize(2);
    oled.printf("%.1f %cF", t * 9 / 5 + 32, 247);
  } else if (mode == 2) {  // humidity
    oled.setTextSize(1);
    oled.println("Humidity");
    oled.setTextSize(4);
    oled.printf("%.0f%%", h);
  } else {  // feels like
    oled.setTextSize(1);
    oled.println("Feels like");
    oled.setTextSize(3);
    oled.printf("%.1f%cC", dht.computeHeatIndex(t, h, false), 247);
  }
  oled.display();
}

void setup() {
  Serial.begin(115200);
  pinMode(IR_PIN, INPUT);
  dht.begin();
  Wire.begin(23, 21);  // SDA=GPIO23, SCL=GPIO21
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("oled init failed");
  oled.setTextColor(SSD1306_WHITE);
  oled.cp437(true);  // char 247 = degree sign
}

void loop() {
  unsigned long now = millis();
  bool near = digitalRead(IR_PIN) == LOW;
  if (near) {
    if (now - lastNear > 300) {  // new swipe only after 300ms clear; sensor flickers while hand is over it
      mode = (mode + 1) % MODES;
      Serial.printf("swipe -> mode %d\n", mode);
      draw();
    }
    lastNear = now;
  }

  if (now - lastRead >= 2000) {  // DHT22 needs >=2s between reads
    lastRead = now;
    t = dht.readTemperature();
    h = dht.readHumidity();
    Serial.printf("T=%.1fC H=%.1f%% ir=%d\n", t, h, near);
    draw();
  }
}
