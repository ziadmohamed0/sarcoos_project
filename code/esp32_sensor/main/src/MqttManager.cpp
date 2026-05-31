// =============================================================================
//  SARCUS Robot — MQTT Manager Implementation
//  Connects to Mosquitto broker via ESP-MQTT.
//  Publishes: IMU, GPS, Ultrasonic, Heartbeat, Debug logs.
//  Subscribes: Movement, Joints (shoulder/elbow/wrist), Suit, E-Stop.
//  JSON parsing uses lightweight manual parsing (no cJSON dependency needed).
// =============================================================================

#include "inc/MqttManager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace SARCUS {

static const char* Tag = "MqttManager";

// ─── Singleton ────────────────────────────────────────────────────────────────

MqttManager& MqttManager::getInstance() {
    static MqttManager instance;
    return instance;
}

// ─── start ────────────────────────────────────────────────────────────────────

void MqttManager::start(const char* broker_uri,
                         const char* username,
                         const char* password) {
    ESP_LOGI(Tag, "[ENTRY] start() broker=%s", broker_uri);

    m_broker_uri_storage = std::string(broker_uri);

    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.uri       = m_broker_uri_storage.c_str();
    cfg.credentials.client_id    = "sarcus-robot-esp32";
    cfg.session.keepalive         = 30;
    cfg.network.reconnect_timeout_ms = 5000;

    if (username) cfg.credentials.username = username;
    if (password) cfg.credentials.authentication.password = password;

    m_client = esp_mqtt_client_init(&cfg);
    if (!m_client) {
        ESP_LOGE(Tag, "esp_mqtt_client_init failed");
        return;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(
        m_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, eventHandler, this));

    ESP_ERROR_CHECK(esp_mqtt_client_start(m_client));

    ESP_LOGI(Tag, "MQTT client started");
    ESP_LOGI(Tag, "[EXIT]  start()");
}

// ─── stop ─────────────────────────────────────────────────────────────────────

void MqttManager::stop() {
    ESP_LOGI(Tag, "[ENTRY] stop()");
    if (m_client) {
        esp_mqtt_client_stop(m_client);
        esp_mqtt_client_destroy(m_client);
        m_client    = nullptr;
        m_connected = false;
    }
    ESP_LOGI(Tag, "[EXIT]  stop()");
}

// ─── Event Handler ────────────────────────────────────────────────────────────

