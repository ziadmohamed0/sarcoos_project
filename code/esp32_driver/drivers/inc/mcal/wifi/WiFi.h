#ifndef WIFI_H_
#define WIFI_H_

#include "wifi_ap.h"
#include "wifi_sta.h"

class WiFi{
    public:
        WiFi(const WiFi_cfg_t &cfg);
        void init();
        void wait_for_connect();
        void switch_to_sta(const std::string &ssid, const std::string &password);
    private:
        WiFi_AP ap;
        WiFi_STA sta;
        WIFI_AP_STA mode;
};


#endif