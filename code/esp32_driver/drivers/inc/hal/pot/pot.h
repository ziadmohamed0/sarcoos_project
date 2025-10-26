#ifndef POT_H_
#define POT_H_

#include "common.h"

extern const char *POT_TAG;

class Potentiometer {
public:
    /**
     * @brief Construct a new Potentiometer object
     * 
     * @param adc_channel ADC1 channel for potentiometer reading
     * @param max_voltage Maximum voltage reference (default 3.3V)
     */
    Potentiometer(adc1_channel_t adc_channel, float max_voltage = 3.3f);
    
    /**
     * @brief Read raw ADC value
     * 
     * @return uint16_t Raw ADC value (0-4095 for 12-bit)
     */
    uint16_t readRaw();
    
    /**
     * @brief Read voltage value
     * 
     * @return float Voltage in volts (0.0 - max_voltage)
     */
    float readVoltage();
    
    /**
     * @brief Read percentage value
     * 
     * @return float Percentage (0.0 - 100.0)
     */
    float readPercentage();
    
    /**
     * @brief Read mapped value to custom range
     * 
     * @param min_value Minimum output value
     * @param max_value Maximum output value
     * @return float Mapped value
     */
    float readMapped(float min_value, float max_value);
    
    /**
     * @brief Read averaged value over multiple samples
     * 
     * @param samples Number of samples to average (default 10)
     * @return float Averaged percentage value
     */
    float readAveraged(uint8_t samples = 10);
    
    ~Potentiometer() = default;

private:
    adc1_channel_t channel;
    float max_voltage;
    const uint16_t ADC_MAX_VALUE = 4095;  // 12-bit ADC
};

#endif