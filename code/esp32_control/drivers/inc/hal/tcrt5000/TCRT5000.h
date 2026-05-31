#ifndef TCRT5000_H_
#define TCRT5000_H_

#include "common.h"

class tcrt_sensor {
    public:
        tcrt_sensor(gpio_num_t d_pin, adc_channel_t a_pin,
                    adc_unit_t adc_unit = ADC_UNIT_1,
                    adc_atten_t atten = ADC_ATTEN_DB_11);
        bool readDigital();
        float readAnaloge();
    private:
        gpio_num_t out_pin;
        adc_channel_t adc_channel;
        adc_unit_t unit;
        adc_atten_t attenuation;
        adc_oneshot_unit_handle_t adc_handle;
};

#endif