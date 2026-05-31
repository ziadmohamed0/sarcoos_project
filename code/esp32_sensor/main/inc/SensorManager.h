#pragma once

// =============================================================================
//  SARCUS Robot — Sensor Manager
//  Reads MPU6050 (IMU), NEO-7M (GPS), 4x HC-SR04 (ultrasonic).
//  Publishes to MQTT on a configurable interval.
// =============================================================================

#include "SarcusTypes.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include <functional>

namespace SARCUS {

using ImuCallback         = std::function<void(const ImuData&)>;
using GpsCallback         = std::function<void(const GpsData&)>;
using UltrasonicCallback  = std::function<void(const UltrasonicData&)>;

class SensorManager {
public:
    static SensorManager& getInstance();

    // Initialize I2C (IMU), UART (GPS), GPIO (ultrasonic)
    void init(
        i2c_port_t  i2c_port,       int sda_pin, int scl_pin,  // MPU6050
        uart_port_t gps_uart_port,  int gps_rx,  int gps_tx,   // NEO-7M
        int trig_front, int echo_front,                          // ultrasonic front
        int trig_back,  int echo_back,                           // ultrasonic back
        int trig_left,  int echo_left,                           // ultrasonic left
        int trig_right, int echo_right                           // ultrasonic right
    );

    // Start sensor polling tasks
    void start(uint32_t imu_interval_ms       = 100,
               uint32_t gps_interval_ms       = 1000,
               uint32_t ultrasonic_interval_ms= 200);

    void stop();

    // Register callbacks for sensor data
    void onImu        (ImuCallback         cb) { m_cb_imu  = cb; }
    void onGps        (GpsCallback         cb) { m_cb_gps  = cb; }
    void onUltrasonic (UltrasonicCallback  cb) { m_cb_us   = cb; }

    // Get latest cached readings
    ImuData        getImu()        const { return m_imu; }
    GpsData        getGps()        const { return m_gps; }
    UltrasonicData getUltrasonic() const { return m_us;  }

private:
    SensorManager()  = default;
    ~SensorManager() = default;
    SensorManager(const SensorManager&) = delete;
    SensorManager& operator=(const SensorManager&) = delete;

    // IMU (MPU6050 via I2C)
    bool     initMPU6050();
    void     calibrateGyro();
    bool     readMPU6050(ImuData& out);
    static void imuTask(void* arg);

    // GPS (NEO-7M via UART)
    bool     parseNMEA(const char* sentence, GpsData& out);
    double   applyMedianFilter(double val, double* buffer);
    static void gpsTask(void* arg);

    // Ultrasonic (HC-SR04 via GPIO)
    float    measureDistanceCm(gpio_num_t trig, gpio_num_t echo);
    static void ultrasonicTask(void* arg);

    // Cached sensor values
    ImuData        m_imu  = {};
    GpsData        m_gps  = {};
    UltrasonicData m_us   = {};

    // Callbacks
    ImuCallback        m_cb_imu;
    GpsCallback        m_cb_gps;
    UltrasonicCallback m_cb_us;

    // Task handles
    TaskHandle_t m_imu_task = nullptr;
    TaskHandle_t m_gps_task = nullptr;
    TaskHandle_t m_us_task  = nullptr;
    bool         m_running  = false;

    // Hardware config
    i2c_port_t   m_i2c_port   = I2C_NUM_0;
    uart_port_t  m_gps_uart   = UART_NUM_2;
    gpio_num_t   m_trig[4]    = {};
    gpio_num_t   m_echo[4]    = {};

    uint32_t     m_imu_interval_ms = 100;
    uint32_t     m_gps_interval_ms = 1000;
    uint32_t     m_us_interval_ms  = 200;

    // MPU6050 I2C address
    static constexpr uint8_t  MPU6050_ADDR   = 0x68;
    static constexpr uint32_t I2C_TIMEOUT_MS = 1000;

    // ─── DSP Filter State ──────────────────────────────────────────────────────

    // IMU complementary filter
    float   m_imu_roll  = 0.0f;
    float   m_imu_pitch = 0.0f;
    float   m_imu_yaw   = 0.0f;
    float   m_ax_filt   = 0.0f;
    float   m_ay_filt   = 0.0f;
    float   m_az_filt   = 0.0f;
    float   m_gx_bias   = 0.0f;
    float   m_gy_bias   = 0.0f;
    float   m_gz_bias   = 0.0f;
    bool    m_gyro_calibrated = false;
    int64_t m_last_imu_ts     = 0;

    // Filter coefficients
    static constexpr float IMU_CF_ALPHA        = 0.98f;
    static constexpr float IMU_ACCEL_LP_ALPHA  = 0.15f;
    static constexpr int   IMU_GYRO_CALIB_SAMPLES = 200;

    // GPS median filter
    static constexpr int GPS_MEDIAN_WINDOW = 5;
    double  m_gps_lat_buf[GPS_MEDIAN_WINDOW] = {};
    double  m_gps_lon_buf[GPS_MEDIAN_WINDOW] = {};
    int     m_gps_hist_idx = 0;
};

} // namespace SARCUS
