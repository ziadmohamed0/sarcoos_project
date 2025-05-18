#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// إعدادات WiFi
const char* ssid = "Mohamed Fathy";          // استبدل باسم شبكتك
const char* password = "341978341978";  // استبدل بكلمة المرور

// إعدادات MQTT
const char* mqtt_server = "192.168.100.102"; // استبدل بعنوان الرسبري باي
const int mqtt_port = 1883;
const char* mqtt_topic = "arms_suit/potentiometers";

// تعريف منافذ البوتينشوميترات
const int pot50kPin = 34;   // GPIO34 (ADC1_CH6)
const int pot100kPin = 35;  // GPIO35 (ADC1_CH7)

WiFiClient espClient;
PubSubClient client(espClient);

// فلتر للقيم لتجنب التقلبات
class SmoothingFilter {
private:
    float alpha;
    float filteredValue;
public:
    SmoothingFilter(float alpha = 0.3) : alpha(alpha), filteredValue(0) {}
    
    float filter(float newValue) {
        filteredValue = alpha * newValue + (1 - alpha) * filteredValue;
        return filteredValue;
    }
};

SmoothingFilter filter50k(0.2);
SmoothingFilter filter100k(0.2);

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

void reconnect() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        
        if (client.connect("ESP32PotentiometerClient")) {
            Serial.println("connected");
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
    
    // إعداد منافذ ADC
    analogReadResolution(12);  // استخدام دقة 12-bit (0-4095)
    analogSetAttenuation(ADC_11db);  // نطاق 0-3.3V
    
    setup_wifi();
    client.setServer(mqtt_server, mqtt_port);
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    // قراءة القيم مع تطبيق الفلتر
    int raw50k = analogRead(pot50kPin);
    int raw100k = analogRead(pot100kPin);
    
    float filtered50k = filter50k.filter(raw50k);
    float filtered100k = filter100k.filter(raw100k);

    // إنشاء رسالة JSON
    String payload = "{\"pot50k\":" + String((int)filtered50k) + 
                    ",\"pot100k\":" + String((int)filtered100k) + "}";

    // إرسال البيانات
    if (client.publish(mqtt_topic, payload.c_str())) {
        Serial.println("Message published:");
        Serial.println(payload);
    } else {
        Serial.println("Message publish failed");
    }

    delay(100);  // تأخير 100ms بين القراءات
}