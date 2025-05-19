cmake_minimum_required(VERSION 3.10)
project(arms_suit)

set(CMAKE_CXX_STANDARD 17)

# Find Threads (for pthread)
find_package(Threads REQUIRED)

# Manually locate Paho MQTT libraries (since find_package fails)
find_library(PAHO_MQTTCPP_LIB paho-mqttpp3)
find_library(PAHO_MQTT_C_LIB paho-mqtt3as)
find_path(PAHO_INCLUDE_DIR MQTTAsync.h PATH_SUFFIXES paho-mqtt)

if(NOT PAHO_MQTTCPP_LIB OR NOT PAHO_MQTT_C_LIB OR NOT PAHO_INCLUDE_DIR)
    message(FATAL_ERROR "Paho MQTT libraries not found! Install with: sudo apt-get install libpaho-mqttpp-dev libpaho-mqtt3-dev")
endif()

include_directories(inc ${PAHO_INCLUDE_DIR})

add_executable(arms_suit
    main.cpp
    src/servo.cpp
    src/mqtt_client.cpp
)

target_link_libraries(arms_suit
    ${PAHO_MQTTCPP_LIB}
    ${PAHO_MQTT_C_LIB}
    pigpio
    ${CMAKE_THREAD_LIBS_INIT}
    rt
)
