idf_component_register(
    SRCS "src/main.cpp"
         "src/ProvisioningServer.cpp"
    INCLUDE_DIRS "inc"
    REQUIRES 
            drivers
            esp_wifi
        esp_event
        nvs_flash
        esp_http_server
        esp_netif
        driver
        esp_driver_gpio
        esp_driver_uart
        mqtt
        freertos
        esp_timer
        log
)
