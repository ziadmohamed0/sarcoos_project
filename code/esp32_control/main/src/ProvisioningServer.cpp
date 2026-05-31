#include "ProvisioningServer.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <strings.h>

static const char* TAG = "Provisioning";

// Static members
volatile bool ProvisioningServer::s_web_stop    = false;
volatile bool ProvisioningServer::s_dns_stop    = false;
volatile bool ProvisioningServer::s_creds_ready = false;
std::string   ProvisioningServer::s_pending_ssid;
std::string   ProvisioningServer::s_pending_pass;
std::string   ProvisioningServer::s_pending_broker;

static EventGroupHandle_t s_ip_ready = nullptr;
static constexpr EventBits_t kIpReadyBit = BIT0;
static esp_netif_t* s_netif_sta = nullptr;

static const char* HTML_PAGE = R"rawliteral(<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta charset="UTF-8">
<style>
  body { font-family: Arial, sans-serif; background: #1a1a2e; color: #eee; margin: 0; padding: 20px; }
  .card { max-width: 420px; margin: 40px auto; background: #16213e; padding: 30px;
          border-radius: 12px; box-shadow: 0 4px 20px rgba(0,0,0,0.4); }
  h2 { text-align: center; color: #0f3460; background: #e94560; padding: 12px;
       border-radius: 8px; margin: -30px -30px 24px -30px; }
  label { display: block; margin-bottom: 4px; font-size: 13px; color: #aaa; }
  input[type=text], input[type=password] {
    width: 100%; padding: 11px; margin-bottom: 16px; border: 1px solid #0f3460;
    border-radius: 6px; background: #0f3460; color: #eee; box-sizing: border-box; font-size: 15px;
  }
  button {
    background: #e94560; color: white; padding: 13px; border: none;
    width: 100%; border-radius: 6px; font-size: 16px; cursor: pointer;
  }
  button:hover { background: #c73652; }
  .logo { text-align: center; font-size: 13px; color: #555; margin-top: 18px; }
  .success { text-align: center; padding: 20px; }
  .success h3 { color: #4caf50; font-size: 24px; margin: 10px 0; }
  .ip-addr { font-size: 28px; font-weight: bold; color: #e94560;
             background: #0f3460; padding: 12px; border-radius: 8px;
             margin: 16px 0; letter-spacing: 2px; }
  .spinner { border: 4px solid #0f3460; border-top: 4px solid #e94560;
             border-radius: 50%; width: 40px; height: 40px;
             animation: spin 1s linear infinite; margin: 20px auto; }
  @keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }
</style>
</head>
<body>
<div class="card" id="setupPage">
  <h2>&#129302; SARCUS Setup</h2>
  <form id="configForm" onsubmit="return submitForm(event)">
    <label>WiFi Network (SSID)</label>
    <input type="text" id="ssid" name="ssid" placeholder="Enter WiFi name" required>
    <label>Password</label>
    <input type="password" id="password" name="password" placeholder="Enter password">
    <label>MQTT Broker URI</label>
    <input type="text" id="broker" name="broker" value="mqtt://192.168.1.5:1883" placeholder="mqtt://IP:PORT">
    <button type="submit">Connect Robot</button>
  </form>
  <div class="logo">SARCUS Robot v1.0 &mdash; Assistive Exoskeleton</div>
</div>
<div class="card" id="waitPage" style="display:none">
  <h2>&#129302; Connecting...</h2>
  <div class="success">
    <div class="spinner"></div>
    <p>Robot is connecting to your WiFi network...</p>
    <p style="font-size:12px;color:#777">This may take up to 30 seconds</p>
  </div>
</div>
<div class="card" id="successPage" style="display:none">
  <h2>&#129302; Connected!</h2>
  <div class="success">
    <h3>&#10003;</h3>
    <p>Robot is online at:</p>
    <div class="ip-addr" id="robotIp">0.0.0.0</div>
    <p style="font-size:13px;color:#aaa">Use this IP from Node-RED or your dashboard</p>
  </div>
</div>
<script>
function submitForm(e) {
  e.preventDefault();
  document.getElementById('setupPage').style.display = 'none';
  document.getElementById('waitPage').style.display = 'block';
  var data = new URLSearchParams(new FormData(document.getElementById('configForm'))).toString();
  fetch('/wifi', { method: 'POST', body: data })
    .then(function() { checkStatus(); })
    .catch(function() { checkStatus(); });
  return false;
}
function checkStatus() {
  fetch('/status')
    .then(function(r) { return r.json(); })
    .then(function(j) {
      if (j.status === 'connected' && j.ip) {
        document.getElementById('waitPage').style.display = 'none';
        document.getElementById('successPage').style.display = 'block';
        document.getElementById('robotIp').textContent = j.ip;
      } else {
        setTimeout(checkStatus, 2000);
      }
    })
    .catch(function() { setTimeout(checkStatus, 2000); });
}
</script>
</body>
</html>)rawliteral";

static void url_decode_inplace(char* src) {
    char* dst = src;
    while (*src) {
        if (*src == '+') { *dst++ = ' '; src++; }
        else if (*src == '%' && isxdigit((unsigned char)src[1])
                             && isxdigit((unsigned char)src[2])) {
            char hex[3] = { src[1], src[2], '\0' };
            *dst++ = (char)strtol(hex, nullptr, 16);
            src += 3;
        } else { *dst++ = *src++; }
    }
    *dst = '\0';
}

static void trim_inplace(char* str) {
    char* start = str;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != str) memmove(str, start, strlen(start) + 1);
    if (*str == '\0') return;
    char* end = str + strlen(str) - 1;
    while (end >= str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
}

static void wifi_event_handler(void* arg, esp_event_base_t base,
                                int32_t id, void* data) {
    auto* self = static_cast<ProvisioningServer*>(arg);
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        auto* info = static_cast<wifi_event_sta_disconnected_t*>(data);
        ESP_LOGW(TAG, "STA disconnected (reason=%u)", info->reason);
        if (self) self->waitForSTA(0);
    }
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto* evt = static_cast<ip_event_got_ip_t*>(data);
        ESP_LOGI(TAG, "STA got IP: " IPSTR, IP2STR(&evt->ip_info.ip));
        self->m_sta_connected = true;
        char buf[16];
        snprintf(buf, sizeof(buf), IPSTR, IP2STR(&evt->ip_info.ip));
        self->m_sta_ip = std::string(buf);
        if (s_ip_ready) xEventGroupSetBits(s_ip_ready, kIpReadyBit);
    }
}

static void dns_task(void*) {
    ESP_LOGI(TAG, "DNS task on port 53");
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) { vTaskDelete(nullptr); return; }
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_ANY); addr.sin_port = htons(53);
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    struct timeval tv = { 1, 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    uint8_t buf[512];
    while (!ProvisioningServer::s_dns_stop) {
        struct sockaddr_in client = {}; socklen_t clen = sizeof(client);
        int len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&client, &clen);
        if (len >= 12) {
            buf[2]=0x81; buf[3]=0x80; buf[6]=0; buf[7]=1; buf[8]=0; buf[9]=0; buf[10]=0; buf[11]=0;
            uint8_t ans[] = {0xC0,0x0C,0,1,0,1,0,0,0,0x3C,0,4,192,168,4,1};
            if (len + (int)sizeof(ans) <= (int)sizeof(buf)) {
                memcpy(buf+len, ans, sizeof(ans));
                sendto(sock, buf, len+sizeof(ans), 0, (struct sockaddr*)&client, clen);
            }
        }
    }
    close(sock); vTaskDelete(nullptr);
}

