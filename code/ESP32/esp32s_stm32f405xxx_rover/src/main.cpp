#include <Arduino.h>

#define RXD2 16  // RX -> TX stm32 (PA2)
#define TXD2 17  // TX -> RX stm32 (PA3)

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 UART Bridge Ready");
  
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  
  Serial.println("Commands:");
  Serial.println("1: robot move forward");
  Serial.println("2: robot move reverse");
  Serial.println("3: robot turn left");
  Serial.println("4: robot turn right");
  Serial.println("5: robot stop");
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    Serial2.write(c);
    Serial.print("Sent to STM32: ");
    Serial.println(c);
  }
  
  if (Serial2.available()) {
    String response = Serial2.readStringUntil('\n');
    Serial.print("STM32 Response: ");
    Serial.println(response);
  }
}