#ifndef WIFI_MODE_H_
#define WIFI_MODE_H_

#include "common.h"

constexpr uint8_t WIFI_CONNECT_BIT = BIT0;
constexpr uint8_t WIFI_FAIL_BIT = BIT1;

enum class WIFI_AP_STA : uint8_t {
    station, access_point, dual_mode
};

struct WiFi_cfg_t {
    std::string ssid;
    std::string password;
    WIFI_AP_STA state;
    uint8_t MaxRetry;
};

class IWiFiMode {
    public:
        virtual void init() = 0 ;
        virtual void wait_for_connect() = 0;
        virtual ~IWiFiMode() = default;
};

#endif