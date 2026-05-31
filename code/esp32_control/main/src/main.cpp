#include "main.h"
#include "bts7960.h"
#include "servo.h"
#include "WiFi.h"
#include "MQTT.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string>
#include <cstdlib>

const char *TAG = "MAIN";

struct MotionTargets {
    int dir;
    int speed;
    float shoulder_left;
    float shoulder_right;
    float elbow_left;
    float elbow_right;
    bool emergency_stop;
};

struct MotionSmoothState {
    int dir;
    float speed;
    float shoulder_left;
    float shoulder_right;
    float elbow_left;
    float elbow_right;
};

BTS7960* g_motor1 = nullptr;
BTS7960* g_motor2 = nullptr;

Servo* g_shoulder_servo_left  = nullptr;
Servo* g_shoulder_servo_right = nullptr;
Servo* g_elbow_servo_left     = nullptr;
Servo* g_elbow_servo_right    = nullptr;

WiFiManager* wifi = nullptr;
MQTTClient*  mqtt = nullptr;

static SemaphoreHandle_t g_motion_mutex = nullptr;
static MotionTargets g_targets = {
    0, 0,
    SERVO_DEFAULT_ANGLE, SERVO_DEFAULT_ANGLE,
    SERVO_DEFAULT_ANGLE, SERVO_DEFAULT_ANGLE,
    false
};
static MotionSmoothState g_state = {
    0, 0.0f,
    SERVO_DEFAULT_ANGLE, SERVO_DEFAULT_ANGLE,
    SERVO_DEFAULT_ANGLE, SERVO_DEFAULT_ANGLE
};

// ─── helpers ────────────────────────────────────────────────────────────────

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static float slew_toward(float current, float target, float max_step) {
    if (max_step <= 0.0f) return target;
    float diff = target - current;
    if (diff >  max_step) return current + max_step;
    if (diff < -max_step) return current - max_step;
    return target;
}

static float smooth_angle(float current, float target) {
    float limited = slew_toward(current, target, SERVO_ANGLE_SLEW_STEP);
    return current + SERVO_ANGLE_FILTER_ALPHA * (limited - current);
}

static void apply_servo_angle_270(Servo* servo, float angle_deg) {
    if (!servo) return;
    angle_deg = clampf(angle_deg, 0.0f, 270.0f);
    uint32_t pulse_us = 500 + static_cast<uint32_t>((angle_deg * 2000.0f) / 270.0f);
    servo->setPulse(pulse_us);
}

// ─── FIXED: motor direction logic ───────────────────────────────────────────

static void apply_robot_motion(int dir, int speed) {
    if (speed < 0)   speed = 0;
    if (speed > 100) speed = 100;

    switch (dir) {
        case 1:  // Forward
            g_motor1->forward(speed);
            g_motor2->forward(speed);
            break;
        case 2:  // Backward
            g_motor1->backward(speed);
            g_motor2->backward(speed);
            break;
        case 3:  // Turn Left
            g_motor1->backward(speed);
            g_motor2->forward(speed);
            break;
        case 4:  // Turn Right
            g_motor1->forward(speed);
            g_motor2->backward(speed);
            break;
        case 0:
        default:
            g_motor1->stop();
            g_motor2->stop();
            break;
    }
}

// ─── motion control task ────────────────────────────────────────────────────

static void motion_control_task(void* arg) {
    (void)arg;

    while (true) {
        MotionTargets target;
        if (xSemaphoreTake(g_motion_mutex, pdMS_TO_TICKS(MOTION_CTRL_PERIOD_MS)) == pdTRUE) {
            target = g_targets;
            xSemaphoreGive(g_motion_mutex);
        } else {
            target = g_targets;
        }

        if (target.emergency_stop) {
            g_state.dir   = 0;
            g_state.speed = 0.0f;
            apply_robot_motion(0, 0);
        } else {
            float speed_target = (target.dir == 0) ? 0.0f : static_cast<float>(target.speed);

            if (target.dir != g_state.dir && g_state.speed > 1.0f) {
                g_state.speed = slew_toward(g_state.speed, 0.0f, MOTOR_SPEED_SLEW_STEP);
                if (g_state.speed <= 1.0f) {
                    g_state.dir   = target.dir;
                    g_state.speed = 0.0f;
                }
            } else {
                g_state.dir   = target.dir;
                g_state.speed = slew_toward(g_state.speed, speed_target, MOTOR_SPEED_SLEW_STEP);
            }

            apply_robot_motion(g_state.dir, static_cast<int>(g_state.speed + 0.5f));
        }

        g_state.shoulder_left  = smooth_angle(g_state.shoulder_left,  target.shoulder_left);
        g_state.shoulder_right = smooth_angle(g_state.shoulder_right, target.shoulder_right);
        g_state.elbow_left     = smooth_angle(g_state.elbow_left,     target.elbow_left);
        g_state.elbow_right    = smooth_angle(g_state.elbow_right,    target.elbow_right);

        apply_servo_angle_270(g_shoulder_servo_left,  g_state.shoulder_left);
        apply_servo_angle_270(g_shoulder_servo_right, g_state.shoulder_right);
        apply_servo_angle_270(g_elbow_servo_left,     g_state.elbow_left);
        apply_servo_angle_270(g_elbow_servo_right,    g_state.elbow_right);

        vTaskDelay(pdMS_TO_TICKS(MOTION_CTRL_PERIOD_MS));
    }
}

