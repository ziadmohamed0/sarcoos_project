#ifndef PROVISIONING_SERVER_H_
#define PROVISIONING_SERVER_H_

#include <string>

struct ProvisioningResult {
    bool     success;
    std::string wifi_ssid;
    std::string wifi_pass;
    std::string mqtt_broker;
    std::string sta_ip;       // IP address obtained after connection
};

class ProvisioningServer {
public:
    ProvisioningServer();
    ~ProvisioningServer();

    // Start AP mode + captive portal, serve form, wait for credentials,
    // connect to WiFi, return result with IP.
    // Blocks until provisioning completes or fails.
    ProvisioningResult run();

    // Start AP mode (non-blocking, tasks created)
    void startAP();

    // Stop AP mode and web server
    void stopAP();

    // Check if STA has connected and got IP
    bool waitForSTA(uint32_t timeout_ms);

    // Get current STA IP
    std::string getSTAIP();

    // Shared state (accessed by helper free functions in ProvisioningServer.cpp)
    bool m_sta_connected;
    std::string m_sta_ip;
    static volatile bool s_web_stop;
    static volatile bool s_dns_stop;
    static volatile bool s_creds_ready;
    static std::string   s_pending_ssid;
    static std::string   s_pending_pass;
    static std::string   s_pending_broker;

private:
    void startWiFiAP();
    void startWiFiSTA(const std::string& ssid, const std::string& password);
    void startWebServer();
    void startDNSServer();
};

#endif
