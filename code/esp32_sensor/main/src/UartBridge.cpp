// =============================================================================
//  SARCUS Robot — UART Bridge Implementation
//  Framed UART communication between ESP32 ↔ STM32F405 and ESP32 ↔ RPi 4.
//
//  Frame format: [0xAA][SRC][DST][CMD_TYPE][DATA_LEN][DATA...][CHECKSUM][0x55]
//  Checksum: XOR of SRC ^ DST ^ CMD_TYPE ^ DATA_LEN ^ all DATA bytes
//
//  FIX v1.1:
//    - sendFrame: added missing DST byte (was skipped → STM32 misaligned all fields)
//    - calcChecksum: now includes DST in XOR calculation
//    - parseFrame: aligned to 7-byte minimum (was 6) after DST addition
// =============================================================================

#include "inc/UartBridge.h"
#include "inc/DebugManager.h"
#include "esp_log.h"
#include <cstring>
#include <cstdio>

namespace SARCUS {

static const char* Tag = "UartBridge";

// =============================================================================
//  Singleton
// =============================================================================

UartBridge& UartBridge::getInstance() {
    static UartBridge instance;
    return instance;
}

// =============================================================================
//  Internal helpers — port lookup
// =============================================================================

UartBridge::PortCtx* UartBridge::findPort(uart_port_t port) {
    for (int i = 0; i < kMaxPorts; i++) {
        if (m_ports[i].initialized && m_ports[i].port == port) {
            return &m_ports[i];
        }
    }
    // Find empty slot
    for (int i = 0; i < kMaxPorts; i++) {
        if (!m_ports[i].initialized) return &m_ports[i];
    }
    return nullptr;
}

// =============================================================================
//  Checksum
//  XOR of: SRC ^ DST ^ CMD_TYPE ^ DATA_LEN ^ DATA[0..n-1]
//
//  FIX: DST added to checksum calculation (was missing before)
// =============================================================================

uint8_t UartBridge::calcChecksum(const uint8_t* data, uint8_t data_len,
                                  DeviceID dst, CmdType cmd) {
    // ✅ FIX: include dst in checksum — was missing, caused STM32 validation to always fail
    uint8_t cs = (uint8_t)DeviceID::ROBOT_ESP32  // SRC
               ^ (uint8_t)dst                     // DST ← added
               ^ (uint8_t)cmd
               ^ data_len;
    for (uint8_t i = 0; i < data_len; i++) {
        cs ^= data[i];
    }
    return cs;
}

// =============================================================================
//  Frame parser
//  Parses raw bytes into a UartFrame.
//
//  Frame layout (receiver side):
//  [0xAA][SRC][DST][CMD][LEN][DATA...][CSUM][0x55]
//   [0]   [1]  [2]  [3]  [4]  [5..N]  [N+1] [N+2]
//
//  Minimum frame: 7 bytes (0 data bytes)
// =============================================================================

UartFrame UartBridge::parseFrame(const uint8_t* raw, int len) {
    UartFrame frame = {};
    frame.valid = false;

    // Minimum: START(1)+SRC(1)+DST(1)+CMD(1)+LEN(1)+CSUM(1)+END(1) = 7
    if (len < 7) {
        ESP_LOGD(Tag, "parseFrame: too short (%d bytes)", len);
        return frame;
    }

    if (raw[0] != FRAME_START) {
        ESP_LOGD(Tag, "parseFrame: bad START 0x%02X", raw[0]);
        return frame;
    }

    // ✅ Correct field offsets after DST byte addition
    frame.src_device = (DeviceID)raw[1];
    // raw[2] = DST  (we are the destination — could validate here)
    frame.cmd_type   = (CmdType)raw[3];
    frame.data_len   = raw[4];

    // Validate total length: 5 header + data_len + checksum(1) + end(1) = 7 + data_len
    int expected = 5 + frame.data_len + 2;
    if (len < expected) {
        ESP_LOGD(Tag, "parseFrame: incomplete frame exp=%d got=%d", expected, len);
        return frame;
    }

    if (raw[5 + frame.data_len + 1] != FRAME_END) {
        ESP_LOGD(Tag, "parseFrame: bad END byte 0x%02X", raw[5 + frame.data_len + 1]);
        return frame;
    }

    // Copy data (starts at byte 5)
    if (frame.data_len > MAX_FRAME_DATA) frame.data_len = MAX_FRAME_DATA;
    memcpy(frame.data, raw + 5, frame.data_len);

    // Verify checksum
    uint8_t expected_cs = raw[5 + frame.data_len];
    uint8_t calc_cs = (uint8_t)frame.src_device  // SRC
                    ^ raw[2]                       // DST
                    ^ (uint8_t)frame.cmd_type
                    ^ frame.data_len;
    for (uint8_t i = 0; i < frame.data_len; i++) {
        calc_cs ^= frame.data[i];
    }

    if (calc_cs != expected_cs) {
        ESP_LOGW(Tag, "parseFrame: checksum mismatch calc=0x%02X got=0x%02X",
                 calc_cs, expected_cs);
        DebugManager::getInstance().reportError(ErrorCode::CHECKSUM_ERROR,
                                                "UartBridge::parseFrame");
        return frame;
    }

    frame.checksum = expected_cs;
    frame.valid    = true;
    return frame;
}

// =============================================================================
//  init — configure and install UART driver for one port
// =============================================================================

void UartBridge::init(uart_port_t port, int rx_pin, int tx_pin, int baud) {
    ESP_LOGI(Tag, "[ENTRY] init() port=%d rx=%d tx=%d baud=%d",
             (int)port, rx_pin, tx_pin, baud);

    PortCtx* ctx = findPort(port);
    if (!ctx) {
        ESP_LOGE(Tag, "No free port slot for UART%d", (int)port);
        return;
    }

    ctx->port        = port;
    ctx->initialized = true;

    uart_config_t cfg = {};
    cfg.baud_rate  = baud;
    cfg.data_bits  = UART_DATA_8_BITS;
    cfg.parity     = UART_PARITY_DISABLE;
    cfg.stop_bits  = UART_STOP_BITS_1;
    cfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_param_config(port, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(port, tx_pin, rx_pin,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(port, kRxBufSize * 2, 0, 0, nullptr, 0));

    ESP_LOGI(Tag, "UART%d initialized (baud=%d)", (int)port, baud);
    ESP_LOGI(Tag, "[EXIT]  init()");
}

// =============================================================================
//  start — launch RX task for a port
// =============================================================================

void UartBridge::start(uart_port_t port, FrameCallback cb) {
    ESP_LOGI(Tag, "[ENTRY] start() port=%d", (int)port);

    PortCtx* ctx = findPort(port);
    if (!ctx || !ctx->initialized) {
        ESP_LOGE(Tag, "start(): UART%d not initialized", (int)port);
        return;
    }

    if (ctx->running) {
        ESP_LOGW(Tag, "UART%d RX task already running", (int)port);
        return;
    }

    ctx->callback = cb;
    ctx->running  = true;

    char task_name[24];
    snprintf(task_name, sizeof(task_name), "uart_rx_%d", (int)port);

    xTaskCreate(rxTask, task_name, 4096, ctx,
                tskIDLE_PRIORITY + 6, &ctx->task_handle);

    ESP_LOGI(Tag, "UART%d RX task started", (int)port);
    ESP_LOGI(Tag, "[EXIT]  start()");
}

// =============================================================================
//  stop — delete RX task and uninstall driver
// =============================================================================

void UartBridge::stop(uart_port_t port) {
    ESP_LOGI(Tag, "[ENTRY] stop() port=%d", (int)port);

    PortCtx* ctx = findPort(port);
    if (!ctx || !ctx->initialized) return;

    ctx->running = false;

    if (ctx->task_handle) {
        for (int i = 0; i < 20; i++) {
            if (eTaskGetState(ctx->task_handle) == eDeleted) break;
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        ctx->task_handle = nullptr;
    }

    uart_driver_delete(port);
    ctx->initialized = false;

    ESP_LOGI(Tag, "UART%d stopped", (int)port);
    ESP_LOGI(Tag, "[EXIT]  stop()");
}

// =============================================================================
//  rxTask — blocking RX loop, builds frames byte-by-byte
// =============================================================================

void UartBridge::rxTask(void* arg) {
    auto* ctx = static_cast<PortCtx*>(arg);

    ESP_LOGI(Tag, "RX task running on UART%d", (int)ctx->port);

    // Frame assembly buffer — extra 2 bytes for DST field
    static constexpr int kFrameBuf = MAX_FRAME_DATA + 9;
    uint8_t frame_buf[kFrameBuf];
    int     frame_pos = 0;
    bool    in_frame  = false;

    while (ctx->running) {
        uint8_t byte;
        int r = uart_read_bytes(ctx->port, &byte, 1, pdMS_TO_TICKS(100));
        if (r != 1) continue;

        if (!in_frame) {
            if (byte == FRAME_START) {
                frame_buf[0] = byte;
                frame_pos    = 1;
                in_frame     = true;
            }
            continue;
        }

        // Inside frame
        if (frame_pos < kFrameBuf) {
            frame_buf[frame_pos++] = byte;
        } else {
            ESP_LOGW(Tag, "UART%d frame buffer overflow — resetting", (int)ctx->port);
            in_frame  = false;
            frame_pos = 0;
            continue;
        }

        // Need at least 5 bytes to know data_len:
        // [START][SRC][DST][CMD][LEN] = indices 0..4
        if (frame_pos >= 5) {
            uint8_t data_len = frame_buf[4];  // ✅ LEN now at index 4 (was 3)

            // Total: START(1)+SRC(1)+DST(1)+CMD(1)+LEN(1)+DATA(n)+CSUM(1)+END(1) = 7+n
            int expected_total = 7 + data_len;

            if (frame_pos >= expected_total) {
                if (frame_buf[frame_pos - 1] == FRAME_END) {
                    UartFrame frame = parseFrame(frame_buf, frame_pos);
                    if (frame.valid) {
                        ESP_LOGD(Tag, "UART%d valid frame cmd=0x%02X len=%u",
                                 (int)ctx->port, (uint8_t)frame.cmd_type, frame.data_len);
                        if (ctx->callback) ctx->callback(frame);
                    } else {
                        ESP_LOGD(Tag, "UART%d invalid frame dropped", (int)ctx->port);
                    }
                } else {
                    ESP_LOGD(Tag, "UART%d: expected END at pos %d got 0x%02X",
                             (int)ctx->port, frame_pos - 1, frame_buf[frame_pos - 1]);
                }
                in_frame  = false;
                frame_pos = 0;
            }
        }
    }

    ESP_LOGI(Tag, "RX task UART%d exiting", (int)ctx->port);
    vTaskDelete(nullptr);
}

// =============================================================================
//  sendFrame — build and transmit a frame
//
//  Frame layout: [START][SRC][DST][CMD][LEN][DATA...][CHECKSUM][END]
//  FIX: DST byte is now correctly inserted at position 2
// =============================================================================

void UartBridge::sendFrame(uart_port_t port, DeviceID dst, CmdType cmd,
                            const uint8_t* data, uint8_t data_len) {
    ESP_LOGD(Tag, "[ENTRY] sendFrame() port=%d dst=0x%02X cmd=0x%02X len=%u",
             (int)port, (uint8_t)dst, (uint8_t)cmd, data_len);

    PortCtx* ctx = findPort(port);
    if (!ctx || !ctx->initialized) {
        ESP_LOGW(Tag, "sendFrame(): UART%d not initialized", (int)port);
        return;
    }

    if (data_len > MAX_FRAME_DATA) {
        ESP_LOGW(Tag, "sendFrame(): data_len=%u exceeds MAX=%u — truncating",
                 data_len, MAX_FRAME_DATA);
        data_len = MAX_FRAME_DATA;
    }

    // Build: [START][SRC][DST][CMD][LEN][DATA...][CHECKSUM][END]
    uint8_t frame[MAX_FRAME_DATA + 9];
    int     pos = 0;

    frame[pos++] = FRAME_START;
    frame[pos++] = (uint8_t)DeviceID::ROBOT_ESP32;  // SRC
    frame[pos++] = (uint8_t)dst;                     // ✅ DST — was missing before
    frame[pos++] = (uint8_t)cmd;
    frame[pos++] = data_len;

    if (data && data_len > 0) {
        for (uint8_t i = 0; i < data_len; i++) {
            frame[pos++] = data[i];
        }
    }

    frame[pos++] = calcChecksum(data ? data : nullptr, data_len, dst, cmd);  // ✅ dst passed
    frame[pos++] = FRAME_END;

    int written = uart_write_bytes(port, (const char*)frame, pos);
    if (written != pos) {
        ESP_LOGE(Tag, "UART%d write error: wrote %d/%d bytes", (int)port, written, pos);
        DebugManager::getInstance().reportError(ErrorCode::UART_TIMEOUT,
                                                "UartBridge::sendFrame");
    }

    ESP_LOGD(Tag, "[EXIT]  sendFrame() wrote=%d", written);
}

// =============================================================================
//  Convenience senders
// =============================================================================

void UartBridge::sendMoveCmd(uart_port_t port, const MoveCmd& cmd) {
    ESP_LOGD(Tag, "[ENTRY] sendMoveCmd() dir=%u speed=%u",
             (uint8_t)cmd.direction, cmd.speed_percent);
    uint8_t data[2] = {
        (uint8_t)cmd.direction,
        cmd.speed_percent
    };
    sendFrame(port, DeviceID::STM32, CmdType::CMD_MOVE, data, sizeof(data));
    ESP_LOGD(Tag, "[EXIT]  sendMoveCmd()");
}

void UartBridge::sendShoulderCmd(uart_port_t port, const ShoulderCmd& cmd) {
    ESP_LOGD(Tag, "[ENTRY] sendShoulderCmd() dc=%d servo=%d side=%u",
             cmd.dc_angle_deg, cmd.servo_angle_deg, cmd.side);
    // Pack: [dc_lo][dc_hi][servo_lo][servo_hi][side]
    uint8_t data[5];
    data[0] = (uint8_t)(cmd.dc_angle_deg    & 0xFF);
    data[1] = (uint8_t)(cmd.dc_angle_deg    >> 8);
    data[2] = (uint8_t)(cmd.servo_angle_deg & 0xFF);
    data[3] = (uint8_t)(cmd.servo_angle_deg >> 8);
    data[4] = cmd.side;
    sendFrame(port, DeviceID::STM32, CmdType::CMD_SHOULDER, data, sizeof(data));
    ESP_LOGD(Tag, "[EXIT]  sendShoulderCmd()");
}

void UartBridge::sendElbowCmd(uart_port_t port, const ElbowCmd& cmd) {
    ESP_LOGD(Tag, "[ENTRY] sendElbowCmd() angle=%d side=%u",
             cmd.servo_angle_deg, cmd.side);
    uint8_t data[3];
    data[0] = (uint8_t)(cmd.servo_angle_deg & 0xFF);
    data[1] = (uint8_t)(cmd.servo_angle_deg >> 8);
    data[2] = cmd.side;
    sendFrame(port, DeviceID::STM32, CmdType::CMD_ELBOW, data, sizeof(data));
    ESP_LOGD(Tag, "[EXIT]  sendElbowCmd()");
}

void UartBridge::sendWristCmd(uart_port_t port, const WristCmd& cmd) {
    ESP_LOGD(Tag, "[ENTRY] sendWristCmd() steps=%ld speed=%u dir=%u side=%u",
             (long)cmd.steps, cmd.speed_rpm, cmd.direction, cmd.side);
    // Pack: [steps 4 bytes LE][speed 2 bytes LE][dir][side]
    uint8_t data[8];
    int32_t s = cmd.steps;
    data[0] = (uint8_t)(s         & 0xFF);
    data[1] = (uint8_t)((s >> 8)  & 0xFF);
    data[2] = (uint8_t)((s >> 16) & 0xFF);
    data[3] = (uint8_t)((s >> 24) & 0xFF);
    data[4] = (uint8_t)(cmd.speed_rpm & 0xFF);
    data[5] = (uint8_t)(cmd.speed_rpm >> 8);
    data[6] = cmd.direction;
    data[7] = cmd.side;
    sendFrame(port, DeviceID::STM32, CmdType::CMD_WRIST, data, sizeof(data));
    ESP_LOGD(Tag, "[EXIT]  sendWristCmd()");
}

void UartBridge::sendHeartbeat(uart_port_t port) {
    ESP_LOGD(Tag, "[ENTRY] sendHeartbeat() port=%d", (int)port);
    sendFrame(port, DeviceID::STM32, CmdType::CMD_HEARTBEAT, nullptr, 0);
    ESP_LOGD(Tag, "[EXIT]  sendHeartbeat()");
}

void UartBridge::sendEmergencyStop(uart_port_t port) {
    ESP_LOGW(Tag, "!!! sendEmergencyStop() port=%d !!!", (int)port);
    // Send E-STOP twice for reliability
    sendFrame(port, DeviceID::STM32, CmdType::CMD_STOP_ALL, nullptr, 0);
    sendFrame(port, DeviceID::STM32, CmdType::CMD_STOP_ALL, nullptr, 0);
    ESP_LOGW(Tag, "E-STOP frames sent");
}

} // namespace SARCUS
