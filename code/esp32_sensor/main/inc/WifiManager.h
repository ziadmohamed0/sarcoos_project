#pragma once

// =============================================================================
//  SARCUS Robot — WiFi Manager
//  AP mode (provisioning) → STA mode (connected).
//  Credentials stored in NVS. Singleton pattern.
// =============================================================================

#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string>

namespace SARCUS {

class WifiManager {
public:
    static WifiManager& getInstance();

    // Initialize WiFi stack (call once at boot)
    void init();

    // Start Access Point mode for provisioning
    // SSID: "SARCUS_SETUP", Password: "sarcus2024"
    void startAP();

    // Connect to WiFi in STA mode
    void startSTA(const std::string& ssid, const std::string& password);

    // Block until IP is obtained or timeout. Returns true on success.
    bool waitForIP(TickType_t timeout_ticks);

    // Stop WiFi
    void stop();

    // Check if connected
    bool isConnected() const { return m_connected; }

    static constexpr EventBits_t kIpReadyBit = BIT0;

private:
    WifiManager()  = default;
    ~WifiManager() = default;
    WifiManager(const WifiManager&) = delete;
    WifiManager& operator=(const WifiManager&) = delete;

    static void eventHandler(void* arg, esp_event_base_t base,
                             int32_t id, void* data);

    EventGroupHandle_t m_ip_ready   = nullptr;
    esp_netif_t*       m_netif_ap   = nullptr;
    esp_netif_t*       m_netif_sta  = nullptr;
    bool               m_connected  = false;
    uint8_t            m_retry_cnt  = 0;

    static constexpr uint8_t kMaxRetries = 5;
};

} // namespace SARCUS