void MqttManager::eventHandler(void* arg, esp_event_base_t base,
                                 int32_t event_id, void* event_data) {
    auto* self  = static_cast<MqttManager*>(arg);
    auto* event = static_cast<esp_mqtt_event_handle_t>(event_data);

    ESP_LOGD(Tag, "MQTT event id=%ld", (long)event_id);

    switch (event_id) {

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(Tag, "MQTT connected to broker");
        self->m_connected = true;

        // Subscribe to all control topics (QoS 1)
        esp_mqtt_client_subscribe(self->m_client, Topics::MOVEMENT,       1);
        esp_mqtt_client_subscribe(self->m_client, Topics::SHOULDER,       1);
        esp_mqtt_client_subscribe(self->m_client, Topics::ELBOW,          1);
        esp_mqtt_client_subscribe(self->m_client, Topics::WRIST,          1);
        esp_mqtt_client_subscribe(self->m_client, Topics::SUIT_BUTTONS,   1);
        esp_mqtt_client_subscribe(self->m_client, Topics::SUIT_POTS,      1);
        esp_mqtt_client_subscribe(self->m_client, Topics::EMERGENCY_STOP, 1);

        ESP_LOGI(Tag, "Subscribed to all control topics");
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(Tag, "MQTT disconnected — will auto-reconnect");
        self->m_connected = false;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGD(Tag, "Subscribed OK, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        if (event->topic && event->data) {
            self->dispatchMessage(event->topic, event->topic_len,
                                  event->data,  event->data_len,
                                  event->current_data_offset);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(Tag, "MQTT error — error_type=%d",
                 (int)event->error_handle->error_type);
        break;

    default:
        break;
    }
}

// ─── dispatchMessage ──────────────────────────────────────────────────────────

void MqttManager::dispatchMessage(const char* topic, int topic_len,
                                   const char* data,  int data_len,
                                   int event_data_offset) {
    // Null-terminate strings for easier comparison
    char topic_buf[128] = {};
    char data_buf[512]  = {};

    int tlen = (topic_len < (int)sizeof(topic_buf) - 1) ? topic_len : (int)sizeof(topic_buf) - 1;
    int dlen = (data_len  < (int)sizeof(data_buf)  - 1) ? data_len  : (int)sizeof(data_buf)  - 1;
    memcpy(topic_buf, topic, tlen);
    memcpy(data_buf,  data,  dlen);

    // Guard against MQTT fragmented (multi-packet) messages
    if (event_data_offset > 0) {
        ESP_LOGW(Tag, "Fragmented MQTT msg on [%s] — skipped (offset=%d)", topic_buf, event_data_offset);
        return;
    }

    ESP_LOGD(Tag, "Received [%s]: %s", topic_buf, data_buf);

    // ── Movement ──────────────────────────────────────────────────────────────
    if (strcmp(topic_buf, Topics::MOVEMENT) == 0) {
        MoveCmd cmd = {};
        if (parseMoveCmd(data_buf, cmd) && m_cb_move) {
            ESP_LOGD(Tag, "[ENTRY] move callback dir=%u speed=%u",
                     (uint8_t)cmd.direction, cmd.speed_percent);
            m_cb_move(cmd);
            ESP_LOGD(Tag, "[EXIT]  move callback");
        }
    }
    // ── Shoulder ──────────────────────────────────────────────────────────────
    else if (strcmp(topic_buf, Topics::SHOULDER) == 0) {
        ShoulderCmd cmd = {};
        if (parseShoulderCmd(data_buf, cmd) && m_cb_shoulder) {
            ESP_LOGD(Tag, "[ENTRY] shoulder callback dc=%d servo=%d side=%u",
                     cmd.dc_angle_deg, cmd.servo_angle_deg, cmd.side);
            m_cb_shoulder(cmd);
            ESP_LOGD(Tag, "[EXIT]  shoulder callback");
        }
    }
    // ── Elbow ─────────────────────────────────────────────────────────────────
    else if (strcmp(topic_buf, Topics::ELBOW) == 0) {
        ElbowCmd cmd = {};
        if (parseElbowCmd(data_buf, cmd) && m_cb_elbow) {
            ESP_LOGD(Tag, "[ENTRY] elbow callback angle=%d side=%u",
                     cmd.servo_angle_deg, cmd.side);
            m_cb_elbow(cmd);
            ESP_LOGD(Tag, "[EXIT]  elbow callback");
        }
    }
    // ── Wrist ─────────────────────────────────────────────────────────────────
    else if (strcmp(topic_buf, Topics::WRIST) == 0) {
        WristCmd cmd = {};
        if (parseWristCmd(data_buf, cmd) && m_cb_wrist) {
            ESP_LOGD(Tag, "[ENTRY] wrist callback steps=%ld side=%u",
                     (long)cmd.steps, cmd.side);
            m_cb_wrist(cmd);
            ESP_LOGD(Tag, "[EXIT]  wrist callback");
        }
    }
    // ── Suit Buttons ──────────────────────────────────────────────────────────
    else if (strcmp(topic_buf, Topics::SUIT_BUTTONS) == 0) {
        // Payload: {"buttons": 0x0000001F}  or plain number
        uint32_t mask = 0;
        const char* p = strstr(data_buf, "\"buttons\"");
        if (p) {
            p = strchr(p, ':');
            if (p) mask = (uint32_t)strtoul(p + 1, nullptr, 0);
        } else {
            mask = (uint32_t)strtoul(data_buf, nullptr, 0);
        }
        if (m_cb_suit_btn) {
            ESP_LOGD(Tag, "[ENTRY] suit_button callback mask=0x%08lX", (unsigned long)mask);
            m_cb_suit_btn(mask);
            ESP_LOGD(Tag, "[EXIT]  suit_button callback");
        }
    }
    // ── Suit Pots ─────────────────────────────────────────────────────────────
    else if (strcmp(topic_buf, Topics::SUIT_POTS) == 0) {
        // Payload: [v0, v1, v2, ... v19]   JSON array
        uint16_t pots[20] = {};
        uint8_t  count    = 0;
        const char* p = data_buf;
        while (*p && count < 20) {
            while (*p && (*p == '[' || *p == ',' || *p == ' ')) p++;
            if (*p == ']' || *p == '\0') break;
            char* end;
            long val = strtol(p, &end, 10);
            if (end == p) break;
            pots[count++] = (uint16_t)(val & 0xFFFF);
            p = end;
        }
        if (m_cb_suit_pot && count > 0) {
            ESP_LOGD(Tag, "[ENTRY] suit_pot callback count=%u", count);
            m_cb_suit_pot(pots, count);
            ESP_LOGD(Tag, "[EXIT]  suit_pot callback");
        }
    }
    // ── Emergency Stop ────────────────────────────────────────────────────────
    else if (strcmp(topic_buf, Topics::EMERGENCY_STOP) == 0) {
        ESP_LOGW(Tag, "EMERGENCY STOP received via MQTT");
        if (m_cb_estop) m_cb_estop();
    }
}

// ─── JSON Parsers ─────────────────────────────────────────────────────────────
// Minimal key:value JSON extractor — no external cJSON dependency.

static int32_t json_get_int(const char* json, const char* key, int32_t default_val) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char* p = strstr(json, pattern);
    if (!p) return default_val;
    p += strlen(pattern);
    // Skip : and whitespace
    while (*p == ':' || *p == ' ') p++;
    if (*p == '\0') return default_val;
    return (int32_t)strtol(p, nullptr, 10);
}

bool MqttManager::parseMoveCmd(const char* json, MoveCmd& out) {
    // {"dir":1,"speed":75}
    out.direction     = (MoveDir)json_get_int(json, "dir",   0);
    out.speed_percent = (uint8_t)json_get_int(json, "speed", 0);
    ESP_LOGD(Tag, "parseMoveCmd dir=%u speed=%u",
             (uint8_t)out.direction, out.speed_percent);
    return true;
}

