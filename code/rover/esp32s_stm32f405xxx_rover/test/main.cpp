/**
 * @file mqtt_uart_bridge.cpp
 * @author Ziad Mohammed Fathy
 * @brief 
 * ESP32 MQTT to UART bridge for STM32 robot control.
 * 
 * This program connects ESP32 to WiFi and a local MQTT broker,
 * listens on the topic "robot/control", and sends the command
 * via UART2 to an STM32. Responses from STM32 are published
 * on "robot/response" topic.
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
#include <WiFi.h>
#include <PubSubClient.h>

/* ---------- WiFi credentials ---------- */
const char* ssid = "Mohamed Fathy";
const char* password = "341978341978";

/* ---------- MQTT broker IP (Raspberry Pi) ---------- */
const char* mqtt_server = "192.168.100.102";

/* ---------- Client declaration ---------- */
WiFiClient espClient;
PubSubClient client(espClient);

/* ---------- UART2 pin mapping ---------- */
#define RXD2 16  /**< GPIO16 as RX → STM32 TX (PA2) */
#define TXD2 17  /**< GPIO17 as TX → STM32 RX (PA3) */

/**
 * @brief MQTT callback function.
 * Receives message from topic and sends it to STM32.
 * 
 * @param topic Topic name
 * @param payload Message payload
 * @param length Length of payload
 */
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  Serial.print("MQTT Received [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(msg);

  Serial2.println(msg);
}

/**
 * @brief Connects ESP32 to WiFi.
 */
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

/**
 * @brief Reconnects to MQTT broker and subscribes to topic.
 */
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
      client.subscribe("robot/control");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      delay(2000);
    }
  }
}

/**
 * @brief Initializes UART2, WiFi, and MQTT.
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

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

/**
 * @brief Main loop:
 * - Handles MQTT reconnect and message loop
 * - Relays data between STM32 and MQTT broker
 */
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

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

    client.publish("robot/response", response.c_str());
  }
}
