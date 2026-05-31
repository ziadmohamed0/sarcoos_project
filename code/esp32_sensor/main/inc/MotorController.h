#pragma once

// =============================================================================
//  SARCUS Robot — Motor Controller
//  Receives commands from MQTT / Suit / ROS2 and forwards to STM32 via UART.
//  Implements emergency stop logic and command priority:
//    Priority 1 (highest): Emergency Stop
//    Priority 2: ROS2 commands
//    Priority 3: MQTT Dashboard
//    Priority 4 (lowest): Suit wearable
// =============================================================================

#include "SarcusTypes.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/uart.h"

namespace SARCUS {

enum class ControlSource : uint8_t {
    NONE    = 0,
    MQTT    = 1,
    SUIT    = 2,
    ROS2    = 3,
    ESTOP   = 4,   // highest priority, always overrides
};

struct MotorCommand {
    CmdType       type;
    ControlSource source;
    union {
        MoveCmd     move;
        ShoulderCmd shoulder;
        ElbowCmd    elbow;
        WristCmd    wrist;
    };
};

class MotorController {
public:
    static MotorController& getInstance();

    // Initialize. Must be called after UartBridge::init()
    void init(uart_port_t stm32_port);

    // Start command dispatch task
    void start();

    // Stop everything — sends E-STOP to STM32
    void stop();

    // Submit commands from different sources
    void submitMove      (const MoveCmd&     cmd, ControlSource src);
    void submitShoulder  (const ShoulderCmd& cmd, ControlSource src);
    void submitElbow     (const ElbowCmd&    cmd, ControlSource src);
    void submitWrist     (const WristCmd&    cmd, ControlSource src);
    void triggerEStop    ();   // Immediate — bypasses queue

    // Get current active control source
    ControlSource getActiveSource() const { return m_active_source; }

private:
    MotorController()  = default;
    ~MotorController() = default;
    MotorController(const MotorController&) = delete;
    MotorController& operator=(const MotorController&) = delete;

    static void dispatchTask(void* arg);
    void        processCommand(const MotorCommand& cmd);

    QueueHandle_t m_cmd_queue       = nullptr;
    TaskHandle_t  m_task_handle     = nullptr;
    uart_port_t   m_stm32_port      = UART_NUM_1;
    ControlSource m_active_source   = ControlSource::NONE;
    bool          m_estop_active    = false;
    bool          m_running         = false;

    static constexpr int kQueueDepth = 16;
};

} // namespace SARCUS
