# @author: Ziad Mohammed Fathy 
# @brief: cmake file to build project 
# @notes: to add source file add in set function.

cmake_minimum_required(VERSION 3.10)
project(SARCOS_ROBOT)

set(SRC
    src/main.cpp
)

include_directories(inc)
add_executable(${PROJECT_NAME} ${SRC})