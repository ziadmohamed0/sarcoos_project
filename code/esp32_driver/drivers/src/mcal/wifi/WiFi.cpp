#include "WiFi.h"

WiFi::WiFi(const WiFi_cfg_t &cfg) 
    : ap(cfg, 4), sta(cfg), mode(cfg.state) {
    this->init();
}


void WiFi::init() {
    if(this->mode == WIFI_AP_STA::access_point) {
        this->ap.init();
    } 
    
    else if(this->mode == WIFI_AP_STA::station) {
        this->sta.init();
    } 
    
    else { // dual mode
        this->ap.init();
        this->sta.init();
    }
}


void WiFi::wait_for_connect() {
    if (this->mode == WIFI_AP_STA::access_point) {
        this->ap.wait_for_connect();
    } 
    
    else if (this->mode == WIFI_AP_STA::station) {
        this->sta.wait_for_connect();
    } 
    
    else { // dual mode
        this->ap.wait_for_connect();
        this->sta.wait_for_connect();
    }
}


void WiFi::switch_to_sta(const std::string &ssid, const std::string &password) {
    WiFi_cfg_t cfg;
    cfg.ssid = ssid;
    cfg.password = password;
    cfg.state = WIFI_AP_STA::station;
    cfg.MaxRetry = 5;
    
    this->sta = WiFi_STA(cfg);
    this->mode = WIFI_AP_STA::station;
    this->sta.init();
}