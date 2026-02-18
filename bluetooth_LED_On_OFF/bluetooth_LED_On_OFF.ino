//This example code is in the Public Domain (or CC0 licensed, at your option.)
//By Evandro Copercini - 2018
//
//This example creates a bridge between Serial and Classical Bluetooth (SPP)
//and also demonstrate that SerialBT have the same functionalities of a normal Serial

#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;

#define LED_NUM 2
#define BT_DEVICE_NAME "ESP32test"

void setup() {
  pinMode(LED_NUM, OUTPUT);

  Serial.begin(2000000);
  Serial.println("Hello");
  SerialBT.begin(BT_DEVICE_NAME);
  Serial.println("The device started, now you can pair it with bluetooth!");
}

void loop() {
  if (Serial.available()) {
    SerialBT.write(Serial.read());
  }
  if (SerialBT.available()) {
    String msg = SerialBT.readString();
    Serial.write(msg.c_str());

    msg.trim();

    if(msg == "LED-ON"){
      Serial.write("turning on the LED");
      digitalWrite(LED_NUM, HIGH);
    } else if(msg == "LED-OFF") {
      Serial.write("turning off the LED");
      digitalWrite(LED_NUM, LOW);
    }
  }
  delay(20);
}