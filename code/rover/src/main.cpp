#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define m1 D1
#define m2 D2
#define m3 D3
#define m4 D4
#define m5 D5
#define m6 D6
#define m7 D7
#define m8 D8

const char* ssid = "Mohamed Fathy";
const char* password = "341978341978";
const char* mqtt_server = "192.168.100.25";

WiFiClient espClient;
PubSubClient client(espClient);

String lastForward = "1";
String lastReverse = "1";
String lastRight = "1";
String lastLeft = "1";

void stopMotors() {
  digitalWrite(m1, LOW); 
  digitalWrite(m2, LOW);
  digitalWrite(m3, LOW); 
  digitalWrite(m4, LOW);
  digitalWrite(m5, LOW); 
  digitalWrite(m6, LOW);
  digitalWrite(m7, LOW); 
  digitalWrite(m8, LOW);
}

void forward() {
  digitalWrite(m1, HIGH); 
  digitalWrite(m2, LOW);
  digitalWrite(m3, HIGH); 
  digitalWrite(m4, LOW);
  digitalWrite(m5, HIGH); 
  digitalWrite(m6, LOW);
  digitalWrite(m7, HIGH); 
  digitalWrite(m8, LOW);
}

void reverse() {
  digitalWrite(m1, LOW); 
  digitalWrite(m2, HIGH);
  digitalWrite(m3, LOW); 
  digitalWrite(m4, HIGH);
  digitalWrite(m5, LOW); 
  digitalWrite(m6, HIGH);
  digitalWrite(m7, LOW); 
  digitalWrite(m8, HIGH);
}

void right() {
  digitalWrite(m1, HIGH); 
  digitalWrite(m2, LOW);
  digitalWrite(m3, HIGH); 
  digitalWrite(m4, LOW);
  digitalWrite(m5, LOW);  
  digitalWrite(m6, HIGH);
  digitalWrite(m7, LOW);  
  digitalWrite(m8, HIGH);
}

void left() {
  digitalWrite(m1, LOW);  
  digitalWrite(m2, HIGH);
  digitalWrite(m3, LOW);  
  digitalWrite(m4, HIGH);
  digitalWrite(m5, HIGH); 
  digitalWrite(m6, LOW);
  digitalWrite(m7, HIGH); 
  digitalWrite(m8, LOW);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) 
    message += (char)payload[i];

  String t = String(topic);
  Serial.print("Topic: "); 
  Serial.println(t);
  Serial.print("Payload: "); 
  Serial.println(message);

  if (t == "button/forward") {
    if (message == "0" && lastForward != "0") 
      forward();
    else if (message == "1" && lastForward != "1") 
      stopMotors();
    lastForward = message;
  }

  else if (t == "button/reverse") {
    if (message == "0" && lastReverse != "0") 
      reverse();
    else if (message == "1" && lastReverse != "1") 
      stopMotors();
    lastReverse = message;
  }

  else if (t == "button/right") {
    if (message == "0" && lastRight != "0") right();
    else if (message == "1" && lastRight != "1") stopMotors();
    lastRight = message;
  }

  else if (t == "button/left") {
    if (message == "0" && lastLeft != "0") 
      left();
    else if (message == "1" && lastLeft != "1") 
      stopMotors();
    lastLeft = message;
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      client.subscribe("button/forward");
      client.subscribe("button/reverse");
      client.subscribe("button/right");
      client.subscribe("button/left");
    } 
    
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(m1, OUTPUT); 
  pinMode(m2, OUTPUT);
  pinMode(m3, OUTPUT); 
  pinMode(m4, OUTPUT);
  pinMode(m5, OUTPUT); 
  pinMode(m6, OUTPUT);
  pinMode(m7, OUTPUT); 
  pinMode(m8, OUTPUT);
  stopMotors();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();
}
