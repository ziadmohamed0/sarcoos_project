#include "TCRT5000.h"

tcrt_sensor::tcrt_sensor(gpio_num_t d_pin, adc_channel_t a_pin,
                         adc_unit_t adc_unit,
                         adc_atten_t atten)
    : out_pin(d_pin),
      adc_channel(a_pin),
      unit(adc_unit),
      attenuation(atten),
      adc_handle(nullptr) {
    gpio_set_direction(this->out_pin, GPIO_MODE_INPUT);

    adc_oneshot_unit_init_cfg_t init_config = {};
    init_config.unit_id = this->unit;
    init_config.ulp_mode = ADC_ULP_MODE_DISABLE;

    esp_err_t ret = adc_oneshot_new_unit(&init_config, &this->adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TCRT_TAG, "Failed to initialize ADC unit: %s", esp_err_to_name(ret));
        return;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {};
    chan_cfg.atten = this->attenuation;
    chan_cfg.bitwidth = ADC_BITWIDTH_12;

    ret = adc_oneshot_config_channel(this->adc_handle, this->adc_channel, &chan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TCRT_TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
    }
}

bool tcrt_sensor::readDigital() {
    return gpio_get_level(this->out_pin);
}

float tcrt_sensor::readAnaloge() {
    return adc1_get_raw(this->adc_channel);
}