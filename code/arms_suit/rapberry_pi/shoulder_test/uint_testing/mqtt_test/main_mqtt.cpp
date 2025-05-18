#include <pigpio.h>
#include <thread>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <mosquitto.h>
#include <cstring>

#define SERVO1_PIN 18
#define SERVO2_PIN 19
#define MQTT_HOST "localhost"
#define MQTT_PORT 1883
#define MQTT_TOPIC "arms_suit/shoulder_status"

struct ServoStatus {
    int angle1;
    int angle2;
    const char* movement_type;
};

// دالة لنشر الحالة على MQTT
void publish_status(struct mosquitto *mosq, const ServoStatus &status) {
    char message[100];
    snprintf(message, sizeof(message), 
             "{\"servo1\":%d, \"servo2\":%d, \"movement\":\"%s\"}", 
             status.angle1, status.angle2, status.movement_type);
    
    mosquitto_publish(mosq, NULL, MQTT_TOPIC, strlen(message), message, 0, false);
}

void moveServo(int pin, int angle, struct mosquitto *mosq, ServoStatus &status) {
    int pulseWidth = 500 + (angle * 2000 / 180);
    gpioServo(pin, pulseWidth);
    
    // تحديث الحالة
    if (pin == SERVO1_PIN) status.angle1 = angle;
    else status.angle2 = angle;
}

void fastMovement(struct mosquitto *mosq) {
    ServoStatus status = {0, 0, "fast"};
    std::vector<int> angles = {0, 45, 90, 135, 180, 135, 90, 45, 0};
    
    for (int angle : angles) {
        moveServo(SERVO1_PIN, angle, mosq, status);
        moveServo(SERVO2_PIN, 180-angle, mosq, status);
        publish_status(mosq, status);
        usleep(300000); // 0.3 ثانية
    }
}

void slowMovement(struct mosquitto *mosq) {
    ServoStatus status = {0, 0, "slow"};
    std::vector<int> angles = {0, 30, 60, 90, 120, 150, 180};
    
    for (int angle : angles) {
        moveServo(SERVO1_PIN, angle, mosq, status);
        moveServo(SERVO2_PIN, angle, mosq, status);
        publish_status(mosq, status);
        usleep(800000); // 0.8 ثانية
    }
}

int main() {
    // إعداد MQTT
    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new(NULL, true, NULL);
    
    if (!mosq || mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, 60) != MOSQ_ERR_SUCCESS) {
        std::cerr << "فشل في الاتصال بـ MQTT Broker!" << std::endl;
        return 1;
    }

    // إعداد GPIO
    if (gpioInitialise() < 0) {
        std::cerr << "فشل في تهيئة pigpio!" << std::endl;
        return 1;
    }

    try {
        while (true) {
            std::cout << "الحركة السريعة..." << std::endl;
            fastMovement(mosq);
            sleep(1);
            
            std::cout << "الحركة البطيئة..." << std::endl;
            slowMovement(mosq);
            sleep(1);
        }
    } catch (...) {
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        gpioTerminate();
        return 0;
    }

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    gpioTerminate();
    return 0;
}
