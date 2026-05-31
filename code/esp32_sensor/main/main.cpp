// =============================================================================
//  SARCUS Robot — Main Entry Point (Robot ESP32)
//  Target  : ESP32 — ESP-IDF v5.x (C++17)
//
//  Boot Flow:
//   1. NVS init
//   2. WiFi: check NVS credentials
//      → First boot / no creds: AP mode + captive portal
//      → Has creds: STA mode → connect
//      → Failed STA: AP fallback
//   3. MQTT connect → subscribe to all control topics
//   4. UART init:
//      → UART1: ESP32 ↔ STM32 (motor control)
//      → UART2: ESP32 ↔ RPi   (ROS2 bridge)
//   5. Motor Controller init + start
//   6. Sensor Manager init + start
//   7. Debug Manager init (routes logs to MQTT)
//   8. Heartbeat task start (500ms)
//   9. Main loop: heartbeat watchdog
//
//  Control Modes (all active simultaneously):
//   ─ Node-RED MQTT Dashboard  → MqttManager callbacks → MotorController
//   ─ Suit ESP32               → MQTT suit topics      → MotorController
//   ─ ROS2 (RPi)               → UartBridge UART2      → MotorController
//
//  Pin Assignments (configure below):
// =============================================================================

#include "inc/NVSManager.h"
#include "inc/WifiManager.h"
#include "inc/WebServer.h"
#include "inc/MqttManager.h"
#include "inc/UartBridge.h"
#include "inc/MotorController.h"
#include "inc/SensorManager.h"
#include "inc/DebugManager.h"
#include "inc/HeartbeatManager.h"
#include "inc/SarcusTypes.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>

static const char* TAG = "main";

// =============================================================================
//  ── PIN CONFIGURATION ────────────────────────────────────────────────────────
// =============================================================================

// UART1 → STM32F405 (motor control)
static constexpr uart_port_t STM32_UART_PORT  = UART_NUM_1;
static constexpr int         STM32_TX_PIN     = 17;
static constexpr int         STM32_RX_PIN     = 16;
static constexpr int         STM32_BAUD       = 115200;

// UART2 → NEO-7M GPS
// FIX: GPS moved from UART_NUM_0 (USB Serial) to UART_NUM_2.
// UART_NUM_0 = USB Serial port (TX=GPIO1, RX=GPIO3) — cannot be used for GPS.
// RPi ROS2 bridge is handled over WiFi/MQTT (not UART) in this phase.
static constexpr uart_port_t GPS_UART_PORT    = UART_NUM_2;
static constexpr int         GPS_RX_PIN       = 18;
static constexpr int         GPS_TX_PIN       = 19;
static constexpr int         GPS_BAUD         = 9600;

// RPi ROS2 bridge — DISABLED in this phase (uses WiFi MQTT instead of UART)
// Uncomment and assign pins when hardware serial bridge to RPi is wired.
// static constexpr uart_port_t RPI_UART_PORT = UART_NUM_X;
// static constexpr int         RPI_TX_PIN    = X;
// static constexpr int         RPI_RX_PIN    = X;
// static constexpr int         RPI_BAUD      = 115200;

// I2C → MPU6050 (IMU)
static constexpr i2c_port_t  I2C_PORT         = I2C_NUM_0;
static constexpr int         I2C_SDA_PIN      = 21;
static constexpr int         I2C_SCL_PIN      = 22;

// HC-SR04 Ultrasonic (TRIG/ECHO pairs)
static constexpr int US_TRIG_FRONT = 25, US_ECHO_FRONT = 26;
static constexpr int US_TRIG_BACK  = 27, US_ECHO_BACK  = 14;
static constexpr int US_TRIG_LEFT  = 12, US_ECHO_LEFT  = 13;
static constexpr int US_TRIG_RIGHT = 32, US_ECHO_RIGHT = 33;

// MQTT Broker (Mosquitto running on RPi or local network)
static constexpr const char* MQTT_BROKER_URI = "mqtt://192.168.1.4:1883";
// static constexpr const char* MQTT_USER     = "sarcus";
// static constexpr const char* MQTT_PASS     = "sarcus_pass";

