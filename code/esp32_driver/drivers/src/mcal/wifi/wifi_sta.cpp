#include "wifi_sta.h"

const char* WiFi_STA_TAG = "WiFi station Driver";

WiFi_STA::WiFi_STA(const WiFi_cfg_t &copy_cfg) : 
            cfg{copy_cfg.ssid, copy_cfg.password, copy_cfg.state, copy_cfg.MaxRetry} {
    this->init();
} 

void WiFi_STA::wifi_event_handler(void *arg, 
                        esp_event_base_t event_base, 
                        int32_t event_id, void* event_data) {
    WiFi_STA* self = static_cast<WiFi_STA*>(arg);
    
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)  {
        esp_wifi_connect();
    }

    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if(self->s_retry_counter < self->cfg.MaxRetry) {
            esp_wifi_connect();
            self->s_retry_counter++;
            ESP_LOGI(WiFi_STA_TAG, "Retry to connect to AP");
        }
        else {
            xEventGroupSetBits(self->s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(WiFi_STA_TAG, "Faild to connect to AP");
    }

    else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WiFi_STA_TAG, "GOT IP: %d.%d.%d.%d", IP2STR(&event->ip_info.ip));
        self->s_retry_counter = 0;
        xEventGroupSetBits(self->s_wifi_event_group, WIFI_CONNECT_BIT);
    }
}


void WiFi_STA::init() {
    // create freeRtos event group for wifi events.
    this->s_wifi_event_group = xEventGroupCreate(); 

    // init network interface abstraction layer.
    ESP_ERROR_CHECK(esp_netif_init()); 

    //create default event loop for handeling event.
    ESP_ERROR_CHECK(esp_event_loop_create_default()); 

    // create a defaulte wifi station network interface
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta(); 

    // Ensure the returned pointer is valid.
    assert(sta_netif);

    // defaulte wifi driver init configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    // init wifi driver with defaulte config
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // register wifi  and ip events handler so our wifi_event_handelr will be called.
    esp_event_handler_instance_t intance_any_id;
    esp_event_handler_instance_t intance_got_id;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                         ESP_EVENT_ANY_ID,
                                                         WiFi_STA::wifi_event_handler,
                                                         this,
                                                         &intance_any_id));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                         IP_EVENT_STA_GOT_IP,       
                                                         WiFi_STA::wifi_event_handler,
                                                         this,
                                                         &intance_got_id));

    // fill the wifi_config_t structure with ssid , password and security parameters.
    wifi_config_t wifi_cfg = {};
    strncpy((char*)wifi_cfg.sta.ssid, this->cfg.ssid.c_str(), sizeof(wifi_cfg.sta.ssid));
    strncpy((char*)wifi_cfg.sta.password, this->cfg.password.c_str(), sizeof(wifi_cfg.sta.password));
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_cfg.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;


    // set wifi interface to station client
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    // apply station configurations
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));

    // start wifi driver
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WiFi_STA_TAG, "WiFi init in STA mode.");
}

void WiFi_STA::wait_for_connect() {
    EventBits_t bits = xEventGroupWaitBits(
        this->s_wifi_event_group,
        WIFI_CONNECT_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY
    );

    if(bits & WIFI_CONNECT_BIT) {
        ESP_LOGI(WiFi_STA_TAG, "connected to access point successfully");
    }

    else if(bits & WIFI_FAIL_BIT) {
        ESP_LOGI(WiFi_STA_TAG, "faild connected to access point");
    }
}