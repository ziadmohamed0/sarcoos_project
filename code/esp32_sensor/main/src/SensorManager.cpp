// =============================================================================
//  SARCUS Robot — Sensor Manager Implementation
//  MPU6050 (I2C) + NEO-7M GPS (UART) + 4x HC-SR04 (GPIO)
// =============================================================================

#include "inc/SensorManager.h"
#include "inc/MqttManager.h"
#include "inc/DebugManager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "rom/ets_sys.h"
#include <cstring>
#include <cstdlib>
#include <cmath>

namespace SARCUS {

static const char* Tag = "SensorManager";

// ─── Singleton ────────────────────────────────────────────────────────────────

SensorManager& SensorManager::getInstance() {
    static SensorManager instance;
    return instance;
}

// ─── init ─────────────────────────────────────────────────────────────────────

void SensorManager::init(
    i2c_port_t  i2c_port, int sda_pin, int scl_pin,
    uart_port_t gps_uart, int gps_rx,  int gps_tx,
    int trig_front, int echo_front,
    int trig_back,  int echo_back,
    int trig_left,  int echo_left,
    int trig_right, int echo_right)
{
    ESP_LOGI(Tag, "[ENTRY] init()");

    m_i2c_port = i2c_port;
    m_gps_uart = gps_uart;

    // — I2C for MPU6050 ───────────────────────────────────────────────────────
    i2c_config_t i2c_cfg = {};
    i2c_cfg.mode             = I2C_MODE_MASTER;
    i2c_cfg.sda_io_num       = static_cast<gpio_num_t>(sda_pin);
    i2c_cfg.scl_io_num       = static_cast<gpio_num_t>(scl_pin);
    i2c_cfg.sda_pullup_en    = GPIO_PULLUP_ENABLE;
    i2c_cfg.scl_pullup_en    = GPIO_PULLUP_ENABLE;
    i2c_cfg.master.clk_speed = 400000;
    ESP_ERROR_CHECK(i2c_param_config(i2c_port, &i2c_cfg));
    ESP_ERROR_CHECK(i2c_driver_install(i2c_port, I2C_MODE_MASTER, 0, 0, 0));

    if (initMPU6050()) {
        ESP_LOGI(Tag, "MPU6050 initialized OK");
    } else {
        ESP_LOGE(Tag, "MPU6050 init FAILED");
        DebugManager::getInstance().reportError(ErrorCode::IMU_FAIL, "SensorManager::init");
    }

    // — UART for NEO-7M GPS ───────────────────────────────────────────────────
    uart_config_t gps_cfg = {};
    gps_cfg.baud_rate  = 9600;
    gps_cfg.data_bits  = UART_DATA_8_BITS;
    gps_cfg.parity     = UART_PARITY_DISABLE;
    gps_cfg.stop_bits  = UART_STOP_BITS_1;
    gps_cfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    gps_cfg.source_clk = UART_SCLK_DEFAULT;
    ESP_ERROR_CHECK(uart_param_config(gps_uart, &gps_cfg));
    ESP_ERROR_CHECK(uart_set_pin(gps_uart, gps_tx, gps_rx,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(gps_uart, 1024, 0, 0, nullptr, 0));
    ESP_LOGI(Tag, "GPS UART initialized (baud=9600)");

    // — GPIO for HC-SR04 ultrasonic ───────────────────────────────────────────
    int trigs[4]  = { trig_front, trig_back, trig_left, trig_right };
    int echos[4]  = { echo_front, echo_back, echo_left, echo_right };
    for (int i = 0; i < 4; i++) {
        m_trig[i] = (gpio_num_t)trigs[i];
        m_echo[i] = (gpio_num_t)echos[i];

        gpio_config_t trig_conf = {};
        trig_conf.pin_bit_mask = (1ULL << trigs[i]);
        trig_conf.mode         = GPIO_MODE_OUTPUT;
        gpio_config(&trig_conf);
        gpio_set_level(m_trig[i], 0);

        gpio_config_t echo_conf = {};
        echo_conf.pin_bit_mask = (1ULL << echos[i]);
        echo_conf.mode         = GPIO_MODE_INPUT;
        gpio_config(&echo_conf);
    }
    ESP_LOGI(Tag, "4x HC-SR04 GPIO configured");
    ESP_LOGI(Tag, "[EXIT]  init()");
}

// ─── MPU6050 ─────────────────────────────────────────────────────────────────

bool SensorManager::initMPU6050() {
    // Wake up MPU6050 (write 0 to PWR_MGMT_1 register 0x6B)
    uint8_t data[2] = { 0x6B, 0x00 };
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, 2, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(m_i2c_port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (err == ESP_OK) {
        calibrateGyro();
    }
    return (err == ESP_OK);
}

void SensorManager::calibrateGyro() {
    float sum_gx = 0, sum_gy = 0, sum_gz = 0;
    float gx, gy, gz;

    for (int i = 0; i < IMU_GYRO_CALIB_SAMPLES; i++) {
        uint8_t reg = 0x43;
        uint8_t raw[6] = {};
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, reg, true);
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_READ, true);
        i2c_master_read(cmd, raw, 6, I2C_MASTER_LAST_NACK);
        i2c_master_stop(cmd);
        if (i2c_master_cmd_begin(m_i2c_port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS)) == ESP_OK) {
            gx = (int16_t)((raw[0] << 8) | raw[1]) / 131.0f;
            gy = (int16_t)((raw[2] << 8) | raw[3]) / 131.0f;
            gz = (int16_t)((raw[4] << 8) | raw[5]) / 131.0f;
            sum_gx += gx; sum_gy += gy; sum_gz += gz;
        }
        i2c_cmd_link_delete(cmd);
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    m_gx_bias = sum_gx / IMU_GYRO_CALIB_SAMPLES;
    m_gy_bias = sum_gy / IMU_GYRO_CALIB_SAMPLES;
    m_gz_bias = sum_gz / IMU_GYRO_CALIB_SAMPLES;
    m_gyro_calibrated = true;
    ESP_LOGI(Tag, "Gyro bias: x=%.3f y=%.3f z=%.3f", m_gx_bias, m_gy_bias, m_gz_bias);
}

bool SensorManager::readMPU6050(ImuData& out) {
    uint8_t reg = 0x3B;
    uint8_t raw[14] = {};

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, raw, 13, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &raw[13], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(m_i2c_port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK) {
        out.valid = false;
        return false;
    }

    auto to_int16 = [](uint8_t h, uint8_t l) -> int16_t {
        return (int16_t)((h << 8) | l);
    };

    float ax_raw = to_int16(raw[0],  raw[1])  / 16384.0f;
    float ay_raw = to_int16(raw[2],  raw[3])  / 16384.0f;
    float az_raw = to_int16(raw[4],  raw[5])  / 16384.0f;
    float gx_raw = to_int16(raw[8],  raw[9])  / 131.0f;
    float gy_raw = to_int16(raw[10], raw[11]) / 131.0f;
    float gz_raw = to_int16(raw[12], raw[13]) / 131.0f;

    out.accel_x = ax_raw;
    out.accel_y = ay_raw;
    out.accel_z = az_raw;

    // Gyro bias subtraction
    if (m_gyro_calibrated) {
        gx_raw -= m_gx_bias;
        gy_raw -= m_gy_bias;
        gz_raw -= m_gz_bias;
    }
    out.gyro_x = gx_raw;
    out.gyro_y = gy_raw;
    out.gyro_z = gz_raw;

    // DT calculation
    int64_t now = esp_timer_get_time();
    float dt = (m_last_imu_ts == 0) ? 0.01f : (now - m_last_imu_ts) / 1000000.0f;
    m_last_imu_ts = now;
    if (dt > 0.5f) dt = 0.01f;

    // EMA low-pass pre-filter on accelerometer (rejects motor vibration)
    m_ax_filt = IMU_ACCEL_LP_ALPHA * ax_raw + (1.0f - IMU_ACCEL_LP_ALPHA) * m_ax_filt;
    m_ay_filt = IMU_ACCEL_LP_ALPHA * ay_raw + (1.0f - IMU_ACCEL_LP_ALPHA) * m_ay_filt;
    m_az_filt = IMU_ACCEL_LP_ALPHA * az_raw + (1.0f - IMU_ACCEL_LP_ALPHA) * m_az_filt;

    // Accelerometer-based angles from filtered values
    float roll_acc  = atan2f(m_ay_filt, m_az_filt) * 180.0f / M_PI;
    float pitch_acc = atan2f(-m_ax_filt,
                             sqrtf(m_ay_filt * m_ay_filt + m_az_filt * m_az_filt)) * 180.0f / M_PI;

    // Complementary filter: blend gyro integration with accel reference
    m_imu_roll  = IMU_CF_ALPHA * (m_imu_roll  + gx_raw * dt) + (1.0f - IMU_CF_ALPHA) * roll_acc;
    m_imu_pitch = IMU_CF_ALPHA * (m_imu_pitch + gy_raw * dt) + (1.0f - IMU_CF_ALPHA) * pitch_acc;
    m_imu_yaw  += gz_raw * dt;

    out.roll  = m_imu_roll;
    out.pitch = m_imu_pitch;
    out.yaw   = m_imu_yaw;
    out.valid = true;
    return true;
}

// ─── GPS NMEA Parser ──────────────────────────────────────────────────────────

bool SensorManager::parseNMEA(const char* sentence, GpsData& out) {
    // Parse $GPRMC: $GPRMC,HHMMSS,A/V,lat,N/S,lon,E/W,speed,course,date,...
    if (strncmp(sentence, "$GPRMC", 6) != 0 &&
        strncmp(sentence, "$GNRMC", 6) != 0) return false;

    char buf[128];
    strncpy(buf, sentence, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* fields[12];
    int   field_count = 0;
    char* tok = strtok(buf, ",");
    while (tok && field_count < 12) {
        fields[field_count++] = tok;
        tok = strtok(nullptr, ",");
    }

    if (field_count < 7) return false;

    // fields[2] = A(active) / V(void)
    if (fields[2][0] != 'A') {
        out.fix = false;
        return true;  // parsed but no fix
    }

    // Latitude: DDMM.MMMM
    double lat_raw  = atof(fields[3]);
    int    lat_deg  = (int)(lat_raw / 100);
    double lat_min  = lat_raw - lat_deg * 100;
    out.latitude    = lat_deg + lat_min / 60.0;
    if (fields[4][0] == 'S') out.latitude *= -1;

    // Longitude: DDDMM.MMMM
    double lon_raw  = atof(fields[5]);
    int    lon_deg  = (int)(lon_raw / 100);
    double lon_min  = lon_raw - lon_deg * 100;
    out.longitude   = lon_deg + lon_min / 60.0;
    if (fields[6][0] == 'W') out.longitude *= -1;

    out.fix        = true;
    out.satellites = 4;   // RMC doesn't carry satellite count

    // Moving median filter on coordinates to reject single-sample spikes
    out.latitude  = applyMedianFilter(out.latitude,  m_gps_lat_buf);
    out.longitude = applyMedianFilter(out.longitude, m_gps_lon_buf);
    return true;
}

double SensorManager::applyMedianFilter(double val, double* buffer) {
    buffer[m_gps_hist_idx % GPS_MEDIAN_WINDOW] = val;
    m_gps_hist_idx++;

    double sorted[GPS_MEDIAN_WINDOW];
    int count = (m_gps_hist_idx < GPS_MEDIAN_WINDOW) ? m_gps_hist_idx : GPS_MEDIAN_WINDOW;
    for (int i = 0; i < count; i++) sorted[i] = buffer[i];

    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (sorted[i] > sorted[j]) {
                double t = sorted[i]; sorted[i] = sorted[j]; sorted[j] = t;
            }
        }
    }
    return sorted[count / 2];
}

