#ifndef MPU6050_H_
#define MPU6050_H_

#include "common.h"
#include <cmath>

class MPU6050 {
    public:
        MPU6050(i2c_port_t port_i2c, gpio_num_t sda_, gpio_num_t scl_,
                float alpha_cf = 0.98f, float alpha_lp = 0.15f);
        esp_err_t readAccel(float& ax, float& ay, float& az);
        esp_err_t readGyro(float& gx, float& gy, float& gz);
        float readTemprature();

        void updateAngles(float dt);
        float getRoll();
        float getPitch();
        float getYaw();

        void setAlpha(float alpha_cf) { alpha = alpha_cf; }
        void setAccelLpAlpha(float alpha_lp) { m_accel_lp_alpha = alpha_lp; }
        void calibrateGyro(int samples = 200);

    private:
        esp_err_t writeByte(uint8_t reg, uint8_t data);
        esp_err_t readBytes(uint8_t reg, uint8_t* buffer, size_t length);
        esp_err_t init();

        i2c_port_t port;
        gpio_num_t sda;
        gpio_num_t scl;

        float roll = 0;
        float pitch = 0;
        float yaw = 0;
        float alpha;

        float m_accel_lp_alpha;
        float m_ax_filt = 0, m_ay_filt = 0, m_az_filt = 0;

        float m_gx_bias = 0, m_gy_bias = 0, m_gz_bias = 0;
        bool m_bias_initialized = false;

        static constexpr uint8_t MPU6050_ADDR = 0x68;
};

#endif