// ─── JSON parser ────────────────────────────────────────────────────────────

static int extract_int_field(const std::string& payload,
                              const std::string& field,
                              int default_value = -1) {
    size_t pos = payload.find(field);
    if (pos == std::string::npos) return default_value;
    pos = payload.find(':', pos);
    if (pos == std::string::npos) return default_value;
    pos++;
    while (pos < payload.size() && isspace((unsigned char)payload[pos])) pos++;
    int sign = 1;
    if (pos < payload.size() && payload[pos] == '-') { sign = -1; pos++; }
    int value = 0; bool found = false;
    while (pos < payload.size() && isdigit((unsigned char)payload[pos])) {
        found = true;
        value = value * 10 + (payload[pos++] - '0');
    }
    return found ? value * sign : default_value;
}

// ─── MQTT handlers ──────────────────────────────────────────────────────────

static void handle_movement_command(const std::string& topic, const std::string& payload) {
    int dir = extract_int_field(payload, "\"dir\"", -1);
    if (dir == -1) dir = extract_int_field(payload, "dir", -1);

    int speed = extract_int_field(payload, "\"speed\"", 0);
    if (speed == -1) speed = extract_int_field(payload, "speed", 0);

    if (dir < 0 || dir > 4) {
        ESP_LOGW(TAG, "Unknown dir value: %d in payload %s", dir, payload.c_str());
        return;
    }

    if (xSemaphoreTake(g_motion_mutex, portMAX_DELAY) == pdTRUE) {
        g_targets.emergency_stop = false;
        g_targets.dir   = dir;
        g_targets.speed = speed;
        xSemaphoreGive(g_motion_mutex);
    }

    ESP_LOGI(TAG, "Movement target dir=%d speed=%d", dir, speed);
}

static void handle_shoulder_command(const std::string& topic, const std::string& payload) {
    int side        = extract_int_field(payload, "\"side\"",  -1);
    if (side == -1) side = extract_int_field(payload, "side", -1);
    int servo_angle = extract_int_field(payload, "\"servo\"", -1);
    if (servo_angle == -1) servo_angle = extract_int_field(payload, "servo", -1);

    if (side < 0 || side > 1 || servo_angle < 0) {
        ESP_LOGW(TAG, "Invalid shoulder command: %s", payload.c_str());
        return;
    }

    if (xSemaphoreTake(g_motion_mutex, portMAX_DELAY) == pdTRUE) {
        if (side == 0) g_targets.shoulder_left  = static_cast<float>(servo_angle);
        else           g_targets.shoulder_right = static_cast<float>(servo_angle);
        xSemaphoreGive(g_motion_mutex);
    }

    ESP_LOGI(TAG, "Shoulder target side=%d angle=%d", side, servo_angle);
}

static void handle_elbow_command(const std::string& topic, const std::string& payload) {
    int side  = extract_int_field(payload, "\"side\"",  -1);
    if (side == -1) side = extract_int_field(payload, "side", -1);
    int angle = extract_int_field(payload, "\"angle\"", -1);
    if (angle == -1) angle = extract_int_field(payload, "angle", -1);

    if (side < 0 || side > 1 || angle < 0) {
        ESP_LOGW(TAG, "Invalid elbow command: %s", payload.c_str());
        return;
    }

    if (xSemaphoreTake(g_motion_mutex, portMAX_DELAY) == pdTRUE) {
        if (side == 0) g_targets.elbow_left  = static_cast<float>(angle);
        else           g_targets.elbow_right = static_cast<float>(angle);
        xSemaphoreGive(g_motion_mutex);
    }

    ESP_LOGI(TAG, "Elbow target side=%d angle=%d", side, angle);
}