// ─── HC-SR04 Distance ─────────────────────────────────────────────────────────

float SensorManager::measureDistanceCm(gpio_num_t trig, gpio_num_t echo) {
    // Send 10µs trigger pulse
    gpio_set_level(trig, 0);
    ets_delay_us(2);
    gpio_set_level(trig, 1);
    ets_delay_us(10);
    gpio_set_level(trig, 0);

    // Wait for echo high (timeout 25ms = ~430cm max)
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(echo) == 0) {
        if (esp_timer_get_time() - start > 25000) return -1.0f;
    }
    int64_t echo_start = esp_timer_get_time();

    // Wait for echo low
    while (gpio_get_level(echo) == 1) {
        if (esp_timer_get_time() - echo_start > 25000) return -1.0f;
    }
    int64_t echo_end = esp_timer_get_time();

    // Distance = time_µs * speed_of_sound / 2
    float duration_us = (float)(echo_end - echo_start);
    return duration_us * 0.0343f / 2.0f;   // 0.0343 cm/µs
}

// ─── Tasks ────────────────────────────────────────────────────────────────────

void SensorManager::imuTask(void* arg) {
    auto* self = static_cast<SensorManager*>(arg);
    ESP_LOGI(Tag, "IMU task running (interval=%ums)", self->m_imu_interval_ms);

    while (self->m_running) {
        ImuData imu;
        if (self->readMPU6050(imu)) {
            self->m_imu = imu;
            if (self->m_cb_imu) self->m_cb_imu(imu);
            MqttManager::getInstance().publishIMU(imu);
        } else {
            ESP_LOGW(Tag, "IMU read failed");
            DebugManager::getInstance().reportError(ErrorCode::IMU_FAIL, "imuTask");
        }
        vTaskDelay(pdMS_TO_TICKS(self->m_imu_interval_ms));
    }
    vTaskDelete(nullptr);
}

