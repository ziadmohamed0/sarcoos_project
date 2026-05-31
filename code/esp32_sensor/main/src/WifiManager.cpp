// =============================================================================
//  SARCUS Robot — WiFi Manager Implementation
//  AP mode (provisioning captive portal) → STA mode (connected to network).
//  Credentials stored/loaded from NVS.
//  Singleton. FreeRTOS EventGroup for IP-ready signalling.
// =============================================================================

#include "inc/WifiManager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include <cstring>

namespace SARCUS {

static const char* Tag = "WifiManager";

// ─── Singleton ────────────────────────────────────────────────────────────────

WifiManager& WifiManager::getInstance() {
    static WifiManager instance;
    return instance;
}

// ─── Event Handler ────────────────────────────────────────────────────────────

void WifiManager::eventHandler(void* arg, esp_event_base_t base,
                                int32_t id, void* data) {
    auto* self = static_cast<WifiManager*>(arg);
    ESP_LOGI(Tag, "eventHandler: base=%s id=%d", base, id);

    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(Tag, "STA started → connecting...");
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(Tag, "STA connected to AP");
            break;

        case WIFI_EVENT_STA_DISCONNECTED: {
            auto* info = static_cast<wifi_event_sta_disconnected_t*>(data);
            ESP_LOGW(Tag, "STA disconnected (reason=%u), retry=%u/%u",
                     info->reason, self->m_retry_cnt, kMaxRetries);
            self->m_connected = false;

            if (self->m_retry_cnt < kMaxRetries) {
                self->m_retry_cnt++;
                esp_wifi_connect();
            } else {
                ESP_LOGW(Tag, "Max retries reached — giving up STA connect");
                // Signal failure so app_main can fall back to AP
                if (self->m_ip_ready) {
                    xEventGroupClearBits(self->m_ip_ready, kIpReadyBit);
                }
            }
            break;
        }

        case WIFI_EVENT_AP_STACONNECTED: {
            auto* info = static_cast<wifi_event_ap_staconnected_t*>(data);
            ESP_LOGI(Tag, "AP: Station connected — MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                     info->mac[0], info->mac[1], info->mac[2],
                     info->mac[3], info->mac[4], info->mac[5]);
            // Signal IP-ready so waitForIP() returns (AP gives DHCP lease)
            if (self->m_ip_ready) {
                xEventGroupSetBits(self->m_ip_ready, kIpReadyBit);
            }
            break;
        }

        case WIFI_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(Tag, "AP: Station disconnected");
            break;

        case WIFI_EVENT_HOME_CHANNEL_CHANGE:
            ESP_LOGI(Tag, "WIFI_EVENT: home channel changed");
            break;

        default:
            ESP_LOGD(Tag, "Ignored WIFI_EVENT id=%d", id);
            break;
        }
    }
    else if (base == IP_EVENT) {
        if (id == IP_EVENT_STA_GOT_IP) {
            auto* evt = static_cast<ip_event_got_ip_t*>(data);
            ESP_LOGI(Tag, "STA got IP: " IPSTR, IP2STR(&evt->ip_info.ip));
            self->m_connected  = true;
            self->m_retry_cnt  = 0;
            if (self->m_ip_ready) {
                xEventGroupSetBits(self->m_ip_ready, kIpReadyBit);
            }
        }
        else if (id == IP_EVENT_STA_LOST_IP) {
            ESP_LOGW(Tag, "STA lost IP");
            self->m_connected = false;
        }
        else {
            ESP_LOGI(Tag, "Unhandled IP_EVENT id=%d", id);
        }
    }
}

// ─── init ─────────────────────────────────────────────────────────────────────

void WifiManager::init() {
    ESP_LOGI(Tag, "[ENTRY] init()");

    // Create event group before registering handlers
    m_ip_ready = xEventGroupCreate();
    if (!m_ip_ready) {
        ESP_LOGE(Tag, "Failed to create event group");
        return;
    }

    // Initialize TCP/IP stack and default event loop (idempotent)
    ESP_ERROR_CHECK(esp_netif_init());
    
    // esp_event_loop_create_default() is idempotent in IDF, but we check anyway
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(Tag, "Event loop already created (ESP_ERR_INVALID_STATE) — OK");
    }

    // Create default netif objects (will be reused for AP/STA)
    m_netif_sta = esp_netif_create_default_wifi_sta();
    m_netif_ap  = esp_netif_create_default_wifi_ap();

    // WiFi init with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &eventHandler, this, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &eventHandler, this, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_LOST_IP, &eventHandler, this, nullptr));

    ESP_LOGI(Tag, "WiFi stack initialized");
    ESP_LOGI(Tag, "[EXIT]  init()");
}

