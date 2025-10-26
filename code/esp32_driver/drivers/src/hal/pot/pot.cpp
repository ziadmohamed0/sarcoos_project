#include "pot.h"

const char *POT_TAG = "POTENTIOMETER_DRIVER";

Potentiometer::Potentiometer(adc1_channel_t adc_channel, float max_voltage) 
    : channel(adc_channel), max_voltage(max_voltage) {
    
    // Configure ADC width (12-bit resolution)
    adc1_config_width(ADC_WIDTH_BIT_12);
    
    // Configure channel attenuation (0-3.3V range)
    adc1_config_channel_atten(this->channel, ADC_ATTEN_DB_11);
    
    ESP_LOGI(POT_TAG, "Potentiometer configured on ADC1 channel %d", this->channel);
}

uint16_t Potentiometer::readRaw() {
    int raw_value = adc1_get_raw(this->channel);
    
    if (raw_value < 0) {
        ESP_LOGE(POT_TAG, "Failed to read ADC value");
        return 0;
    }
    
    return static_cast<uint16_t>(raw_value);
}

float Potentiometer::readVoltage() {
    uint16_t raw = this->readRaw();
    float voltage = (raw * this->max_voltage) / static_cast<float>(ADC_MAX_VALUE);
    
    return voltage;
}

float Potentiometer::readPercentage() {
    uint16_t raw = this->readRaw();
    float percentage = (raw * 100.0f) / static_cast<float>(ADC_MAX_VALUE);
    
    return percentage;
}

float Potentiometer::readMapped(float min_value, float max_value) {
    float percentage = this->readPercentage();
    float mapped = min_value + (percentage / 100.0f) * (max_value - min_value);
    
    return mapped;
}

float Potentiometer::readAveraged(uint8_t samples) {
    if (samples == 0) {
        ESP_LOGW(POT_TAG, "Invalid sample count, using default (10)");
        samples = 10;
    }
    
    float sum = 0.0f;
    
    for (uint8_t i = 0; i < samples; i++) {
        sum += this->readPercentage();
        vTaskDelay(pdMS_TO_TICKS(2));  // Small delay between samples
    }
    
    float average = sum / samples;
    
    return average;
}