// =============================================================================
//  ── HELPERS ──────────────────────────────────────────────────────────────────
// =============================================================================

static std::string readDeviceId() {
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(buf);
}

static std::string readNvsBroker() {
    SARCUS::NVSHandle handle("wifi_config", NVS_READONLY);
    std::string broker;
    if (handle.isValid() && handle.getString("broker", broker) == ESP_OK && !broker.empty()) {
        return broker;
    }
    return MQTT_BROKER_URI;
}

// WiFi provisioning logic (AP → AP+STA with web status)
static bool connectWifi(SARCUS::WifiManager& wifi) {
    std::string nvs_ssid, nvs_pass;

    SARCUS::NVSHandle handle("wifi_config", NVS_READONLY);
    bool has_creds = handle.isValid()
                  && handle.getString("ssid",     nvs_ssid) == ESP_OK
                  && handle.getString("password", nvs_pass) == ESP_OK
                  && !nvs_ssid.empty();

    if (!has_creds) {
        ESP_LOGI(TAG, "No WiFi credentials → provisioning mode");
        ESP_LOGI(TAG, "Connect to: SARCUS_SETUP  Password: sarcus2024");

        wifi.startAP();
        SARCUS::WebServer::getInstance().start();

        std::string pending_ssid, pending_pass, pending_broker;
        while (!SARCUS::WebServer::getInstance().getPendingWifiCredentials(
                   pending_ssid, pending_pass, pending_broker)) {
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        ESP_LOGI(TAG, "Provisioning complete — applying STA credentials");

        // Start AP+STA dual mode — WebServer stays up on AP
        wifi.startSTA_keepAP(pending_ssid, pending_pass);
        if (wifi.waitForIP(pdMS_TO_TICKS(30000))) {
            std::string ip = wifi.getIP();
            ESP_LOGI(TAG, "Connected! IP: %s", ip.c_str());
            SARCUS::WebServer::setStaConnectionInfo(ip);
            return true;
        }

        // STA failed → fallback to AP-only
        ESP_LOGW(TAG, "STA connect failed — AP fallback");
        wifi.startAP();
        return false;
    }

    // STA mode with saved credentials
    ESP_LOGI(TAG, "Connecting to: %s", nvs_ssid.c_str());
    wifi.startSTA(nvs_ssid, nvs_pass);

    if (wifi.waitForIP(pdMS_TO_TICKS(15000))) {
        std::string ip = wifi.getIP();
        ESP_LOGI(TAG, "Connected! IP: %s", ip.c_str());
        return true;
    }

    ESP_LOGW(TAG, "STA connect failed — AP fallback");
    wifi.startAP();
    SARCUS::WebServer::getInstance().start();
    wifi.waitForIP(pdMS_TO_TICKS(1000));
    return false;
}

// =============================================================================
//  ── app_main ─────────────────────────────────────────────────────────────────
// =============================================================================

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "  SARCUS Robot — ESP32 Robot Node          ");
    ESP_LOGI(TAG, "  Assistive Exoskeleton Control System     ");
    ESP_LOGI(TAG, "============================================");

    // ══════════════════════════════════════════════════════════════════════════
    //  STEP 1 — NVS
    // ══════════════════════════════════════════════════════════════════════════
    auto& nvs = SARCUS::NVSManager::getInstance();
    nvs.init();
    ESP_LOGI(TAG, "[1/8] NVS ready ✓");

    // ══════════════════════════════════════════════════════════════════════════
    //  STEP 2 — Device Identity
    // ══════════════════════════════════════════════════════════════════════════
    std::string device_id = readDeviceId();
    ESP_LOGI(TAG, "[2/8] Device ID: %s", device_id.c_str());

    // ══════════════════════════════════════════════════════════════════════════
    //  STEP 3 — WiFi (AP provisioning or STA)
    // ══════════════════════════════════════════════════════════════════════════
    auto& wifi = SARCUS::WifiManager::getInstance();
    wifi.init();
    bool wifi_ok = connectWifi(wifi);
    if (!wifi_ok) {
        ESP_LOGE(TAG, "[3/8] WiFi failed ✗");
        ESP_LOGE(TAG, "Cannot continue to MQTT or network services without STA connection");
        return;
    }
    ESP_LOGI(TAG, "[3/8] WiFi connected ✓");

    // ══════════════════════════════════════════════════════════════════════════
    //  STEP 4 — MQTT + callback registration
    // ══════════════════════════════════════════════════════════════════════════
    auto& mqtt  = SARCUS::MqttManager::getInstance();
    auto& motor = SARCUS::MotorController::getInstance();

    // Register MQTT → MotorController callbacks
    mqtt.onMove([&motor](const SARCUS::MoveCmd& cmd) {
        motor.submitMove(cmd, SARCUS::ControlSource::MQTT);
    });

    mqtt.onShoulder([&motor](const SARCUS::ShoulderCmd& cmd) {
        motor.submitShoulder(cmd, SARCUS::ControlSource::MQTT);
    });

    mqtt.onElbow([&motor](const SARCUS::ElbowCmd& cmd) {
        motor.submitElbow(cmd, SARCUS::ControlSource::MQTT);
    });

    mqtt.onWrist([&motor](const SARCUS::WristCmd& cmd) {
        motor.submitWrist(cmd, SARCUS::ControlSource::MQTT);
    });

    mqtt.onSuitButton([&motor](uint32_t buttons) {
        // Map buttons to movement commands
        // Bits: 0=FWD, 1=BWD, 2=LEFT, 3=RIGHT, 4=STOP, 5=ESTOP
        if (buttons & (1 << 5)) {
            motor.triggerEStop();
            return;
        }
        SARCUS::MoveCmd mv = {};
        mv.speed_percent = 60;
        if      (buttons & (1 << 0)) mv.direction = SARCUS::MoveDir::FORWARD;
        else if (buttons & (1 << 1)) mv.direction = SARCUS::MoveDir::BACKWARD;
        else if (buttons & (1 << 2)) mv.direction = SARCUS::MoveDir::LEFT;
        else if (buttons & (1 << 3)) mv.direction = SARCUS::MoveDir::RIGHT;
        else                          mv.direction = SARCUS::MoveDir::STOP;
        motor.submitMove(mv, SARCUS::ControlSource::SUIT);
    });

    mqtt.onSuitPot([&motor](const uint16_t* pots, uint8_t count) {
        // Map pots 0-3 → shoulder left/right, 4-5 → elbow, 6-7 → wrist
        if (count < 8) return;

        // Shoulder left (pot 0 = DC, pot 1 = servo)
        SARCUS::ShoulderCmd sh_l = {};
        sh_l.dc_angle_deg    = (int16_t)((pots[0] / 4095.0f) * 360.0f - 180.0f);
        sh_l.servo_angle_deg = (int16_t)((pots[1] / 4095.0f) * 270.0f);
        sh_l.side = 0;
        motor.submitShoulder(sh_l, SARCUS::ControlSource::SUIT);

        // Elbow left (pot 4)
        SARCUS::ElbowCmd el_l = {};
        el_l.servo_angle_deg = (int16_t)((pots[4] / 4095.0f) * 270.0f);
        el_l.side = 0;
        motor.submitElbow(el_l, SARCUS::ControlSource::SUIT);
    });

    mqtt.onEStop([&motor]() {
        motor.triggerEStop();
    });

    {
        std::string broker_uri = readNvsBroker();
        ESP_LOGI(TAG, "MQTT broker: %s", broker_uri.c_str());
        mqtt.start(broker_uri.c_str());
    }
    ESP_LOGI(TAG, "[4/8] MQTT connected ✓");

    // ══════════════════════════════════════════════════════════════════════════
    //  STEP 5a — Motor Controller (MUST BE BEFORE uart.start!)
    // ══════════════════════════════════════════════════════════════════════════
    // Motor init must happen before UART RX tasks start, because the STM32
    // UART callback can call motor.triggerEStop() immediately upon receiving
    // a CMD_STOP_ALL or motor fault notification.
    motor.init(STM32_UART_PORT);
    motor.start();
    ESP_LOGI(TAG, "[5a/9] Motor controller ready ✓");

    // ══════════════════════════════════════════════════════════════════════════
    //  STEP 5b — UART Bridge (STM32 + RPi)
    // ══════════════════════════════════════════════════════════════════════════
    auto& uart = SARCUS::UartBridge::getInstance();

    // STM32 port
    uart.init(STM32_UART_PORT, STM32_RX_PIN, STM32_TX_PIN, STM32_BAUD);
    uart.start(STM32_UART_PORT, [](const SARCUS::UartFrame& frame) {
        // Handle frames coming back FROM STM32
        // (status, fault notifications, encoder feedback, etc.)
        ESP_LOGD("uart_stm32", "Frame from STM32: cmd=0x%02X len=%u",
                 (uint8_t)frame.cmd_type, frame.data_len);

        if (frame.cmd_type == SARCUS::CmdType::CMD_HEARTBEAT) {
            ESP_LOGD("uart_stm32", "STM32 heartbeat received");
        }
        else if (frame.cmd_type == SARCUS::CmdType::CMD_STOP_ALL) {
            // STM32 requested emergency stop (motor fault)
            ESP_LOGW("uart_stm32", "STM32 requested ESTOP — triggering");
            SARCUS::MotorController::getInstance().triggerEStop();
        }
    });

    // RPi / ROS2 UART bridge — DISABLED: RPi communicates via WiFi/MQTT in this phase.
    // When hardware UART to RPi is wired, re-enable and assign RPI_UART_PORT.
    ESP_LOGI(TAG, "[5b/9] UART bridges ready (STM32 UART%d) ✓ | RPi=WiFi/MQTT",
             (int)STM32_UART_PORT);

    // ══════════════════════════════════════════════════════════════════════════
    //  STEP 6 — Sensor Manager
    // ══════════════════════════════════════════════════════════════════════════
    auto& sensors = SARCUS::SensorManager::getInstance();
    sensors.init(
        I2C_PORT,      I2C_SDA_PIN, I2C_SCL_PIN,
        GPS_UART_PORT, GPS_RX_PIN,  GPS_TX_PIN,
        US_TRIG_FRONT, US_ECHO_FRONT,
        US_TRIG_BACK,  US_ECHO_BACK,
        US_TRIG_LEFT,  US_ECHO_LEFT,
        US_TRIG_RIGHT, US_ECHO_RIGHT
    );
    sensors.start(
        100,   // IMU: 10Hz
        1000,  // GPS: 1Hz
        200    // Ultrasonic: 5Hz
    );
    ESP_LOGI(TAG, "[6/9] Sensors running ✓");

    // ══════════════════════════════════════════════════════════════════════════
    //  STEP 7 — Debug Manager + Heartbeat
    // ══════════════════════════════════════════════════════════════════════════
    auto& dbg = SARCUS::DebugManager::getInstance();
    dbg.init();

    auto& hb = SARCUS::HeartbeatManager::getInstance();
    hb.start(device_id);

    ESP_LOGI(TAG, "[7/9] Debug + Heartbeat running ✓");

    // ══════════════════════════════════════════════════════════════════════════
    //  MAIN LOOP — Watchdog heartbeat
    // ══════════════════════════════════════════════════════════════════════════
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "  SARCUS Robot fully operational           ");
    ESP_LOGI(TAG, "  Controls: MQTT + Suit + ROS2             ");
    ESP_LOGI(TAG, "  Broker: %s", MQTT_BROKER_URI);
    ESP_LOGI(TAG, "============================================");

    uint32_t tick = 0;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        tick++;

        ESP_LOGI(TAG, "♥ Heartbeat tick=%lu | WiFi=%s | MQTT=%s | EStop=%s",
                 (unsigned long)tick,
                 wifi.isConnected()  ? "OK" : "FAIL",
                 mqtt.isConnected()  ? "OK" : "FAIL",
                 motor.getActiveSource() == SARCUS::ControlSource::ESTOP ? "ACTIVE" : "clear");

        // Send periodic heartbeat to STM32 over UART
        uart.sendHeartbeat(STM32_UART_PORT);
    }
}