// ─── startAP ──────────────────────────────────────────────────────────────────

void WifiManager::startAP() {
    ESP_LOGI(Tag, "[ENTRY] startAP()");

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t ap_config = {};
    strncpy((char*)ap_config.ap.ssid,     "SARCUS_SETUP", sizeof(ap_config.ap.ssid));
    strncpy((char*)ap_config.ap.password, "sarcus2024",   sizeof(ap_config.ap.password));
    ap_config.ap.ssid_len       = 0;   // 0 = auto-detect strlen
    ap_config.ap.channel        = 1;
    ap_config.ap.authmode       = WIFI_AUTH_WPA2_PSK;
    ap_config.ap.max_connection = 4;
    ap_config.ap.beacon_interval= 100;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(Tag, "AP started — SSID: SARCUS_SETUP | Pass: sarcus2024");
    ESP_LOGI(Tag, "[EXIT]  startAP()");
}

// ─── startSTA ─────────────────────────────────────────────────────────────────

void WifiManager::startSTA(const std::string& ssid, const std::string& password) {
    ESP_LOGI(Tag, "[ENTRY] startSTA() ssid='%s'", ssid.c_str());

    m_retry_cnt = 0;
    m_connected = false;
    xEventGroupClearBits(m_ip_ready, kIpReadyBit);

    // Stop any running WiFi first
    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT && err != ESP_ERR_WIFI_STATE) {
        ESP_LOGW(Tag, "esp_wifi_stop() returned %s", esp_err_to_name(err));
    }

    ESP_LOGI(Tag, "startSTA: setting mode WIFI_MODE_STA");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t sta_config = {};
    strncpy((char*)sta_config.sta.ssid,     ssid.c_str(),     sizeof(sta_config.sta.ssid) - 1);
    strncpy((char*)sta_config.sta.password, password.c_str(), sizeof(sta_config.sta.password) - 1);
    sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    sta_config.sta.pmf_cfg.capable    = true;
    sta_config.sta.pmf_cfg.required   = false;

    ESP_LOGI(Tag, "startSTA: applying STA config");
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));

    ESP_LOGI(Tag, "startSTA: starting WiFi");
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(Tag, "startSTA: esp_wifi_start() returned");

    ESP_LOGI(Tag, "startSTA: connecting to AP immediately");
    esp_err_t connect_err = esp_wifi_connect();
    if (connect_err != ESP_OK) {
        ESP_LOGW(Tag, "esp_wifi_connect() returned %s", esp_err_to_name(connect_err));
    }

    ESP_LOGI(Tag, "STA mode started, waiting for connection...");
    ESP_LOGI(Tag, "[EXIT]  startSTA()");
}

// ─── waitForIP ────────────────────────────────────────────────────────────────

bool WifiManager::waitForIP(TickType_t timeout_ticks) {
    ESP_LOGD(Tag, "[ENTRY] waitForIP()");

    if (!m_ip_ready) return false;

    EventBits_t bits = xEventGroupWaitBits(
        m_ip_ready,
        kIpReadyBit,
        pdFALSE,     // don't clear on exit
        pdTRUE,
        timeout_ticks
    );

    bool ok = (bits & kIpReadyBit) != 0;
    ESP_LOGD(Tag, "[EXIT]  waitForIP() → %s", ok ? "OK" : "TIMEOUT");
    return ok;
}

// ─── stop ─────────────────────────────────────────────────────────────────────

void WifiManager::stop() {
    ESP_LOGI(Tag, "[ENTRY] stop()");
    m_connected = false;
    esp_wifi_stop();
    esp_wifi_deinit();
    ESP_LOGI(Tag, "[EXIT]  stop()");
}

} // namespace SARCUS
