#pragma once

#include <string>

namespace SARCUS {

class WebServer {
public:
    static WebServer& getInstance();
    void start();
    void stop();

    bool getPendingWifiCredentials(std::string& out_ssid,
                                    std::string& out_password,
                                    std::string& out_broker);
    static void setPendingWifiCredentials(const std::string& ssid,
                                           const std::string& password,
                                           const std::string& broker);

    static void setStaConnectionInfo(const std::string& ip);
    static bool isStaConnected();
    static std::string getStaIP();

private:
    WebServer()  = default;
    ~WebServer() = default;
    WebServer(const WebServer&) = delete;
    WebServer& operator=(const WebServer&) = delete;

    void init();

    static std::string           s_pending_ssid;
    static std::string           s_pending_password;
    static std::string           s_pending_broker;
    static volatile bool         s_wifi_credentials_ready;

    static std::string           s_sta_ip;
    static volatile bool         s_sta_connected;
};

} // namespace SARCUS
