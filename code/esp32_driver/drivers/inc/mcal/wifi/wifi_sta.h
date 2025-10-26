#ifndef WIFI_STATION_H_
#define WIFI_STATION_H_

#include "wifi_mode.h"

extern const char* WiFi_STA_TAG;

class WiFi_STA : public IWiFiMode {
    public:
        WiFi_STA(const WiFi_cfg_t &copy_cfg);
        void init() override;
        void wait_for_connect() override;    
    private:
        WiFi_cfg_t cfg;
        uint8_t s_retry_counter{0};
        EventGroupHandle_t s_wifi_event_group;

        static void wifi_event_handler(void *arg,
                                    esp_event_base_t event_base,
                                    int32_t event_id,
                                    void* event_data);
};

#endif