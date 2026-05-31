#include "MPU6050.h"

MPU6050::MPU6050(i2c_port_t port_i2c, gpio_num_t sda_, gpio_num_t scl_,
                float alpha_cf, float alpha_lp) :
                port(port_i2c), sda(sda_), scl(scl_),
                alpha(alpha_cf), m_accel_lp_alpha(alpha_lp) {
    this->init();
}

esp_err_t MPU6050::init() {
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = this->sda,
        .scl_io_num = this->scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {.clk_speed = 400000},
    };
    i2c_param_config(this->port, &cfg);
    i2c_driver_install(this->port, I2C_MODE_MASTER, 0, 0, 0);
    this->writeByte(0x6B, 0x00);
    this->writeByte(0x1B, 0x00);
    this->writeByte(0x1C, 0x00);
    return ESP_OK;
}

esp_err_t MPU6050::writeByte(uint8_t reg, uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(this->port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t MPU6050::readBytes(uint8_t reg, uint8_t* buffer, size_t length) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, buffer, length, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(this->port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t MPU6050::readAccel(float& ax, float& ay, float& az) {
    uint8_t data[6];
    if (readBytes(0x3B, data, 6) != ESP_OK)
        return ESP_FAIL;

    int16_t raw_ax = (data[0] << 8) | data[1];
    int16_t raw_ay = (data[2] << 8) | data[3];
    int16_t raw_az = (data[4] << 8) | data[5];

    ax = raw_ax / 16384.0f;
    ay = raw_ay / 16384.0f;
    az = raw_az / 16384.0f;
    return ESP_OK;
}

esp_err_t MPU6050::readGyro(float& gx, float& gy, float& gz) {
    uint8_t data[6];
    if (readBytes(0x43, data, 6) != ESP_OK)
        return ESP_FAIL;

    int16_t raw_gx = (data[0] << 8) | data[1];
    int16_t raw_gy = (data[2] << 8) | data[3];
    int16_t raw_gz = (data[4] << 8) | data[5];

    gx = raw_gx / 131.0f;
    gy = raw_gy / 131.0f;
    gz = raw_gz / 131.0f;
    return ESP_OK;
}

float MPU6050::readTemprature() {
    uint8_t data[2];
    if (readBytes(0x41, data, 2) != ESP_OK)
        return 0;

    int16_t raw_temp = (data[0] << 8) | data[1];
    return (raw_temp / 340.0f) + 36.53f;
}

void MPU6050::updateAngles(float dt) {
    float ax, ay, az, gx, gy, gz;

    readAccel(ax, ay, az);
    readGyro(gx, gy, gz);

    gx -= m_gx_bias;
    gy -= m_gy_bias;
    gz -= m_gz_bias;

    m_ax_filt = m_accel_lp_alpha * ax + (1.0f - m_accel_lp_alpha) * m_ax_filt;
    m_ay_filt = m_accel_lp_alpha * ay + (1.0f - m_accel_lp_alpha) * m_ay_filt;
    m_az_filt = m_accel_lp_alpha * az + (1.0f - m_accel_lp_alpha) * m_az_filt;

    float roll_acc  = atan2(m_ay_filt, m_az_filt) * 180.0f / M_PI;
    float pitch_acc = atan2(-m_ax_filt, sqrt(m_ay_filt * m_ay_filt + m_az_filt * m_az_filt)) * 180.0f / M_PI;

    roll  = alpha * (roll  + gx * dt) + (1.0f - alpha) * roll_acc;
    pitch = alpha * (pitch + gy * dt) + (1.0f - alpha) * pitch_acc;

    yaw += gz * dt;
}

void MPU6050::calibrateGyro(int samples) {
    float sum_gx = 0, sum_gy = 0, sum_gz = 0;
    float gx, gy, gz;

    for (int i = 0; i < samples; i++) {
        if (readGyro(gx, gy, gz) == ESP_OK) {
            sum_gx += gx;
            sum_gy += gy;
            sum_gz += gz;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    m_gx_bias = sum_gx / samples;
    m_gy_bias = sum_gy / samples;
    m_gz_bias = sum_gz / samples;
    m_bias_initialized = true;
}

float MPU6050::getRoll() {
    return this->roll;
}

float MPU6050::getPitch() {
    return this->pitch;
}

float MPU6050::getYaw() {
    return this->yaw;
}
