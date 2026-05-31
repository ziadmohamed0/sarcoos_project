#pragma once

// =============================================================================
//  SARCUS Robot — Web Provisioning Server
//  Runs in AP mode. Serves captive portal, accepts WiFi credentials.
//  DNS server included for captive portal redirect.
//  Same socket-based approach as original Takamul project.
// =============================================================================

#include "esp_http_server.h"
#include <string>

namespace SARCUS {

class WebServer {
public:
    static WebServer& getInstance();

    // Start the socket HTTP server + DNS captive portal
    void start();

    // Stop all server tasks
    void stop();

    // Check if pending WiFi credentials need to be applied (called by main task)
    // Returns true if credentials are ready, and sets ssid/password by reference
    bool getPendingWifiCredentials(std::string& out_ssid, std::string& out_password);
    static void setPendingWifiCredentials(const std::string& ssid, const std::string& password);

private:
    WebServer()  = default;
    ~WebServer() = default;
    WebServer(const WebServer&) = delete;
    WebServer& operator=(const WebServer&) = delete;

    void init();
    httpd_handle_t m_server = nullptr;

    // Pending WiFi credentials from HTTP POST /wifi
    static std::string           s_pending_ssid;
    static std::string           s_pending_password;
    static volatile bool         s_wifi_credentials_ready;

    static esp_err_t handleRoot       (httpd_req_t* req);
    static esp_err_t handleWifiConnect(httpd_req_t* req);
    static esp_err_t handleStatus     (httpd_req_t* req);
};

} // namespace SARCUS
