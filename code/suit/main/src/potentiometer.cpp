#include "../inc/potentiometer.h"

potentiometer::potentiometer(adc1_channel_t copyChannel) : 
                            pot_channel(copyChannel) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(this->pot_channel, ADC_ATTEN_DB_11);
}

int potentiometer::Raw() {
    return adc1_get_raw(this->pot_channel);
}

float potentiometer::getAngle() {
    int raw = Raw();
    return (raw / 4095.0f) * 3.3f; // Assuming 12-bit ADC and 3.3V ref
}

float potentiometer::filter(float current) {
    const float alpha = 0.1f;
    prev_angle = (alpha * current) + ((1.0f - alpha) * prev_angle);
    return prev_angle;
}