static void handle_estop(const std::string& topic, const std::string& payload) {
    ESP_LOGW(TAG, "Emergency stop requested: %s", payload.c_str());

    if (xSemaphoreTake(g_motion_mutex, portMAX_DELAY) == pdTRUE) {
        g_targets.emergency_stop = true;
        g_targets.dir   = 0;
        g_targets.speed = 0;
        xSemaphoreGive(g_motion_mutex);
    }

    g_state.dir   = 0;
    g_state.speed = 0.0f;
    apply_robot_motion(0, 0);
}

static void subscribe_robot_topics() {
    if (mqtt && mqtt->is_connected()) {
        mqtt->subscribe(topic_robot_movement, 1);
        mqtt->subscribe(topic_robot_estop,    1);
        mqtt->subscribe(topic_joint_shoulder, 1);
        mqtt->subscribe(topic_joint_elbow,    1);
    }
}

// ─── app_main ────────────────────────────────────────────────────────────────

extern "C" void app_main(void) {
    g_motion_mutex = xSemaphoreCreateMutex();
    if (!g_motion_mutex) {
        ESP_LOGE(TAG, "Failed to create motion mutex");
        return;
    }

    g_motor1 = new BTS7960(GPIO_NUM_32, GPIO_NUM_33, GPIO_NUM_19, GPIO_NUM_18,
                            LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_TIMER_0);
    g_motor2 = new BTS7960(GPIO_NUM_26, GPIO_NUM_25, GPIO_NUM_17, GPIO_NUM_16,
                            LEDC_CHANNEL_4, LEDC_CHANNEL_5, LEDC_TIMER_1);

    g_motor1->enable();
    g_motor2->enable();

    g_shoulder_servo_left  = new Servo(SERVO_PIN_SHOULDER_LEFT,  SERVO_TYPE::STANDARD_180,
                                       LEDC_CHANNEL_2, LEDC_TIMER_2);
    g_shoulder_servo_right = new Servo(SERVO_PIN_SHOULDER_RIGHT, SERVO_TYPE::STANDARD_180,
                                       LEDC_CHANNEL_3, LEDC_TIMER_2);
    g_elbow_servo_left     = new Servo(SERVO_PIN_ELBOW_LEFT,     SERVO_TYPE::STANDARD_180,
                                       LEDC_CHANNEL_6, LEDC_TIMER_3);
    g_elbow_servo_right    = new Servo(SERVO_PIN_ELBOW_RIGHT,    SERVO_TYPE::STANDARD_180,
                                       LEDC_CHANNEL_7, LEDC_TIMER_3);

    xTaskCreate(motion_control_task, "motion_ctrl", 4096, nullptr, 5, nullptr);

    wifi = new WiFiManager();
    mqtt = new MQTTClient();

    wifi->on_connected([](std::string ip) {
        ESP_LOGI(TAG, "WiFi connected, IP=%s", ip.c_str());
    });
    wifi->on_disconnected([]() {
        ESP_LOGW(TAG, "WiFi disconnected");
    });

    wifi->init_sta(WIFI_SSID, WIFI_PASSWORD, 5);
    wifi->connect();

    while (!wifi->is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    MQTTConfig cfg;
    cfg.broker_uri = mqtt_broker;
    cfg.port       = mqtt_port;
    cfg.client_id  = "ESP32_SARCUS_" + std::to_string(rand() % 10000);
    cfg.keepalive  = 60;

    mqtt->init(cfg);

    mqtt->on_connected([&]() {
        ESP_LOGI(TAG, "MQTT connected");
        subscribe_robot_topics();
    });
    mqtt->on_disconnected([&]() {
        ESP_LOGW(TAG, "MQTT disconnected");
    });

    mqtt->on_topic(topic_robot_movement, handle_movement_command);
    mqtt->on_topic(topic_robot_estop,    handle_estop);
    mqtt->on_topic(topic_joint_shoulder, handle_shoulder_command);
    mqtt->on_topic(topic_joint_elbow,    handle_elbow_command);

    mqtt->on_message([&](std::string topic, std::string msg) {
        ESP_LOGI(TAG, "MQTT recv %s -> %s", topic.c_str(), msg.c_str());
    });

    mqtt->connect();

    while (!mqtt->is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGI(TAG, "ESP32 ready — smoothed motion enabled");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}