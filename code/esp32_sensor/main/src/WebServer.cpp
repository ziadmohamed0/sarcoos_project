#include "inc/WebServer.h"
#include "inc/WifiManager.h"
#include "inc/NVSManager.h"
#include "esp_log.h"
#include "esp_netif.h"
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <strings.h>

namespace SARCUS {

static const char* Tag = "WebServer";

// ─── Static members ────────────────────────────────────────────────────────────

std::string           WebServer::s_pending_ssid;
std::string           WebServer::s_pending_password;
std::string           WebServer::s_pending_broker;
volatile bool         WebServer::s_wifi_credentials_ready = false;
std::string           WebServer::s_sta_ip;
volatile bool         WebServer::s_sta_connected = false;

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
       border-radius: 8px; margin: -30px -30px 24px -30px; border-radius: 12px 12px 0 0; }
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
    <p style="font-size:13px;color:#aaa">Use this IP to connect from Node-RED or your control dashboard</p>
    <p style="font-size:12px;color:#777">The robot will continue to run. Close this page to finish setup.</p>
  </div>
</div>

<script>
function submitForm(e) {
  e.preventDefault();
  document.getElementById('setupPage').style.display = 'none';
  document.getElementById('waitPage').style.display = 'block';

  var form = document.getElementById('configForm');
  var data = new URLSearchParams(new FormData(form)).toString();

  fetch('/wifi', { method: 'POST', body: data })
    .then(function(r) { return r.text(); })
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

static volatile bool s_web_stop = false;
static volatile bool s_dns_stop = false;

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

static void dns_task(void*) {
    ESP_LOGI(Tag, "DNS task starting on port 53");

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) { ESP_LOGE(Tag, "DNS socket failed"); vTaskDelete(nullptr); return; }

    struct sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(53);
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint8_t buf[512];
    while (!s_dns_stop) {
        struct sockaddr_in client = {};
        socklen_t clen = sizeof(client);
        int len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&client, &clen);
        if (len >= 12) {
            buf[2] = 0x81; buf[3] = 0x80;
            buf[6] = 0x00; buf[7] = 0x01;
            buf[8] = 0x00; buf[9] = 0x00;
            buf[10]= 0x00; buf[11]= 0x00;
            uint8_t answer[] = {
                0xC0, 0x0C, 0x00, 0x01, 0x00, 0x01,
                0x00, 0x00, 0x00, 0x3C, 0x00, 0x04,
                192, 168, 4, 1
            };
            if (len + (int)sizeof(answer) <= (int)sizeof(buf)) {
                memcpy(buf + len, answer, sizeof(answer));
                sendto(sock, buf, len + sizeof(answer), 0,
                       (struct sockaddr*)&client, clen);
            }
        }
    }
    close(sock);
    ESP_LOGI(Tag, "DNS task stopped");
    vTaskDelete(nullptr);
}

static void send_all(int sock, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int r = send(sock, data + sent, len - sent, 0);
        if (r < 0) break;
        sent += r;
    }
}

