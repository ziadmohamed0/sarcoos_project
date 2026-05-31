#pragma once

// =============================================================================
//  SARCUS Robot — UART Bridge
//  Handles framed UART communication between:
//    - ESP32 (Robot) ↔ STM32F405  (motor control)
//    - ESP32 (Robot) ↔ RPi 4      (ROS2 bridge)
//    - ESP32 (Robot) ↔ Suit ESP32 (suit input fallback)
//
//  Protocol: [0xAA][DEV_ID][CMD][LEN][DATA...][CHECKSUM][0x55]
// =============================================================================

#include "driver/uart.h"
#include "SarcusTypes.h"
#include <functional>

namespace SARCUS {

// Callback when a valid frame arrives
using FrameCallback = std::function<void(const UartFrame&)>;

class UartBridge {
public:
    static UartBridge& getInstance();

    // Initialize a UART port
    // Call once per port (STM32 port and/or RPi port)
    void init(uart_port_t port, int rx_pin, int tx_pin, int baud = 115200);

    // Start RX task with callback for valid frames
    void start(uart_port_t port, FrameCallback cb);

    // Stop RX task and release driver
    void stop(uart_port_t port);

    // Send a frame to a target device
    void sendFrame(uart_port_t port, DeviceID dst, CmdType cmd,
                   const uint8_t* data, uint8_t data_len);

    // Convenience senders
    void sendMoveCmd     (uart_port_t port, const MoveCmd&     cmd);
    void sendShoulderCmd (uart_port_t port, const ShoulderCmd& cmd);
    void sendElbowCmd    (uart_port_t port, const ElbowCmd&    cmd);
    void sendWristCmd    (uart_port_t port, const WristCmd&    cmd);
    void sendHeartbeat   (uart_port_t port);
    void sendEmergencyStop(uart_port_t port);

private:
    UartBridge()  = default;
    ~UartBridge() = default;
    UartBridge(const UartBridge&) = delete;
    UartBridge& operator=(const UartBridge&) = delete;

    // Frame helpers
    // Checksum = XOR of source (ROBOT_ESP32) ^ cmd ^ len ^ data
    static uint8_t   calcChecksum(const uint8_t* data, uint8_t len, DeviceID dst, CmdType cmd);
    static UartFrame parseFrame  (const uint8_t* raw, int len);

    // RX task (one per port)
    struct PortCtx {
        uart_port_t    port;
        FrameCallback  callback;
        TaskHandle_t   task_handle = nullptr;
        bool           running     = false;
        bool           initialized = false;
    };

    static void rxTask(void* arg);

    // Support 2 ports: STM32 and RPi
    static constexpr int kMaxPorts = 2;
    PortCtx m_ports[kMaxPorts];

    PortCtx* findPort(uart_port_t port);

    static constexpr int kRxBufSize = 512;
};

} // namespace SARCUS
