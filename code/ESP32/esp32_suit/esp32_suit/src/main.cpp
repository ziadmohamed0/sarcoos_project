/**
 * @file main.cpp
 * @author Ziad Mohammed Fathy
 * @brief 
 * This file handles reading 5 physical button states 
 * on an ESP32 board and publishes corresponding 
 * control commands to an MQTT broker running on a 
 * Raspberry Pi. Each button sends a different command 
 * (forward, reverse, left, right, stop) to the topic 
 * "robot/control" to control a robot's movement.
 * 
 * The code connects to WiFi, establishes a connection 
 * to the MQTT broker, and continuously checks for button 
 * presses, publishing messages when buttons are pressed.
 * 
 * @version 0.1
 * @date 2025-05-06
 * 
 * @copyright Copyright (c) 2025
 */

/* ---------- Includes section ---------- */
#include <WiFi.h>
#include <PubSubClient.h>

/* ---------- WiFi credentials section ---------- */
const char* ssid = "Mohamed Fathy";
const char* password = "341978341978";

/* ---------- MQTT Broker IP (Raspberry Pi) section ---------- */
const char* mqtt_server = "192.168.100.102";

/* ---------- Client objects section ---------- */
WiFiClient espClient;
PubSubClient client(espClient);

/* ---------- Define button pins section ---------- */
#define BTN1  12  /**< Forward button */
#define BTN2  13  /**< Reverse button */
#define BTN3  14  /**< Left button */
#define BTN4  27  /**< Right button */
#define BTN5  26  /**< Stop button */

/**
 * @brief Connects the ESP32 to the specified WiFi network.
 * 
 * Blocks execution until a connection is established.
 */
void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

/**
 * @brief Attempts to connect to the MQTT broker.
 * 
 * Blocks execution until the client is connected.
 */
void reconnect() {
  while (!client.connected()) {
    client.connect("ButtonClient");
  }
}

/**
 * @brief Initializes serial communication, sets button 
 * pins as input with internal pull-ups, connects to WiFi, 
 * and configures the MQTT broker connection.
 */
void setup() {
  Serial.begin(115200);

  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT_PULLUP);
  pinMode(BTN4, INPUT_PULLUP);
  pinMode(BTN5, INPUT_PULLUP);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
}

/**
 * @brief Main loop that:
 * - Maintains MQTT connection
 * - Checks button states
 * - Publishes corresponding control messages on button press
 */
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (digitalRead(BTN1) == LOW) {
    client.publish("robot/control", "1"); // Forward
    delay(300);
  }
  if (digitalRead(BTN2) == LOW) {
    client.publish("robot/control", "2"); // Reverse
    delay(300);
  }
  if (digitalRead(BTN3) == LOW) {
    client.publish("robot/control", "3"); // Left
    delay(300);
  }
  if (digitalRead(BTN4) == LOW) {
    client.publish("robot/control", "4"); // Right
    delay(300);
  }
  if (digitalRead(BTN5) == LOW) {
    client.publish("robot/control", "5"); // Stop
    delay(300);
  }
}