static void send_all(int sock, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) { int r = send(sock, data+sent, (int)(len-sent), 0); if (r<0) break; sent += r; }
}

static void handle_client(int client_sock) {
    const int BUF_SZ = 4096;
    char* buf = (char*)malloc(BUF_SZ+1);
    if (!buf) { close(client_sock); return; }
    int r = recv(client_sock, buf, BUF_SZ, 0);
    if (r <= 0) { free(buf); close(client_sock); return; }
    buf[r] = '\0';
    char method[8]={}, uri[256]={};
    if (sscanf(buf, "%7s %255s", method, uri) != 2) { free(buf); close(client_sock); return; }
    char* hdr_end = strstr(buf, "\r\n\r\n");
    char* body = hdr_end ? hdr_end+4 : nullptr;
    int content_len = 0;
    if (hdr_end) { char* cl = strcasestr(buf, "Content-Length:"); if (cl) content_len = atoi(cl+15); }

    if (strcmp(method,"GET")==0 && (strcmp(uri,"/")==0 || strcmp(uri,"/hotspot-detect.html")==0)) {
        char hdr[128];
        snprintf(hdr, sizeof(hdr),
            "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %d\r\n\r\n",
            (int)strlen(HTML_PAGE));
        send_all(client_sock, hdr, strlen(hdr));
        send_all(client_sock, HTML_PAGE, strlen(HTML_PAGE));
    }
    else if (strcmp(method,"GET")==0 && strcmp(uri,"/status")==0) {
        char resp[256];
        bool connected = false;
        std::string ip = "0.0.0.0";
        for (int i=0; i<60; i++) {
            if (ProvisioningServer::s_creds_ready) break;
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        // After form submitted, wait for STA IP (up to 30s)
        extern bool g_prov_sta_connected;
        extern std::string g_prov_sta_ip;
        for (int i=0; i<60; i++) {
            if (g_prov_sta_connected) {
                connected = true;
                ip = g_prov_sta_ip;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        if (connected) {
            snprintf(resp, sizeof(resp),
                "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"connected\",\"ip\":\"%s\"}", ip.c_str());
        } else {
            snprintf(resp, sizeof(resp),
                "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"connecting\",\"ip\":\"0.0.0.0\"}");
        }
        send_all(client_sock, resp, strlen(resp));
    }
    else if (strcmp(method,"POST")==0 && strcmp(uri,"/wifi")==0) {
        int body_received = body ? (r-(int)(body-buf)) : 0;
        while (body_received < content_len && r < BUF_SZ) {
            int n = recv(client_sock, buf+r, BUF_SZ-r, 0);
            if (n<=0) { break; } r+=n; body = hdr_end+4; body_received = r-(int)(body-buf);
        }
        char ssid[64]={}, password[128]={}, broker[128]={};
        if (body && content_len>0) {
            char* tmp = (char*)malloc(content_len+1);
            if (tmp) {
                memcpy(tmp, body, content_len); tmp[content_len]='\0';
                char* pair = strtok(tmp, "&");
                while (pair) {
                    char* eq = strchr(pair, '=');
                    if (eq) {
                        *eq='\0'; char* k=pair; char* v=eq+1;
                        url_decode_inplace(v);
                        if (strcmp(k,"ssid")==0)     strncpy(ssid, v, sizeof(ssid)-1);
                        if (strcmp(k,"password")==0) strncpy(password, v, sizeof(password)-1);
                        if (strcmp(k,"broker")==0)   strncpy(broker, v, sizeof(broker)-1);
                    }
                    pair = strtok(nullptr, "&");
                }
                free(tmp);
            }
        }
        trim_inplace(ssid); trim_inplace(password); trim_inplace(broker);
        ESP_LOGI(TAG, "Provisioning: SSID='%s' broker='%s'", ssid, broker);

        // Save to NVS
        nvs_handle_t nvs_h;
        if (nvs_open("sarcus", NVS_READWRITE, &nvs_h) == ESP_OK) {
            nvs_set_str(nvs_h, "wifi_ssid", ssid);
            nvs_set_str(nvs_h, "wifi_pass", password);
            nvs_set_str(nvs_h, "mqtt_uri",  broker);
            nvs_commit(nvs_h);
            nvs_close(nvs_h);
            ESP_LOGI(TAG, "Saved to NVS");
        }

        const char* resp =
            "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: 60\r\n\r\n"
            "<html><body><h2>Saved</h2></body></html>";
        send_all(client_sock, resp, strlen(resp));

        if (strlen(ssid) > 0) {
            ProvisioningServer::s_pending_ssid   = ssid;
            ProvisioningServer::s_pending_pass   = password;
            ProvisioningServer::s_pending_broker = broker;
            ProvisioningServer::s_creds_ready    = true;
        }
    }
    else {
        const char* resp =
            "HTTP/1.1 302 Found\r\nLocation: http://192.168.4.1/\r\nContent-Length: 0\r\n\r\n";
        send_all(client_sock, resp, strlen(resp));
    }
    free(buf); close(client_sock);
}

static void web_task(void*) {
    ESP_LOGI(TAG, "Web task on port 80");
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) { vTaskDelete(nullptr); return; }
    int opt=1; setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr={};
    addr.sin_family=AF_INET; addr.sin_addr.s_addr=htonl(INADDR_ANY); addr.sin_port=htons(80);
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(sock, 4);
    struct timeval tv={1,0}; setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    while (!ProvisioningServer::s_web_stop) {
        struct sockaddr_in6 ca={}; socklen_t al=sizeof(ca);
        int cs = accept(sock, (struct sockaddr*)&ca, &al);
        if (cs < 0) continue;
        handle_client(cs);
    }
    close(sock); vTaskDelete(nullptr);
}

// Global state for /status endpoint to poll
bool g_prov_sta_connected = false;
std::string g_prov_sta_ip = "0.0.0.0";

ProvisioningServer::ProvisioningServer()
    : m_sta_connected(false), m_sta_ip("0.0.0.0") {}

ProvisioningServer::~ProvisioningServer() {
    stopAP();
}

void ProvisioningServer::startAP() {
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);

    esp_netif_create_default_wifi_ap();
    s_netif_sta = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, this, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, this, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_LOST_IP, &wifi_event_handler, this, nullptr));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    wifi_config_t ap = {};
    strncpy((char*)ap.ap.ssid, "SARCUS_SETUP", sizeof(ap.ap.ssid));
    strncpy((char*)ap.ap.password, "sarcus2024", sizeof(ap.ap.password));
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap.ap.max_connection = 4;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "AP: SARCUS_SETUP / sarcus2024");
}

