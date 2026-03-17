#include "main.h"
#include "bts7960.h"

extern "C" void app_main(void) {
    BTS7960 motor1(GPIO_NUM_12, GPIO_NUM_13, GPIO_NUM_14, GPIO_NUM_15, LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_TIMER_0);
    BTS7960 motor2(GPIO_NUM_16, GPIO_NUM_17, GPIO_NUM_18, GPIO_NUM_19, LEDC_CHANNEL_2, LEDC_CHANNEL_3, LEDC_TIMER_1);

    motor1.enable();
    motor2.enable();

    motor1.forward(40.0f);
    motor2.forward(40.0f);

    while(true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS); 
    }
}