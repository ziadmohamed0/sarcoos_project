#ifndef COMMON_H_
#define COMMON_H_

#include <iostream>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs.h"
#include "driver/uart.h"
#include "driver/uart_select.h"
#include "driver/uart_vfs.h"
#include "driver/uart_wakeup.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "driver/adc.h"
#include "driver/i2c.h"
#include "esp_timer.h"
#include "esp_netif.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_continuous.h"
#include "driver/ledc.h"



enum class state { 
    Low, High 
};

#endif