static void handle_client(int client_sock) {
    const int BUF_SZ = 4096;
    char* buf = (char*)malloc(BUF_SZ + 1);
    if (!buf) { close(client_sock); return; }

    int r = recv(client_sock, buf, BUF_SZ, 0);
    if (r <= 0) { free(buf); close(client_sock); return; }
    buf[r] = '\0';

    char method[8] = {}, uri[256] = {};
    if (sscanf(buf, "%7s %255s", method, uri) != 2) {
        free(buf); close(client_sock); return;
    }

    ESP_LOGI(Tag, "HTTP request: %s %s", method, uri);

    char* hdr_end    = strstr(buf, "\r\n\r\n");
    char* body       = hdr_end ? hdr_end + 4 : nullptr;
    int   content_len = 0;
    if (hdr_end) {
        char* cl = strcasestr(buf, "Content-Length:");
        if (cl) content_len = atoi(cl + 15);
    }

    if (strcmp(method, "GET") == 0 &&
        (strcmp(uri, "/") == 0 || strcmp(uri, "/hotspot-detect.html") == 0)) {
        char hdr[128];
        snprintf(hdr, sizeof(hdr),
                 "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                 "Content-Length: %d\r\n\r\n", (int)strlen(HTML_PAGE));
        send_all(client_sock, hdr, strlen(hdr));
        send_all(client_sock, HTML_PAGE, strlen(HTML_PAGE));
    }
    else if (strcmp(method, "GET") == 0 && strcmp(uri, "/status") == 0) {
        char resp[256];
        int timeout_ms = 30000;
        int waited = 0;
        while (!WebServer::isStaConnected() && waited < timeout_ms) {
            vTaskDelay(pdMS_TO_TICKS(500));
            waited += 500;
        }
        if (WebServer::isStaConnected()) {
            std::string ip = WebServer::getStaIP();
            snprintf(resp, sizeof(resp),
                "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
                "{\"status\":\"connected\",\"ip\":\"%s\"}", ip.c_str());
        } else {
            snprintf(resp, sizeof(resp),
                "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
                "{\"status\":\"connecting\",\"ip\":\"0.0.0.0\"}");
        }
        send_all(client_sock, resp, strlen(resp));
    }
    else if (strcmp(method, "POST") == 0 && strcmp(uri, "/wifi") == 0) {
        int body_received = body ? (r - (int)(body - buf)) : 0;
        while (body_received < content_len && r < BUF_SZ) {
            int n = recv(client_sock, buf + r, BUF_SZ - r, 0);
            if (n <= 0) break;
            r += n;
            body = hdr_end + 4;
            body_received = r - (int)(body - buf);
        }

        char ssid[64] = {}, password[128] = {}, broker[128] = {};
        if (body && content_len > 0) {
            char* tmp = (char*)malloc(content_len + 1);
            if (tmp) {
                memcpy(tmp, body, content_len);
                tmp[content_len] = '\0';

                char* pair = strtok(tmp, "&");
                while (pair) {
                    char* eq = strchr(pair, '=');
                    if (eq) {
                        *eq = '\0';
                        char* k = pair;
                        char* v = eq + 1;
                        url_decode_inplace(v);
                        if (strcmp(k, "ssid") == 0)     strncpy(ssid,     v, sizeof(ssid) - 1);
                        if (strcmp(k, "password") == 0) strncpy(password, v, sizeof(password) - 1);
                        if (strcmp(k, "broker") == 0)   strncpy(broker,   v, sizeof(broker) - 1);
                    }
                    pair = strtok(nullptr, "&");
                }
                free(tmp);
            }
        }

        trim_inplace(ssid);
        trim_inplace(password);
        trim_inplace(broker);

        ESP_LOGI(Tag, "Provisioning: SSID='%s' pass_len=%d broker='%s'",
                 ssid, (int)strlen(password), broker);

        NVSHandle nvs("wifi_config", NVS_READWRITE);
        if (nvs.isValid()) {
            nvs.setString("ssid",     std::string(ssid));
            nvs.setString("password", std::string(password));
            nvs.setString("broker",   std::string(broker));
            nvs.commit();
            ESP_LOGI(Tag, "Credentials saved to NVS");
        }

        const char* resp =
            "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
            "Content-Length: 60\r\n\r\n"
            "<html><body><h2>Saved</h2><p>Connecting...</p></body></html>";
        send_all(client_sock, resp, strlen(resp));

        if (strlen(ssid) > 0) {
            WebServer::setPendingWifiCredentials(
                std::string(ssid), std::string(password), std::string(broker));
            ESP_LOGI(Tag, "Pending WiFi credentials set");
        }
    }
    else {
        const char* resp =
            "HTTP/1.1 302 Found\r\nLocation: http://192.168.4.1/\r\n"
            "Content-Length: 0\r\n\r\n";
        send_all(client_sock, resp, strlen(resp));
    }

    free(buf);
    close(client_sock);
}

static void web_task(void*) {
    ESP_LOGI(Tag, "Web task starting on port 80");

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) { ESP_LOGE(Tag, "Socket failed"); vTaskDelete(nullptr); return; }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(80);
    bind(listen_sock,   (struct sockaddr*)&addr, sizeof(addr));
    listen(listen_sock, 4);

    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(listen_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    while (!s_web_stop) {
        struct sockaddr_in6 client_addr = {};
        socklen_t addr_len = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &addr_len);
        if (client_sock < 0) continue;
        handle_client(client_sock);
    }

    close(listen_sock);
    ESP_LOGI(Tag, "Web task stopped");
    vTaskDelete(nullptr);
}

WebServer& WebServer::getInstance() {
    static WebServer instance;
    return instance;
}

void WebServer::init() {
    s_web_stop = false;
    s_dns_stop = false;
    xTaskCreate(web_task, "web_task", 8192, nullptr, tskIDLE_PRIORITY + 5, nullptr);
    xTaskCreate(dns_task, "dns_task", 4096, nullptr, tskIDLE_PRIORITY + 4, nullptr);
}

void WebServer::start() {
    ESP_LOGI(Tag, "Starting provisioning web server");
    init();
}

void WebServer::stop() {
    ESP_LOGI(Tag, "Stopping web server");
    s_web_stop = true;
    s_dns_stop = true;
}

bool WebServer::getPendingWifiCredentials(std::string& out_ssid,
                                           std::string& out_password,
                                           std::string& out_broker) {
    if (!s_wifi_credentials_ready) return false;
    out_ssid     = s_pending_ssid;
    out_password = s_pending_password;
    out_broker   = s_pending_broker;
    s_wifi_credentials_ready = false;
    ESP_LOGI(Tag, "Pending credentials read: SSID='%s' broker='%s'",
             out_ssid.c_str(), out_broker.c_str());
    return true;
}

void WebServer::setPendingWifiCredentials(const std::string& ssid,
                                           const std::string& password,
                                           const std::string& broker) {
    s_pending_ssid     = ssid;
    s_pending_password = password;
    s_pending_broker   = broker;
    s_wifi_credentials_ready = true;
}

void WebServer::setStaConnectionInfo(const std::string& ip) {
    s_sta_ip        = ip;
    s_sta_connected = true;
    ESP_LOGI(Tag, "STA IP set: %s", ip.c_str());
}

bool WebServer::isStaConnected() {
    return s_sta_connected;
}

std::string WebServer::getStaIP() {
    return s_sta_ip;
}

} // namespace SARCUS
