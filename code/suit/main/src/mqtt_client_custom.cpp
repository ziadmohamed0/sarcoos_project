#include "../inc/mqtt_client_custom.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include "freertos/event_groups.h"

#define WIFI_CONNECTED_BIT BIT0
static EventGroupHandle_t wifi_event_group;
static esp_mqtt_client_handle_t client = NULL;
static const char* TAG = "MQTTClient";
static std::function<void(const char*, const char*)> global_cb;

mqtt_client_custom::mqtt_client_custom(const std::string& copySsd, 
                                        const std::string& copyPassword, 
                                        const std::string& copyBroker) : 
                                            ssd(copySsd),
                                            password(copyPassword),
                                            broker(copyBroker) {}

void mqtt_client_custom::begin() {

}

void mqtt_client_custom::publish(const std::string& copyTopic, const std::string& copyPayload) {

}

void mqtt_client_custom::subscrib(const std::string& copyTopic) {

}

void mqtt_client_custom::on_message(std::function<void(const char*, const char*)> cb) {

}

void mqtt_client_custom::init_wifi() {

}

void mqtt_client_custom::init_mqtt() {

}