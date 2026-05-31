// =============================================================================
//  SARCUS Robot — Motor Controller Implementation
//  Receives commands from MQTT / Suit / ROS2 and forwards to STM32.
//  Priority: ESTOP > ROS2 > MQTT > SUIT
//  Emergency stop bypasses queue and sends directly to STM32.
// =============================================================================

#include "inc/MotorController.h"
#include "inc/UartBridge.h"
#include "inc/DebugManager.h"
#include "esp_log.h"
#include <cstring>

namespace SARCUS {

static const char* Tag = "MotorController";

// ─── Singleton ────────────────────────────────────────────────────────────────

MotorController& MotorController::getInstance() {
    static MotorController instance;
    return instance;
}

// ─── init ─────────────────────────────────────────────────────────────────────

void MotorController::init(uart_port_t stm32_port) {
    ESP_LOGI(Tag, "[ENTRY] init() stm32_port=%d", (int)stm32_port);

    m_stm32_port = stm32_port;

    if (m_cmd_queue == nullptr) {
        m_cmd_queue = xQueueCreate(kQueueDepth, sizeof(MotorCommand));
        if (!m_cmd_queue) {
            ESP_LOGE(Tag, "Failed to create command queue");
            return;
        }
    }

    ESP_LOGI(Tag, "Motor controller initialized. Queue depth=%d", kQueueDepth);
    ESP_LOGI(Tag, "[EXIT]  init()");
}

// ─── start ────────────────────────────────────────────────────────────────────

void MotorController::start() {
    ESP_LOGI(Tag, "[ENTRY] start()");

    if (m_running) {
        ESP_LOGW(Tag, "Already running");
        return;
    }

    m_running      = true;
    m_estop_active = false;

    xTaskCreate(dispatchTask, "motor_dispatch", 4096, this,
                tskIDLE_PRIORITY + 7, &m_task_handle);

    ESP_LOGI(Tag, "Motor dispatch task started");
    ESP_LOGI(Tag, "[EXIT]  start()");
}

// ─── stop ─────────────────────────────────────────────────────────────────────

void MotorController::stop() {
    ESP_LOGI(Tag, "[ENTRY] stop()");

    // Always send E-STOP first
    triggerEStop();

    m_running = false;

    if (m_task_handle) {
        for (int i = 0; i < 10; i++) {
            if (eTaskGetState(m_task_handle) == eDeleted) break;
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        m_task_handle = nullptr;
    }

    ESP_LOGI(Tag, "[EXIT]  stop()");
}

// ─── triggerEStop ─────────────────────────────────────────────────────────────

void MotorController::triggerEStop() {
    ESP_LOGW(Tag, "!!! EMERGENCY STOP TRIGGERED !!!");
    m_estop_active  = true;
    m_active_source = ControlSource::ESTOP;

    // Bypass queue — send immediately to STM32
    UartBridge::getInstance().sendEmergencyStop(m_stm32_port);

    // Flush queue to discard any pending commands
    if (m_cmd_queue) xQueueReset(m_cmd_queue);

    DebugManager::getInstance().logWarn(Tag, "EMERGENCY STOP — all motors halted");
}

// ─── submit helpers ───────────────────────────────────────────────────────────

void MotorController::submitMove(const MoveCmd& cmd, ControlSource src) {
    if (m_estop_active) {
        ESP_LOGW(Tag, "submitMove() blocked — E-STOP active");
        return;
    }
    MotorCommand mc = {};
    mc.type   = CmdType::CMD_MOVE;
    mc.source = src;
    mc.move   = cmd;
    if (xQueueSend(m_cmd_queue, &mc, pdMS_TO_TICKS(20)) != pdTRUE) {
        ESP_LOGW(Tag, "Move cmd dropped — queue full");
    }
}

void MotorController::submitShoulder(const ShoulderCmd& cmd, ControlSource src) {
    if (m_estop_active) return;
    MotorCommand mc = {};
    mc.type     = CmdType::CMD_SHOULDER;
    mc.source   = src;
    mc.shoulder = cmd;
    xQueueSend(m_cmd_queue, &mc, pdMS_TO_TICKS(20));
}

void MotorController::submitElbow(const ElbowCmd& cmd, ControlSource src) {
    if (m_estop_active) return;
    MotorCommand mc = {};
    mc.type   = CmdType::CMD_ELBOW;
    mc.source = src;
    mc.elbow  = cmd;
    xQueueSend(m_cmd_queue, &mc, pdMS_TO_TICKS(20));
}

void MotorController::submitWrist(const WristCmd& cmd, ControlSource src) {
    if (m_estop_active) return;
    MotorCommand mc = {};
    mc.type   = CmdType::CMD_WRIST;
    mc.source = src;
    mc.wrist  = cmd;
    xQueueSend(m_cmd_queue, &mc, pdMS_TO_TICKS(20));
}

// ─── dispatchTask ─────────────────────────────────────────────────────────────

void MotorController::dispatchTask(void* arg) {
    auto* self = static_cast<MotorController*>(arg);
    MotorCommand cmd;

    ESP_LOGI(Tag, "Dispatch task running");

    while (self->m_running) {
        if (xQueueReceive(self->m_cmd_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            // If E-STOP is active, only ESTOP source can pass
            if (self->m_estop_active && cmd.source != ControlSource::ESTOP) {
                ESP_LOGD(Tag, "Command discarded — E-STOP active");
                continue;
            }
            self->processCommand(cmd);
        }
    }

    ESP_LOGI(Tag, "Dispatch task exiting");
    vTaskDelete(nullptr);
}

// ─── processCommand ───────────────────────────────────────────────────────────

void MotorController::processCommand(const MotorCommand& cmd) {
    m_active_source = cmd.source;

    const char* src_str =
        cmd.source == ControlSource::MQTT   ? "MQTT"   :
        cmd.source == ControlSource::SUIT   ? "SUIT"   :
        cmd.source == ControlSource::ROS2   ? "ROS2"   :
        cmd.source == ControlSource::ESTOP  ? "ESTOP"  : "NONE";

    auto& uart = UartBridge::getInstance();

    switch (cmd.type) {
    case CmdType::CMD_MOVE:
        ESP_LOGI(Tag, "[%s] Move dir=%u speed=%u",
                 src_str, (uint8_t)cmd.move.direction, cmd.move.speed_percent);
        uart.sendMoveCmd(m_stm32_port, cmd.move);
        break;

    case CmdType::CMD_SHOULDER:
        ESP_LOGI(Tag, "[%s] Shoulder dc=%d servo=%d side=%u",
                 src_str, cmd.shoulder.dc_angle_deg,
                 cmd.shoulder.servo_angle_deg, cmd.shoulder.side);
        uart.sendShoulderCmd(m_stm32_port, cmd.shoulder);
        break;

    case CmdType::CMD_ELBOW:
        ESP_LOGI(Tag, "[%s] Elbow angle=%d side=%u",
                 src_str, cmd.elbow.servo_angle_deg, cmd.elbow.side);
        uart.sendElbowCmd(m_stm32_port, cmd.elbow);
        break;

    case CmdType::CMD_WRIST:
        ESP_LOGI(Tag, "[%s] Wrist steps=%ld speed=%u dir=%u side=%u",
                 src_str, (long)cmd.wrist.steps,
                 cmd.wrist.speed_rpm, cmd.wrist.direction, cmd.wrist.side);
        uart.sendWristCmd(m_stm32_port, cmd.wrist);
        break;

    case CmdType::CMD_STOP_ALL:
        ESP_LOGW(Tag, "[%s] STOP ALL", src_str);
        uart.sendEmergencyStop(m_stm32_port);
        break;

    default:
        ESP_LOGW(Tag, "Unknown command type: 0x%02X", (uint8_t)cmd.type);
        break;
    }
}

} // namespace SARCUS
