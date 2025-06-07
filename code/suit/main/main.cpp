#include <stdio.h>
#include "inc/potentiometer.h"
#include "inc/mqtt_client_custom.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

potentiometer pot1(ADC1_CHANNEL_0);
potentiometer pot2(ADC1_CHANNEL_3);
potentiometer pot3(ADC1_CHANNEL_6);
potentiometer pot4(ADC1_CHANNEL_7);
// potentiometer pot5(ADC1_CHANNEL_4);
// potentiometer pot6(ADC1_CHANNEL_5);

const std::string ssid = "Mohamed Fathy";
const std::string password = "341978341978";
const std::string broker_uri = "mqtt://192.168.100.25:1883";

const std::string topic1 = "sensor/potentiometer_1";
const std::string topic2 = "sensor/potentiometer_2";
const std::string topic3 = "sensor/potentiometer_3";
const std::string topic4 = "sensor/potentiometer_4";
// const std::string topic5 = "sensor/potentiometer_5";
// const std::string topic6 = "sensor/potentiometer_6";
const std::string topic7 = "button/forward";
const std::string topic8 = "button/reverse";
const std::string topic9 = "button/right";
const std::string topic10 = "button/left";

mqtt_client_custom mqttClient(ssid, password, broker_uri);

extern "C" void app_main() {
    gpio_config_t io_cfg1 = {
        .pin_bit_mask = (1ULL << GPIO_NUM_25),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_cfg1);

    gpio_config_t io_cfg2 = {
        .pin_bit_mask = (1ULL << GPIO_NUM_26),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_cfg2);

    gpio_config_t io_cfg3 = {
        .pin_bit_mask = (1ULL << GPIO_NUM_27),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_cfg3);
    
    gpio_config_t io_cfg4 = {
        .pin_bit_mask = (1ULL << GPIO_NUM_14),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_cfg4);    

    mqttClient.begin();
    while (true) {
        float angle1 = pot1.getAngle();
        // vTaskDelay(pdMS_TO_TICKS(2));
        float angle2 = pot2.getAngle();
        // vTaskDelay(pdMS_TO_TICKS(2));
        float angle3 = pot3.getAngle();
        // vTaskDelay(pdMS_TO_TICKS(2));
        float angle4 = pot4.getAngle();
        // vTaskDelay(pdMS_TO_TICKS(2));
        // float angle5 = pot5.getAngle();
        // vTaskDelay(pdMS_TO_TICKS(2));
        // float angle6 = pot6.getAngle();
        // vTaskDelay(pdMS_TO_TICKS(2));

        int forward_btn = gpio_get_level(GPIO_NUM_25);
        int reverse_btn = gpio_get_level(GPIO_NUM_26);
        int right_btn = gpio_get_level(GPIO_NUM_27);
        int left_btn = gpio_get_level(GPIO_NUM_14);

        mqttClient.publish(topic1, std::to_string(angle1));
        mqttClient.publish(topic2, std::to_string(angle2));
        mqttClient.publish(topic3, std::to_string(angle3));
        mqttClient.publish(topic4, std::to_string(angle4));
        // mqttClient.publish(topic5, std::to_string(angle5));
        // mqttClient.publish(topic6, std::to_string(angle6));

        mqttClient.publish(topic7, std::to_string(forward_btn));
        mqttClient.publish(topic8, std::to_string(reverse_btn));
        mqttClient.publish(topic9, std::to_string(right_btn));
        mqttClient.publish(topic10, std::to_string(left_btn));

        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