bool MqttManager::parseShoulderCmd(const char* json, ShoulderCmd& out) {
    // {"dc_angle":45,"servo":90,"side":0}
    out.dc_angle_deg    = (int16_t)json_get_int(json, "dc_angle", 0);
    out.servo_angle_deg = (int16_t)json_get_int(json, "servo",    0);
    out.side            = (uint8_t)json_get_int(json, "side",     0);
    ESP_LOGD(Tag, "parseShoulderCmd dc=%d servo=%d side=%u",
             out.dc_angle_deg, out.servo_angle_deg, out.side);
    return true;
}

bool MqttManager::parseElbowCmd(const char* json, ElbowCmd& out) {
    // {"angle":120,"side":0}
    out.servo_angle_deg = (int16_t)json_get_int(json, "angle", 0);
    out.side            = (uint8_t)json_get_int(json, "side",  0);
    ESP_LOGD(Tag, "parseElbowCmd angle=%d side=%u",
             out.servo_angle_deg, out.side);
    return true;
}

bool MqttManager::parseWristCmd(const char* json, WristCmd& out) {
    // {"steps":200,"speed":60,"dir":0,"side":0}
    out.steps     = (int32_t) json_get_int(json, "steps", 0);
    out.speed_rpm = (uint16_t)json_get_int(json, "speed", 60);
    out.direction = (uint8_t) json_get_int(json, "dir",   0);
    out.side      = (uint8_t) json_get_int(json, "side",  0);
    ESP_LOGD(Tag, "parseWristCmd steps=%ld speed=%u dir=%u side=%u",
             (long)out.steps, out.speed_rpm, out.direction, out.side);
    return true;
}

// ─── Publish Helpers ──────────────────────────────────────────────────────────

void MqttManager::publish(const char* topic, const std::string& payload, int qos) {
    if (!m_client || !m_connected) {
        ESP_LOGW(Tag, "publish() skipped — not connected [%s]", topic);
        return;
    }
    int msg_id = esp_mqtt_client_publish(m_client, topic,
                                          payload.c_str(), (int)payload.size(),
                                          qos, 0);
    ESP_LOGD(Tag, "Published to [%s] msg_id=%d", topic, msg_id);
}

void MqttManager::publishIMU(const ImuData& imu) {
    if (!imu.valid) return;
    char buf[192];
    snprintf(buf, sizeof(buf),
             "{\"roll\":%.2f,\"pitch\":%.2f,\"yaw\":%.2f,"
             "\"ax\":%.3f,\"ay\":%.3f,\"az\":%.3f,"
             "\"gx\":%.2f,\"gy\":%.2f,\"gz\":%.2f,\"ts\":%lu}",
             imu.roll, imu.pitch, imu.yaw,
             imu.accel_x, imu.accel_y, imu.accel_z,
             imu.gyro_x,  imu.gyro_y,  imu.gyro_z,
             (unsigned long)(esp_timer_get_time() / 1000));
    publish(Topics::IMU, std::string(buf), 0);
}

void MqttManager::publishGPS(const GpsData& gps) {
    char buf[160];
    if (gps.fix) {
        snprintf(buf, sizeof(buf),
                 "{\"fix\":true,\"lat\":%.7f,\"lon\":%.7f,"
                 "\"alt\":%.1f,\"sats\":%u,\"ts\":%lu}",
                 gps.latitude, gps.longitude, gps.altitude, gps.satellites,
                 (unsigned long)(esp_timer_get_time() / 1000));
    } else {
        snprintf(buf, sizeof(buf),
                 "{\"fix\":false,\"ts\":%lu}",
                 (unsigned long)(esp_timer_get_time() / 1000));
    }
    publish(Topics::GPS, std::string(buf), 0);
}

void MqttManager::publishUltrasonic(const UltrasonicData& us) {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"front\":%.1f,\"back\":%.1f,\"left\":%.1f,\"right\":%.1f,\"ts\":%lu}",
             us.front_cm, us.back_cm, us.left_cm, us.right_cm,
             (unsigned long)(esp_timer_get_time() / 1000));
    publish(Topics::ULTRASONIC, std::string(buf), 0);
}

void MqttManager::publishHeartbeat(const std::string& device_id) {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"device\":\"%s\",\"uptime\":%lu,\"ts\":%lu}",
             device_id.c_str(),
             (unsigned long)(esp_timer_get_time() / 1000),
             (unsigned long)(esp_timer_get_time() / 1000));
    publish(Topics::HEARTBEAT, std::string(buf), 0);
}

void MqttManager::publishDebugLog(const std::string& msg) {
    if (!m_client || !m_connected) return;
    // QoS 0 for debug logs — best effort, don't block
    esp_mqtt_client_publish(m_client, Topics::DEBUG_LOGS,
                             msg.c_str(), (int)msg.size(), 0, 0);
}

} // namespace SARCUS
