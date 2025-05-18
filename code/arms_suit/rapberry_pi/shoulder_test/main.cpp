#include <iostream>
#include <string>
#include <vector>
#include <pigpio.h>
#include <mosquitto.h>
#include <json/json.h> // تحتاج لتثبيت مكتبة jsoncpp

#define SERVO1_PIN 18
#define SERVO2_PIN 19
#define MQTT_HOST "localhost"
#define MQTT_PORT 1883
#define MQTT_TOPIC "arms_suit/potentiometers"
#define MQTT_QOS 1 // ضمان وصول الرسائل

struct MotorState {
    int angle1;
    int angle2;
};

void setServoAngle(int pin, int angle) {
    angle = std::max(0, std::min(180, angle));
    int pulseWidth = 500 + (angle * 2000 / 180);
    if (gpioServo(pin, pulseWidth) != 0) {
        std::cerr << "Error setting servo on GPIO " << pin << std::endl;
    }
}

void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg) {
    MotorState *state = (MotorState *)obj;
    
    if (!msg->payload || msg->payloadlen == 0) {
        std::cerr << "Empty message received" << std::endl;
        return;
    }

    try {
        std::string payload(static_cast<char*>(msg->payload), msg->payloadlen);
        Json::Value root;
        Json::CharReaderBuilder builder;
        std::istringstream iss(payload);
        std::string parseErrors;
        
        if (!Json::parseFromStream(builder, iss, &root, &parseErrors)) {
            throw std::runtime_error("JSON parse error: " + parseErrors);
        }

        // التحقق من وجود الحقول المطلوبة
        if (!root.isMember("pot50k") || !root.isMember("pot100k")) {
            throw std::runtime_error("Missing required fields in JSON");
        }

        int pot50k = root["pot50k"].asInt();
        int pot100k = root["pot100k"].asInt();

        state->angle1 = (pot50k * 180) / 4095;
        state->angle2 = (pot100k * 180) / 4095;

        setServoAngle(SERVO1_PIN, state->angle1);
        setServoAngle(SERVO2_PIN, state->angle2);

        std::cout << "Motor Angles - 50k: " << state->angle1 
                  << "°, 100k: " << state->angle2 << "°" << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "Message processing error: " << e.what() 
                  << " | Raw payload: " 
                  << std::string(static_cast<char*>(msg->payload), msg->payloadlen)
                  << std::endl;
    }
}

int main() {
    if (gpioInitialise() < 0) {
        std::cerr << "GPIO initialization failed" << std::endl;
        return 1;
    }

    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new("rpi_controller", true, nullptr);
    
    if (!mosq) {
        std::cerr << "MQTT client creation failed" << std::endl;
        gpioTerminate();
        return 1;
    }

    MotorState state = {90, 90};
    mosquitto_user_data_set(mosq, &state);
    mosquitto_message_callback_set(mosq, on_message);
    mosquitto_max_inflight_messages_set(mosq, 10); // تحسين أداء MQTT

    // إعداد اتصال آمن (اختياري)
    mosquitto_tls_opts_set(mosq, 1, NULL, NULL);
    
    int connection_timeout = 10; // ثواني
    while (true) {
        int rc = mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, connection_timeout);
        if (rc == MOSQ_ERR_SUCCESS) break;
        
        std::cerr << "Connection failed: " << mosquitto_strerror(rc) 
                  << ". Retrying in 5 seconds..." << std::endl;
        sleep(5);
    }

    if (mosquitto_subscribe(mosq, NULL, MQTT_TOPIC, MQTT_QOS) != MOSQ_ERR_SUCCESS) {
        std::cerr << "Subscription failed" << std::endl;
        mosquitto_destroy(mosq);
        gpioTerminate();
        return 1;
    }

    std::cout << "Successfully connected to MQTT broker and subscribed to topic" << std::endl;

    try {
        while (true) {
            int rc = mosquitto_loop(mosq, 1000, 1);
            if (rc) {
                std::cerr << "Connection error: " << mosquitto_strerror(rc) << std::endl;
                mosquitto_reconnect(mosq);
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
    }

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    gpioTerminate();
    return 0;
}