void ProvisioningServer::startWiFiSTA(const std::string& ssid, const std::string& password) {
    m_sta_connected = false;
    m_sta_ip = "0.0.0.0";
    g_prov_sta_connected = false;
    g_prov_sta_ip = "0.0.0.0";

    s_ip_ready = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    wifi_config_t sta = {};
    strncpy((char*)sta.sta.ssid, ssid.c_str(), sizeof(sta.sta.ssid)-1);
    strncpy((char*)sta.sta.password, password.c_str(), sizeof(sta.sta.password)-1);
    sta.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_connect();
    ESP_LOGI(TAG, "AP+STA mode: connecting to '%s'", ssid.c_str());
}

bool ProvisioningServer::waitForSTA(uint32_t timeout_ms) {
    if (!s_ip_ready) return false;
    EventBits_t bits = xEventGroupWaitBits(s_ip_ready, kIpReadyBit,
        pdFALSE, pdTRUE, pdMS_TO_TICKS(timeout_ms));
    bool ok = (bits & kIpReadyBit) != 0;
    if (ok) {
        m_sta_connected = true;
        g_prov_sta_connected = true;
        g_prov_sta_ip = m_sta_ip;
    }
    return ok;
}

std::string ProvisioningServer::getSTAIP() {
    return m_sta_ip;
}

