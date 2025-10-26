#include "wifi_ap.h"

const char* WiFi_AP_TAG = "WiFi access point Driver";

WiFi_AP::WiFi_AP(const WiFi_cfg_t &copy_cfg, uint8_t ap_max_connection) : 
                    cfg{copy_cfg.ssid, copy_cfg.password, copy_cfg.state, copy_cfg.MaxRetry},
                    AP_MAX_CONNECTION(ap_max_connection) {
    this->init();
}

void WiFi_AP::wait_for_connect() {
    ESP_LOGI(WiFi_AP_TAG, "Access Point running, waiting for clients...");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000)); // log status every 10s
        ESP_LOGI(WiFi_AP_TAG, "AP active");
    }
}

void WiFi_AP::init() {
    // init network interface abstraction layer
    ESP_ERROR_CHECK(esp_netif_init());

    // create default event loop for handling events
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // create default AP network interface
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    assert(ap_netif);

    // init WiFi driver with default configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // register event handler
    esp_event_handler_instance_t instance_any_id;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &WiFi_AP::wifi_event_handler,
        this,
        &instance_any_id
    ));

    // fill AP configuration
    wifi_config_t wifi_cfg = {};
    strncpy((char*)wifi_cfg.ap.ssid, this->cfg.ssid.c_str(), sizeof(wifi_cfg.ap.ssid));
    strncpy((char*)wifi_cfg.ap.password, this->cfg.password.c_str(), sizeof(wifi_cfg.ap.password));
    wifi_cfg.ap.ssid_len = this->cfg.ssid.length();
    wifi_cfg.ap.channel = 1;
    wifi_cfg.ap.max_connection = this->AP_MAX_CONNECTION;
    wifi_cfg.ap.authmode = WIFI_AUTHMODE;

    if (this->cfg.password.empty()) {
        wifi_cfg.ap.authmode = WIFI_AUTH_OPEN;
    }

    // set WiFi mode and config
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg));

    // start WiFi driver
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WiFi_AP_TAG, "WiFi initialized in AP mode");
    ESP_LOGI(WiFi_AP_TAG, "SSID: %s | Password: %s | Max Conn: %d", 
             wifi_cfg.ap.ssid, wifi_cfg.ap.password, wifi_cfg.ap.max_connection);
}

void WiFi_AP::wifi_event_handler(void *arg,
                            esp_event_base_t event_base,
                            int32_t event_id,
                            void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(WiFi_AP_TAG, "Access Point started");
    } 

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        auto* event = (wifi_event_ap_staconnected_t*)event_data;
        ESP_LOGI(WiFi_AP_TAG, "Device connected: AID=%d", event->aid);
    } 

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        auto* event = (wifi_event_ap_stadisconnected_t*)event_data;
        ESP_LOGI(WiFi_AP_TAG, "Device disconnected: AID=%d", event->aid);
    }
}