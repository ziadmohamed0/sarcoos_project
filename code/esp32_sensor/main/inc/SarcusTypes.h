#pragma once

// =============================================================================
//  SARCUS Robot — Common Types & Definitions
//  Target : ESP32 (Robot Node) — ESP-IDF + C++17
//
//  All shared enums, structs, and constants across modules.
// =============================================================================

#include <cstdint>
#include <string>

namespace SARCUS {

// ─── UART Frame Protocol ──────────────────────────────────────────────────────
// [0xAA][DEVICE_ID][CMD_TYPE][DATA_LEN][DATA...][CHECKSUM][0x55]
static constexpr uint8_t  FRAME_START    = 0xAA;
static constexpr uint8_t  FRAME_END      = 0x55;
static constexpr uint8_t  MAX_FRAME_DATA = 64;

// Device IDs
enum class DeviceID : uint8_t {
    ROBOT_ESP32  = 0x01,
    STM32        = 0x02,
    RPI          = 0x03,
    SUIT_ESP32   = 0x04,
};

// Command types
enum class CmdType : uint8_t {
    // Locomotion
    CMD_MOVE         = 0x10,   // movement (fwd/bwd/left/right/stop)
    CMD_STOP_ALL     = 0x11,   // emergency stop

    // Joints
    CMD_SHOULDER     = 0x20,   // shoulder DC + servo
    CMD_ELBOW        = 0x21,   // elbow servo 35kg
    CMD_WRIST        = 0x22,   // wrist stepper NEMA23

    // Sensors
    DATA_IMU         = 0x30,   // MPU6050 data
    DATA_GPS         = 0x31,   // NEO-7M data
    DATA_ULTRASONIC  = 0x32,   // 4x HC-SR04 data

    // System
    CMD_HEARTBEAT    = 0x40,
    CMD_DEBUG_LOG    = 0x41,
    CMD_STATUS_REQ   = 0x42,
    CMD_STATUS_RESP  = 0x43,
};

// Error codes (unified across all devices)
enum class ErrorCode : uint8_t {
    OK               = 0x00,
    UART_TIMEOUT     = 0x01,
    MOTOR_FAULT      = 0x02,
    IMU_FAIL         = 0x03,
    GPS_NO_FIX       = 0x04,
    WIFI_DISCONNECT  = 0x05,
    MQTT_DISCONNECT  = 0x06,
    CHECKSUM_ERROR   = 0x07,
    INVALID_FRAME    = 0x08,
};

// ─── Movement Command ─────────────────────────────────────────────────────────
enum class MoveDir : uint8_t {
    STOP    = 0,
    FORWARD = 1,
    BACKWARD= 2,
    LEFT    = 3,
    RIGHT   = 4,
};

struct MoveCmd {
    MoveDir  direction;
    uint8_t  speed_percent;  // 0-100
};

// ─── Joint Command ────────────────────────────────────────────────────────────
struct ShoulderCmd {
    int16_t  dc_angle_deg;      // DC motor shoulder: -180 to +180
    int16_t  servo_angle_deg;   // Feetech 95kg: 0 to 270
    uint8_t  side;              // 0=left, 1=right
};

struct ElbowCmd {
    int16_t  servo_angle_deg;   // Feetech 35kg: 0 to 270
    uint8_t  side;              // 0=left, 1=right
};

struct WristCmd {
    int32_t  steps;             // NEMA23 stepper steps
    uint16_t speed_rpm;
    uint8_t  direction;         // 0=CW, 1=CCW
    uint8_t  side;              // 0=left, 1=right
};

// ─── Sensor Data ──────────────────────────────────────────────────────────────
struct ImuData {
    float    accel_x, accel_y, accel_z;
    float    gyro_x,  gyro_y,  gyro_z;
    float    roll, pitch, yaw;
    bool     valid;
};

struct GpsData {
    double   latitude;
    double   longitude;
    float    altitude;
    uint8_t  satellites;
    bool     fix;
};

struct UltrasonicData {
    float    front_cm;
    float    back_cm;
    float    left_cm;
    float    right_cm;
};

// ─── Suit Input Frame ─────────────────────────────────────────────────────────
struct SuitFrame {
    uint32_t button_mask;       // bitmask of 20 buttons (bits 0-19)
    uint16_t pot_values[20];    // 0-4095 ADC values for 20 pots
    bool     valid;
};

// ─── UART Frame ───────────────────────────────────────────────────────────────
struct UartFrame {
    DeviceID  src_device;
    CmdType   cmd_type;
    uint8_t   data_len;
    uint8_t   data[MAX_FRAME_DATA];
    uint8_t   checksum;
    bool      valid;
};

// ─── MQTT Topics ─────────────────────────────────────────────────────────────
namespace Topics {
    // Commands (subscribe)
    constexpr const char* MOVEMENT       = "sarcus/robot/movement";
    constexpr const char* SHOULDER       = "sarcus/robot/joints/shoulder";
    constexpr const char* ELBOW          = "sarcus/robot/joints/elbow";
    constexpr const char* WRIST          = "sarcus/robot/joints/wrist";

    // Sensor data (publish)
    constexpr const char* IMU            = "sarcus/sensors/imu";
    constexpr const char* GPS            = "sarcus/sensors/gps";
    constexpr const char* ULTRASONIC     = "sarcus/sensors/ultrasonic";

    // Suit data (subscribe)
    constexpr const char* SUIT_BUTTONS   = "sarcus/suit/buttons";
    constexpr const char* SUIT_POTS      = "sarcus/suit/pots";

    // Debug
    constexpr const char* DEBUG_LOGS     = "sarcus/debug/logs";
    constexpr const char* HEARTBEAT      = "sarcus/debug/heartbeat";
    constexpr const char* EMERGENCY_STOP = "sarcus/robot/estop";
} // namespace Topics

} // namespace SARCUS
