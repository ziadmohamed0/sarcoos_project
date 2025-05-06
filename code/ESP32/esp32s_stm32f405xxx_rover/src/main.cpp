/**
 * @file uart_bridge.cpp
 * @author Ziad Mohammed Fathy
 * @brief 
 * UART bridge between ESP32 and STM32.
 * 
 * This code allows bidirectional communication between
 * the ESP32 and STM32 using UART2 on the ESP32.
 * It reads data from the Serial Monitor and sends it
 * to the STM32, and displays STM32 responses on the monitor.
 * 
 * TXD2 (GPIO 17) → RX on STM32 (PA3)  
 * RXD2 (GPIO 16) ← TX on STM32 (PA2)
 * 
 * @version 0.1
 * @date 2025-05-06
 * 
 * @copyright Copyright (c) 2025
 */

/* ---------- Includes section ---------- */
#include <Arduino.h>

/* ---------- UART pins for ESP32 UART2 ---------- */
#define RXD2 16  /**< GPIO16 as RX -> connected to STM32 TX (PA2) */
#define TXD2 17  /**< GPIO17 as TX -> connected to STM32 RX (PA3) */

/**
 * @brief Setup function initializes Serial ports and prints command list.
 */
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

/**
 * @brief Loop function handles data relay:
 * - ESP32 Serial input is forwarded to STM32
 * - STM32 responses are printed on Serial Monitor
 */
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
