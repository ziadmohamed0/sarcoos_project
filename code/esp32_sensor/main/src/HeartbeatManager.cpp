// =============================================================================
//  SARCUS Robot — Heartbeat Manager Implementation
//  Publishes periodic heartbeat to MQTT to signal system is alive.
// =============================================================================

#include "inc/HeartbeatManager.h"
#include "inc/MqttManager.h"
#include "esp_log.h"
#include "freertos/task.h"

namespace SARCUS {

static const char* Tag = "HeartbeatMgr";

// ─── Singleton ────────────────────────────────────────────────────────────────

HeartbeatManager& HeartbeatManager::getInstance() {
    static HeartbeatManager instance;
    return instance;
}

// ─── start ────────────────────────────────────────────────────────────────────

void HeartbeatManager::start(const std::string& device_id) {
    ESP_LOGI(Tag, "[ENTRY] start() device_id=%s", device_id.c_str());

    if (m_running) {
        ESP_LOGW(Tag, "Already running");
        return;
    }

    m_device_id = device_id;
    m_running   = true;

    xTaskCreate(heartbeatTask, "heartbeat", 2048, this,
                tskIDLE_PRIORITY + 2, &m_task_handle);

    ESP_LOGI(Tag, "Heartbeat task started (interval=%ums)", kHeartbeatIntervalMs);
    ESP_LOGI(Tag, "[EXIT]  start()");
}

// ─── stop ─────────────────────────────────────────────────────────────────────

void HeartbeatManager::stop() {
    m_running = false;
    if (m_task_handle) {
        for (int i = 0; i < 10; i++) {
            if (eTaskGetState(m_task_handle) == eDeleted) break;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        m_task_handle = nullptr;
    }
}

// ─── heartbeatTask ────────────────────────────────────────────────────────────

void HeartbeatManager::heartbeatTask(void* arg) {
    auto* self = static_cast<HeartbeatManager*>(arg);
    ESP_LOGI(Tag, "Heartbeat task running");

    while (self->m_running) {
        MqttManager::getInstance().publishHeartbeat(self->m_device_id);
        vTaskDelay(pdMS_TO_TICKS(kHeartbeatIntervalMs));
    }

    ESP_LOGI(Tag, "Heartbeat task exiting");
    vTaskDelete(nullptr);
}

} // namespace SARCUS
