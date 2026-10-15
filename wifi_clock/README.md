# WiFi Clock

ESP32 clock on a 128×64 SSD1306 OLED. It gets the time from NTP and works out its own timezone from your IP address, so nothing timezone-related is hardcoded.

![WiFi clock running on an ESP32 and OLED](esp32-clock.jpg)

## Wiring

| OLED | ESP32 |
|------|-------|
| GND  | GND   |
| VCC  | 3V3   |
| SCL  | GPIO 21 |
| SDA  | GPIO 23 |

## Setup

1. Install the **Adafruit SSD1306** and **Adafruit GFX** libraries.
2. Create `secrets.h` next to the sketch (it is gitignored):
   ```c
   #define WIFI_SSID "your-network"
   #define WIFI_PASS "your-password"
   ```
   The ESP32 only supports 2.4 GHz WiFi.
3. Upload:
   ```sh
   arduino-cli compile --fqbn esp32:esp32:esp32 -u -p /dev/ttyUSB0 wifi_clock
   ```

## How it works

- Timezone and UTC offset come from `http://ip-api.com/json` at boot and once a day after that. The offset includes daylight saving.
- If the lookup fails, the clock shows `UTC` and retries every minute.
- If WiFi doesn't connect within 20 seconds, it shows `WiFi failed` and restarts.
- Behind a VPN, the clock uses the VPN server's timezone.
