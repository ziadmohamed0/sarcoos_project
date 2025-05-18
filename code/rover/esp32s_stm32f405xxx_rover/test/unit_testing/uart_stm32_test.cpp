#include <Arduino.h>

#define STM32_RX_PIN 16  // GPIO16 (RX2)
#define STM32_TX_PIN 17  // GPIO17 (TX2)

HardwareSerial stm32Serial(2);  // Use UART2

void setup() {
  Serial.begin(115200);  // Debug Monitor
  stm32Serial.begin(115200, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);

  Serial.println("ESP32 is ready to receive from STM32...");
}

void loop() {
  if (stm32Serial.available()) {
    String msg = stm32Serial.readStringUntil('\n');
    Serial.print("Received from STM32: ");
    Serial.println(msg);
  }
}
