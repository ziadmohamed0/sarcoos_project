#ifndef WIFI_AP_H_
#define WIFI_AP_H_

#include "wifi_mode.h"

extern const char* WiFi_AP_TAG;
constexpr wifi_auth_mode_t WIFI_AUTHMODE = WIFI_AUTH_WPA2_PSK;

class WiFi_AP : public IWiFiMode {
    public:
        WiFi_AP(const WiFi_cfg_t &copy_cfg, uint8_t ap_max_connection);
        void init() override;
        void wait_for_connect() override;   
    private:
        WiFi_cfg_t cfg;
        uint8_t AP_MAX_CONNECTION;
        
        static void wifi_event_handler(void *arg,
                                    esp_event_base_t event_base,
                                    int32_t event_id,
                                    void* event_data);        
};

#endif