#pragma once

// =============================================================================
//  SARCUS Robot — Debug Manager
//  Routes ESP_LOG output to MQTT debug topic.
//  Provides structured log levels: DEBUG / INFO / WARN / ERROR
//
//  FIX v1.1: Removed HeartbeatManager class that was incorrectly embedded here.
//            HeartbeatManager now lives only in inc/HeartbeatManager.h
// =============================================================================

#include "MqttManager.h"
#include <string>
#include <cstdarg>

namespace SARCUS {

class DebugManager {
public:
    static DebugManager& getInstance();

    // Start routing logs to MQTT (requires MQTT to be connected first)
    void init();

    // Log to Serial + MQTT
    void log(const char* level, const char* tag, const char* fmt, ...);

    // Shorthand helpers
    void logDebug(const char* tag, const char* msg);
    void logInfo (const char* tag, const char* msg);
    void logWarn (const char* tag, const char* msg);
    void logError(const char* tag, const char* msg);

    // Report a system error code
    void reportError(ErrorCode code, const char* context);

private:
    DebugManager()  = default;
    ~DebugManager() = default;
    DebugManager(const DebugManager&) = delete;
    DebugManager& operator=(const DebugManager&) = delete;

    bool m_mqtt_ready = false;
};

} // namespace SARCUS