void SensorManager::gpsTask(void* arg) {
    auto* self = static_cast<SensorManager*>(arg);
    ESP_LOGI(Tag, "GPS task running (interval=%ums)", self->m_gps_interval_ms);

    char  line[128];
    int   pos = 0;

    while (self->m_running) {
        uint8_t byte;
        int r = uart_read_bytes(self->m_gps_uart, &byte, 1, pdMS_TO_TICKS(200));
        if (r != 1) continue;

        if (byte == '\n' || byte == '\r') {
            if (pos > 0) {
                line[pos] = '\0';
                GpsData gps;
                if (self->parseNMEA(line, gps)) {
                    self->m_gps = gps;
                    if (self->m_cb_gps) self->m_cb_gps(gps);
                    MqttManager::getInstance().publishGPS(gps);
                    if (!gps.fix) {
                        DebugManager::getInstance().reportError(
                            ErrorCode::GPS_NO_FIX, "gpsTask");
                    }
                }
                pos = 0;
            }
        } else if (pos < (int)sizeof(line) - 1) {
            line[pos++] = (char)byte;
        }
    }
    vTaskDelete(nullptr);
}

void SensorManager::ultrasonicTask(void* arg) {
    auto* self = static_cast<SensorManager*>(arg);
    ESP_LOGI(Tag, "Ultrasonic task running (interval=%ums)", self->m_us_interval_ms);

    const char* labels[4] = {"front", "back", "left", "right"};

    while (self->m_running) {
        UltrasonicData us;
        float* vals[4] = { &us.front_cm, &us.back_cm, &us.left_cm, &us.right_cm };

        for (int i = 0; i < 4; i++) {
            *vals[i] = self->measureDistanceCm(self->m_trig[i], self->m_echo[i]);
            vTaskDelay(pdMS_TO_TICKS(10));  // small gap between sensors
        }

        ESP_LOGI(Tag, "US distances: front=%.1fcm back=%.1fcm left=%.1fcm right=%.1fcm",
                 us.front_cm, us.back_cm, us.left_cm, us.right_cm);
        if (us.front_cm < 0 || us.back_cm < 0 || us.left_cm < 0 || us.right_cm < 0) {
            ESP_LOGW(Tag, "One or more ultrasonic sensors returned no echo; check wiring and pin mapping");
        }

        self->m_us = us;
        if (self->m_cb_us) self->m_cb_us(us);
        MqttManager::getInstance().publishUltrasonic(us);

        vTaskDelay(pdMS_TO_TICKS(self->m_us_interval_ms));
    }
    vTaskDelete(nullptr);
}

// ─── start / stop ─────────────────────────────────────────────────────────────

void SensorManager::start(uint32_t imu_ms, uint32_t gps_ms, uint32_t us_ms) {
    ESP_LOGI(Tag, "[ENTRY] start()");

    m_imu_interval_ms = imu_ms;
    m_gps_interval_ms = gps_ms;
    m_us_interval_ms  = us_ms;
    m_running = true;

    xTaskCreate(imuTask,         "imu_task",  3072, this, tskIDLE_PRIORITY + 5, &m_imu_task);
    xTaskCreate(gpsTask,         "gps_task",  3072, this, tskIDLE_PRIORITY + 4, &m_gps_task);
    xTaskCreate(ultrasonicTask,  "us_task",   2048, this, tskIDLE_PRIORITY + 4, &m_us_task);

    ESP_LOGI(Tag, "Sensor tasks started — IMU:%ums GPS:%ums US:%ums",
             imu_ms, gps_ms, us_ms);
    ESP_LOGI(Tag, "[EXIT]  start()");
}

void SensorManager::stop() {
    ESP_LOGI(Tag, "Stopping sensor tasks");
    m_running = false;
    vTaskDelay(pdMS_TO_TICKS(500));  // let tasks exit
}

} // namespace SARCUS