void ProvisioningServer::startWebServer() {
    s_web_stop = false;
    s_dns_stop = false;
    xTaskCreate(web_task, "web_task", 8192, nullptr, tskIDLE_PRIORITY+5, nullptr);
}

void ProvisioningServer::startDNSServer() {
    xTaskCreate(dns_task, "dns_task", 4096, nullptr, tskIDLE_PRIORITY+4, nullptr);
}

void ProvisioningServer::stopAP() {
    s_web_stop = true;
    s_dns_stop = true;
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_wifi_stop();
    esp_wifi_deinit();
}

ProvisioningResult ProvisioningServer::run() {
    ProvisioningResult result = {};
    result.success = false;

    startAP();
    startWebServer();
    startDNSServer();

    ESP_LOGI(TAG, "Provisioning started. Connect to SSID: SARCUS_SETUP");
    ESP_LOGI(TAG, "Open http://192.168.4.1 in your browser");

    s_creds_ready = false;
    while (!s_creds_ready) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    ESP_LOGI(TAG, "Credentials received. Switching to AP+STA...");
    startWebServer(); // ensure web server is running

    // Clear global state
    g_prov_sta_connected = false;
    g_prov_sta_ip = "0.0.0.0";

    startWiFiSTA(s_pending_ssid, s_pending_pass);

    if (waitForSTA(30000)) {
        result.success = true;
        result.wifi_ssid  = s_pending_ssid;
        result.wifi_pass  = s_pending_pass;
        result.mqtt_broker = s_pending_broker;
        result.sta_ip     = m_sta_ip;
        ESP_LOGI(TAG, "Provisioning SUCCESS. IP: %s", m_sta_ip.c_str());
        // Keep web server alive so user can see the IP page
    } else {
        ESP_LOGW(TAG, "Provisioning FAILED. STA did not connect.");
        stopAP();
    }

    return result;
}
