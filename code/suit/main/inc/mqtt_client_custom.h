/**
 * @file mqtt_client_custom.h
 * @author Ziad Mohammed Fathy
 * @brief mqtt driver.
 * @version 0.1
 * @date 2025-05-20
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef _INC_MQTT_CLIENT_CUSTOM_H_
#define _INC_MQTT_CLIENT_CUSTOM_H_

#include <string>
#include <functional>
#include "mqtt_client.h"

class mqtt_client_custom {
    public:
        /**
         * @brief Construct a new mqtt client custom object
         * 
         * @param ssd 
         * @param password 
         * @param broker 
         */
        mqtt_client_custom(const std::string& copySsd, const std::string& copyPassword, const std::string& copyBroker);
        
        /**
         * @brief 
         * 
         */
        void begin();

        /**
         * @brief 
         * 
         * @param copyTopic 
         * @param copyPayload 
         */
        void publish(const std::string& copyTopic, const std::string& copyPayload);

        /**
         * @brief 
         * 
         * @param copyTopic 
         */
        void subscrib(const std::string& copyTopic);

        /**
         * @brief 
         * 
         * @param cb 
         */
        void on_message(std::function<void(const char*, const char*)> cb);
    private:
        std::string ssd;
        std::string password;
        std::string broker;
        std::function<void(std::string&, std::string&)> callback;
        void init_wifi();
        void init_mqtt();
};

#endif