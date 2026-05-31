#pragma once

// =============================================================================
//  SARCUS Robot — MQTT Manager
//  Connects to Mosquitto broker. Handles all pub/sub for robot control.
//  Dispatches received commands via registered callbacks.
// =============================================================================

#include "mqtt_client.h"
#include "SarcusTypes.h"
#include <string>
#include <functional>
#include <map>

namespace SARCUS {

// Callback types for incoming MQTT commands
using MoveCallback       = std::function<void(const MoveCmd&)>;
using ShoulderCallback   = std::function<void(const ShoulderCmd&)>;
using ElbowCallback      = std::function<void(const ElbowCmd&)>;
using WristCallback      = std::function<void(const WristCmd&)>;
using SuitButtonCallback = std::function<void(uint32_t buttons)>;
using SuitPotCallback    = std::function<void(const uint16_t* pots, uint8_t count)>;
using EStopCallback      = std::function<void()>;

class MqttManager {
public:
    static MqttManager& getInstance();

    // Start MQTT client. Broker URI e.g. "mqtt://192.168.1.100:1883"
    void start(const char* broker_uri,
               const char* username = nullptr,
               const char* password = nullptr);

    // Stop and cleanup
    void stop();

    // Publish helpers
    void publish(const char* topic, const std::string& payload, int qos = 1);
    void publishIMU        (const ImuData& imu);
    void publishGPS        (const GpsData& gps);
    void publishUltrasonic (const UltrasonicData& us);
    void publishHeartbeat  (const std::string& device_id);
    void publishDebugLog   (const std::string& msg);

    // Register command callbacks
    void onMove      (MoveCallback       cb) { m_cb_move       = cb; }
    void onShoulder  (ShoulderCallback   cb) { m_cb_shoulder   = cb; }
    void onElbow     (ElbowCallback      cb) { m_cb_elbow      = cb; }
    void onWrist     (WristCallback      cb) { m_cb_wrist      = cb; }
    void onSuitButton(SuitButtonCallback cb) { m_cb_suit_btn   = cb; }
    void onSuitPot   (SuitPotCallback    cb) { m_cb_suit_pot   = cb; }
    void onEStop     (EStopCallback      cb) { m_cb_estop      = cb; }

    bool isConnected() const { return m_connected; }

private:
    MqttManager()  = default;
    ~MqttManager() { stop(); }
    MqttManager(const MqttManager&) = delete;
    MqttManager& operator=(const MqttManager&) = delete;

    static void eventHandler(void* arg, esp_event_base_t base,
                             int32_t event_id, void* event_data);

    // JSON parsers for incoming topics
    void dispatchMessage(const char* topic, int topic_len,
                         const char* data,  int data_len,
                         int data_offset = 0);

    bool parseMoveCmd    (const char* json, MoveCmd&     out);
    bool parseShoulderCmd(const char* json, ShoulderCmd& out);
    bool parseElbowCmd   (const char* json, ElbowCmd&    out);
    bool parseWristCmd   (const char* json, WristCmd&    out);

    esp_mqtt_client_handle_t m_client = nullptr;
    std::string              m_broker_uri_storage;
    bool                     m_connected = false;

    // Callbacks
    MoveCallback       m_cb_move;
    ShoulderCallback   m_cb_shoulder;
    ElbowCallback      m_cb_elbow;
    WristCallback      m_cb_wrist;
    SuitButtonCallback m_cb_suit_btn;
    SuitPotCallback    m_cb_suit_pot;
    EStopCallback      m_cb_estop;
};

} // namespace SARCUS
