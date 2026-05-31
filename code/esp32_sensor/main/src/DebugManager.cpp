// =============================================================================
//  SARCUS Robot — Debug Manager Implementation
//  Routes structured logs to both Serial (ESP_LOG) and MQTT debug topic.
// =============================================================================

#include "inc/DebugManager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>

namespace SARCUS {

static const char* Tag = "DebugManager";

// ─── Singleton ────────────────────────────────────────────────────────────────

DebugManager& DebugManager::getInstance() {
    static DebugManager instance;
    return instance;
}

// ─── init ─────────────────────────────────────────────────────────────────────

void DebugManager::init() {
    m_mqtt_ready = true;
    ESP_LOGI(Tag, "Debug routing to MQTT enabled");
}

// ─── log ──────────────────────────────────────────────────────────────────────

void DebugManager::log(const char* level, const char* tag, const char* fmt, ...) {
    char msg_buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    // Serial output via ESP_LOG
    if (strcmp(level, "DEBUG") == 0)      { ESP_LOGD(tag, "%s", msg_buf); }
    else if (strcmp(level, "INFO") == 0)  { ESP_LOGI(tag, "%s", msg_buf); }
    else if (strcmp(level, "WARN") == 0)  { ESP_LOGW(tag, "%s", msg_buf); }
    else if (strcmp(level, "ERROR") == 0) { ESP_LOGE(tag, "%s", msg_buf); }

    // MQTT output
    if (m_mqtt_ready) {
        char json_buf[320];
        snprintf(json_buf, sizeof(json_buf),
                 "{\"level\":\"%s\",\"tag\":\"%s\",\"msg\":\"%s\",\"ts\":%lu}",
                 level, tag, msg_buf,
                 (unsigned long)(esp_timer_get_time() / 1000));
        MqttManager::getInstance().publishDebugLog(std::string(json_buf));
    }
}

void DebugManager::logDebug(const char* tag, const char* msg) { log("DEBUG", tag, "%s", msg); }
void DebugManager::logInfo (const char* tag, const char* msg) { log("INFO",  tag, "%s", msg); }
void DebugManager::logWarn (const char* tag, const char* msg) { log("WARN",  tag, "%s", msg); }
void DebugManager::logError(const char* tag, const char* msg) { log("ERROR", tag, "%s", msg); }

// ─── reportError ──────────────────────────────────────────────────────────────

void DebugManager::reportError(ErrorCode code, const char* context) {
    char msg[128];
    snprintf(msg, sizeof(msg),
             "ErrorCode=0x%02X context=%s", (uint8_t)code, context);

    ESP_LOGE(Tag, "SYSTEM ERROR: %s", msg);

    if (m_mqtt_ready) {
        char json_buf[200];
        snprintf(json_buf, sizeof(json_buf),
                 "{\"level\":\"ERROR\",\"error_code\":\"0x%02X\","
                 "\"context\":\"%s\",\"ts\":%lu}",
                 (uint8_t)code, context,
                 (unsigned long)(esp_timer_get_time() / 1000));
        MqttManager::getInstance().publishDebugLog(std::string(json_buf));
    }
}

} // namespace SARCUS
