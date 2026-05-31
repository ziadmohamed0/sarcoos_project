#include "main.h"
#include "bts7960.h"
#include "servo.h"
#include "WiFi.h"
#include "MQTT.h"
#include "NVS.h"
#include "stepper.h"
#include "ProvisioningServer.h"
#include "json_parser.h"
#include "esp_log.h"
#include "nvs_flash.h"
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

Stepper* g_wrist_left  = nullptr;
Stepper* g_wrist_right = nullptr;

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

// ─── pin config from Kconfig (with defaults) ─────────────────────────────────

#ifndef CONFIG_SARCUS_MOTOR1_R_PWM
    #define CONFIG_SARCUS_MOTOR1_R_PWM 32
#endif
#ifndef CONFIG_SARCUS_MOTOR1_L_PWM
    #define CONFIG_SARCUS_MOTOR1_L_PWM 33
#endif
#ifndef CONFIG_SARCUS_MOTOR1_R_EN
    #define CONFIG_SARCUS_MOTOR1_R_EN 19
#endif
#ifndef CONFIG_SARCUS_MOTOR1_L_EN
    #define CONFIG_SARCUS_MOTOR1_L_EN 18
#endif
#ifndef CONFIG_SARCUS_MOTOR2_R_PWM
    #define CONFIG_SARCUS_MOTOR2_R_PWM 26
#endif
#ifndef CONFIG_SARCUS_MOTOR2_L_PWM
    #define CONFIG_SARCUS_MOTOR2_L_PWM 25
#endif
#ifndef CONFIG_SARCUS_MOTOR2_R_EN
    #define CONFIG_SARCUS_MOTOR2_R_EN 17
#endif
#ifndef CONFIG_SARCUS_MOTOR2_L_EN
    #define CONFIG_SARCUS_MOTOR2_L_EN 16
#endif

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

