#pragma once

// =============================================================================
//  SARCUS Robot — Heartbeat Manager
//  Publishes heartbeat to MQTT every 500ms from all subsystems.
// =============================================================================

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string>

namespace SARCUS {

class HeartbeatManager {
public:
    static HeartbeatManager& getInstance();

    // Start heartbeat task
    void start(const std::string& device_id);

    // Stop heartbeat task
    void stop();

private:
    HeartbeatManager()  = default;
    ~HeartbeatManager() = default;
    HeartbeatManager(const HeartbeatManager&) = delete;
    HeartbeatManager& operator=(const HeartbeatManager&) = delete;

    static void heartbeatTask(void* arg);

    TaskHandle_t m_task_handle = nullptr;
    std::string  m_device_id;
    bool         m_running = false;

    static constexpr uint32_t kHeartbeatIntervalMs = 500;
};

} // namespace SARCUS
