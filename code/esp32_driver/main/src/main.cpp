#include "main.h"
const char *APP_TAG = "MAIN_APP";

extern "C" void app_main(void) {
    ESP_LOGI(APP_TAG, "System start");
    

    
    ESP_LOGI(APP_TAG, "System ready");
    
    while(true) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}