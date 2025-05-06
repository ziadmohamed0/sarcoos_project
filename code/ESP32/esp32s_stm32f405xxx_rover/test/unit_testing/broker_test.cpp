/**
 * @file main.cpp
 * @author Ziad Mohammed Fathy
 * @brief ESP32 code to connect with STM32F405RGT6 
 *        to add WiFi capability for STM32 to control robot rover
 * @version 0.1
 * @date 2025-05-02
 * 
 * @copyright Copyright (c) 2025
 */

 #include <Arduino.h>
 #include <WiFi.h>
 #include <PubSubClient.h>
 
 /* mqtt broker initialization */
 const char* ssid = "Mohamed Fathy";
 const char* password = "341978341978";
 const char* mqtt_server = "192.168.100.102";
 const int mqtt_port = 1883;
 
 /* topics */
 const char* topic_sub = "pi/to/esp";
 const char* topic_pub = "esp/to/pi";
 
 /* initialization client and publisher */
 WiFiClient espClient;
 PubSubClient client(espClient);
 
 unsigned long lastMsg = 0;
 #define MSG_BUFFER_SIZE 50
 char msg[MSG_BUFFER_SIZE];
 int value = 0;
 
 /**
  * @brief Set the up wifi object
  * 
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
 
   Serial.println("");
   Serial.println("WiFi connected");
   Serial.println("IP address: ");
   Serial.println(WiFi.localIP());
 }
 
 /**
  * @brief 
  * 
  * @param topic 
  * @param payload 
  * @param length 
  */
 void callback(char* topic, byte* payload, unsigned int length) {
   Serial.print("Message arrived [");
   Serial.print(topic);
   Serial.print("]: ");
   
   for (int i = 0; i < length; i++) {
     Serial.print((char)payload[i]);
   }
   Serial.println();
 }
 
 /**
  * @brief 
  * 
  */
 void reconnect() {
   while (!client.connected()) {
     Serial.print("Attempting MQTT connection...");
     
     String clientId = "ESP32Client-";
     clientId += String(random(0xffff), HEX);
     
     if (client.connect(clientId.c_str())) {
       Serial.println("connected");
       client.subscribe(topic_sub);
     } else {
       Serial.print("failed, rc=");
       Serial.print(client.state());
       Serial.println(" try again in 5 seconds");
       delay(5000);
     }
   }
 }
 
 void setup() {
   Serial.begin(115200);
   setup_wifi();
   client.setServer(mqtt_server, mqtt_port);
   client.setCallback(callback);
   randomSeed(micros());
 }
 
 void loop() {
   if (!client.connected()) {
     reconnect();
   }
   client.loop();
 
   unsigned long now = millis();
   if (now - lastMsg > 2000) {
     lastMsg = now;
     ++value;
     
     String msg = "Hello from ESP32 #" + String(value);
     Serial.print("Publish message: ");
     Serial.println(msg);
     client.publish(topic_pub, msg.c_str());
   }
 }