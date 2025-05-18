#include <iostream>
#include <string>
#include <vector>
#include <pigpio.h>
#include <mosquitto.h>

// إعدادات GPIO للسيرفو موتورز
#define SERVO1_PIN 18
#define SERVO2_PIN 19

// إعدادات MQTT
#define MQTT_HOST "localhost"
#define MQTT_PORT 1883
#define MQTT_TOPIC "arms_suit/potentiometers"

// هيكل لحالة المواتير
struct MotorState {
    int angle1;
    int angle2;
};

// دالة لتحريك السيرفو موتور
void setServoAngle(int pin, int angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    
    int pulseWidth = 500 + (angle * 2000 / 180); // 500-2500μs
    gpioServo(pin, pulseWidth);
}

// دالة معالجة الرسائل الواردة من MQTT
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
    MotorState *state = (MotorState *)obj;
    
    try {
        std::string payload((char *)message->payload, message->payloadlen);
        size_t comma_pos = payload.find(',');
        
        if (comma_pos != std::string::npos) {
            int pot50k = std::stoi(payload.substr(0, comma_pos));
            int pot100k = std::stoi(payload.substr(comma_pos + 1));
            
            // تحويل القيم إلى زوايا (0-180)
            state->angle1 = (pot50k * 180) / 4095;  // ESP32 ADC 12-bit (0-4095)
            state->angle2 = (pot100k * 180) / 4095;
            
            // تحريك المواتير
            setServoAngle(SERVO1_PIN, state->angle1);
            setServoAngle(SERVO2_PIN, state->angle2);
            
            std::cout << "Motor1: " << state->angle1 << "°, Motor2: " << state->angle2 << "°" << std::endl;
        }
    } catch (...) {
        std::cerr << "Error processing message" << std::endl;
    }
}

int main() {
    // إعداد pigpio
    if (gpioInitialise() < 0) {
        std::cerr << "Failed to initialize pigpio" << std::endl;
        return 1;
    }

    // إعداد MQTT
    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new("rpi_controller", true, nullptr);
    
    if (!mosq) {
        std::cerr << "Failed to create MQTT client" << std::endl;
        gpioTerminate();
        return 1;
    }

    MotorState state = {90, 90}; // حالة ابتدائية (90 درجة)
    mosquitto_userdata_set(mosq, &state);
    mosquitto_message_callback_set(mosq, on_message);

    if (mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, 60) != MOSQ_ERR_SUCCESS) {
        std::cerr << "Failed to connect to MQTT broker" << std::endl;
        mosquitto_destroy(mosq);
        gpioTerminate();
        return 1;
    }

    if (mosquitto_subscribe(mosq, nullptr, MQTT_TOPIC, 0) != MOSQ_ERR_SUCCESS) {
        std::cerr << "Failed to subscribe to topic" << std::endl;
        mosquitto_destroy(mosq);
        gpioTerminate();
        return 1;
    }

    std::cout << "Controller started. Waiting for messages..." << std::endl;

    // الحلقة الرئيسية
    while (true) {
        int rc = mosquitto_loop(mosq, -1, 1);
        if (rc) {
            mosquitto_reconnect(mosq);
        }
    }

    // التنظيف (لن يتم تنفيذها عادةً)
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    gpioTerminate();
    return 0;
}