static void apply_robot_motion(int dir, int speed) {
    if (speed < 0)   speed = 0;
    if (speed > 100) speed = 100;

    switch (dir) {
        case 1:
            g_motor1->forward(speed);
            g_motor2->forward(speed);
            break;
        case 2:
            g_motor1->backward(speed);
            g_motor2->backward(speed);
            break;
        case 3:
            g_motor1->backward(speed);
            g_motor2->forward(speed);
            break;
        case 4:
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

// ─── MQTT handlers ──────────────────────────────────────────────────────────

static void handle_movement_command(const std::string& topic, const std::string& payload) {
    JsonParser json;
    if (!json.parse(payload)) { ESP_LOGW(TAG, "Invalid JSON: %s", payload.c_str()); return; }

    int dir   = json.getInt("dir", -1);
    int speed = json.getInt("speed", 0);

    if (dir < 0 || dir > 4) {
        ESP_LOGW(TAG, "Unknown dir value: %d", dir);
        return;
    }

    if (xSemaphoreTake(g_motion_mutex, portMAX_DELAY) == pdTRUE) {
        g_targets.emergency_stop = false;
        g_targets.dir   = dir;
        g_targets.speed = speed;
        xSemaphoreGive(g_motion_mutex);
    }
    ESP_LOGI(TAG, "Movement dir=%d speed=%d", dir, speed);
}

static void handle_shoulder_command(const std::string& topic, const std::string& payload) {
    JsonParser json;
    if (!json.parse(payload)) { ESP_LOGW(TAG, "Invalid JSON: %s", payload.c_str()); return; }

    int side        = json.getInt("side", -1);
    int servo_angle = json.getInt("servo", -1);

    if (side < 0 || side > 1 || servo_angle < 0) {
        ESP_LOGW(TAG, "Invalid shoulder command");
        return;
    }

    if (xSemaphoreTake(g_motion_mutex, portMAX_DELAY) == pdTRUE) {
        if (side == 0) g_targets.shoulder_left  = static_cast<float>(servo_angle);
        else           g_targets.shoulder_right = static_cast<float>(servo_angle);
        xSemaphoreGive(g_motion_mutex);
    }
    ESP_LOGI(TAG, "Shoulder side=%d angle=%d", side, servo_angle);
}

static void handle_elbow_command(const std::string& topic, const std::string& payload) {
    JsonParser json;
    if (!json.parse(payload)) { ESP_LOGW(TAG, "Invalid JSON: %s", payload.c_str()); return; }

    int side  = json.getInt("side", -1);
    int angle = json.getInt("angle", -1);

    if (side < 0 || side > 1 || angle < 0) {
        ESP_LOGW(TAG, "Invalid elbow command");
        return;
    }

    if (xSemaphoreTake(g_motion_mutex, portMAX_DELAY) == pdTRUE) {
        if (side == 0) g_targets.elbow_left  = static_cast<float>(angle);
        else           g_targets.elbow_right = static_cast<float>(angle);
        xSemaphoreGive(g_motion_mutex);
    }
    ESP_LOGI(TAG, "Elbow side=%d angle=%d", side, angle);
}

static void handle_wrist_command(const std::string& topic, const std::string& payload) {
    JsonParser json;
    if (!json.parse(payload)) { ESP_LOGW(TAG, "Invalid JSON: %s", payload.c_str()); return; }

    int side  = json.getInt("side", -1);
    int steps = json.getInt("steps", 0);
    int speed = json.getInt("speed", 60);
    int dir   = json.getInt("dir", 0);

    if (side < 0 || side > 1 || steps == 0) {
        ESP_LOGW(TAG, "Invalid wrist command");
        return;
    }

    int signed_steps = (dir == 0) ? steps : -steps;

    if (side == 0 && g_wrist_left) {
        g_wrist_left->move(signed_steps, static_cast<float>(speed));
    } else if (side == 1 && g_wrist_right) {
        g_wrist_right->move(signed_steps, static_cast<float>(speed));
    }
    ESP_LOGI(TAG, "Wrist side=%d steps=%d speed=%d", side, signed_steps, speed);
}

static void handle_estop(const std::string& topic, const std::string& payload) {
    ESP_LOGW(TAG, "Emergency stop requested");

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
        mqtt->subscribe(topic_joint_wrist,    1);
    }
}

// ─── NVS helpers ──────────────────────────────────────────────────────────────

static std::string read_nvs_str(const char* ns, const char* key, const std::string& fallback) {
    nvs_handle_t h;
    if (nvs_open(ns, NVS_READONLY, &h) != ESP_OK) return fallback;
    size_t sz = 0;
    if (nvs_get_str(h, key, nullptr, &sz) != ESP_OK || sz == 0) {
        nvs_close(h); return fallback;
    }
    std::string val(sz, '\0');
    nvs_get_str(h, key, &val[0], &sz);
    nvs_close(h);
    if (!val.empty() && val.back() == '\0') val.pop_back();
    return val.empty() ? fallback : val;
}

// ─── app_main ────────────────────────────────────────────────────────────────

extern "C" void app_main(void) {
    // Init NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Check for saved credentials
    std::string saved_ssid  = read_nvs_str("sarcus", "wifi_ssid", "");
    std::string saved_pass  = read_nvs_str("sarcus", "wifi_pass", "");
    std::string saved_broker = read_nvs_str("sarcus", "mqtt_uri", "");

    std::string wifi_ssid, wifi_pass, mqtt_broker_uri;

    if (!saved_ssid.empty()) {
        // Use saved credentials
        wifi_ssid       = saved_ssid;
        wifi_pass       = saved_pass;
        mqtt_broker_uri = saved_broker.empty() ? DEFAULT_MQTT_URI : saved_broker;
        ESP_LOGI(TAG, "Using saved WiFi: %s", wifi_ssid.c_str());
        ESP_LOGI(TAG, "MQTT broker: %s", mqtt_broker_uri.c_str());
    } else {
        // No saved credentials → run provisioning
        ESP_LOGI(TAG, "No WiFi credentials saved. Starting provisioning...");
        ESP_LOGI(TAG, "Connect to AP: SARCUS_SETUP (pass: sarcus2024)");
        ESP_LOGI(TAG, "Then open http://192.168.4.1 in your browser");

        ProvisioningServer prov;
        ProvisioningResult result = prov.run();

        if (!result.success) {
            ESP_LOGE(TAG, "Provisioning failed. Rebooting...");
            esp_restart();
        }

        wifi_ssid       = result.wifi_ssid;
        wifi_pass       = result.wifi_pass;
        mqtt_broker_uri = result.mqtt_broker;
        if (mqtt_broker_uri.empty()) mqtt_broker_uri = DEFAULT_MQTT_URI;

        ESP_LOGI(TAG, "Provisioning complete. IP: %s", result.sta_ip.c_str());
        ESP_LOGI(TAG, "MQTT broker: %s", mqtt_broker_uri.c_str());

        // ProvisioningServer keeps AP alive. We don't need the old WiFiManager
        // for connection — it's already connected. We just set the pin.
        // But we still create WiFiManager for status callbacks.
    }

    g_motion_mutex = xSemaphoreCreateMutex();
    if (!g_motion_mutex) {
        ESP_LOGE(TAG, "Failed to create motion mutex");
        return;
    }

    g_motor1 = new BTS7960(
        static_cast<gpio_num_t>(CONFIG_SARCUS_MOTOR1_R_PWM),
        static_cast<gpio_num_t>(CONFIG_SARCUS_MOTOR1_L_PWM),
        static_cast<gpio_num_t>(CONFIG_SARCUS_MOTOR1_R_EN),
        static_cast<gpio_num_t>(CONFIG_SARCUS_MOTOR1_L_EN),
        LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_TIMER_0);
    g_motor2 = new BTS7960(
        static_cast<gpio_num_t>(CONFIG_SARCUS_MOTOR2_R_PWM),
        static_cast<gpio_num_t>(CONFIG_SARCUS_MOTOR2_L_PWM),
        static_cast<gpio_num_t>(CONFIG_SARCUS_MOTOR2_R_EN),
        static_cast<gpio_num_t>(CONFIG_SARCUS_MOTOR2_L_EN),
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

    g_wrist_left  = new Stepper(STEPPER_PIN_LEFT_STEP,  STEPPER_PIN_LEFT_DIR,  STEPPER_PIN_LEFT_EN);
    g_wrist_right = new Stepper(STEPPER_PIN_RIGHT_STEP, STEPPER_PIN_RIGHT_DIR, STEPPER_PIN_RIGHT_EN);
    g_wrist_left->init();
    g_wrist_right->init();

    xTaskCreate(motion_control_task, "motion_ctrl", 4096, nullptr, 5, nullptr);

    // WiFi + MQTT setup
    mqtt = new MQTTClient();

    if (!saved_ssid.empty()) {
        // NVS had credentials — connect WiFi via WiFiManager
        wifi = new WiFiManager();

        wifi->on_connected([](std::string ip) {
            ESP_LOGI(TAG, "WiFi connected, IP=%s", ip.c_str());
        });
        wifi->on_disconnected([]() {
            ESP_LOGW(TAG, "WiFi disconnected");
        });

        wifi->init_sta(wifi_ssid, wifi_pass, 5);
        wifi->connect();

        while (!wifi->is_connected()) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    } else {
        // Provisioning already connected WiFi — just set up MQTT
        ESP_LOGI(TAG, "WiFi already connected via provisioning");
    }

    MQTTConfig cfg;
    cfg.broker_uri = mqtt_broker_uri;
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
    mqtt->on_topic(topic_joint_wrist,    handle_wrist_command);

    mqtt->on_message([&](std::string topic, std::string msg) {
        ESP_LOGI(TAG, "MQTT recv %s -> %s", topic.c_str(), msg.c_str());
    });

    mqtt->connect();

    while (!mqtt->is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGI(TAG, "ESP32 ready — smoothed motion enabled");

    while (true) {
        if (mqtt && mqtt->is_connected()) {
            if (g_wrist_left && g_wrist_left->is_running()) {
                char buf[80];
                snprintf(buf, sizeof(buf),
                         "{\"side\":0,\"remaining\":%d,\"total\":%d}",
                         g_wrist_left->getRemainingSteps(), g_wrist_left->getTotalSteps());
                mqtt->publish(topic_joint_wrist, buf, 0, false);
            }
            if (g_wrist_right && g_wrist_right->is_running()) {
                char buf[80];
                snprintf(buf, sizeof(buf),
                         "{\"side\":1,\"remaining\":%d,\"total\":%d}",
                         g_wrist_right->getRemainingSteps(), g_wrist_right->getTotalSteps());
                mqtt->publish(topic_joint_wrist, buf, 0, false);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
