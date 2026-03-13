# Project Analysis & Prompt Generation Walkthrough

I have completed a thorough analysis of the **SarcoOS** project to create a comprehensive documentation prompt for Claude AI.

## 🛠️ Analysis Steps

1.  **Codebase Exploration**:
    *   Identified the **Layered Architecture (HAL/MCAL)** in `code/esp32_driver`.
    *   Analyzed drivers for `bts7960`, `mpu6050`, `tmc2209`, `gps`, and `pid`.
    *   Verified the MQTT/WiFi logic in `main.cpp` and `WiFiManager`.
2.  **HMI & Telemetry**:
    *   Analyzed `code/node_red_HMI_IIot/inputs.json` to map the dashboard topics (e.g., `arm_right/shoulder1`, `move/forward`).
3.  **Hardware Mapping**:
    *   Used the provided schematic images and C++ source code to map the ESP32 pins for all components (TMC, BTS, GPS, MPU6050).
4.  **Mechanical & ROS**:
    *   Incorporated the SolidWorks-to-URDF workflow, Gazebo simulation, and SLAM/Localization details as requested.

## 📄 Final Deliverable

I have generated a "Master Prompt" that you can use with Claude AI to write your entire project book. It includes all the technical details, team info, and architectural reasoning.

**Prompt File:** [CLAUDE_PROMPT.md](file:///home/ziad/ziad_ws/sarcoos_project/docs/CLAUDE_PROMPT.md)

### Key Content Included:
*   **Software Stack**: C++, ESP-IDF, FreeRTOS, CMake.
*   **Hardware connections**: Precise pin mapping for the custom PCB.
*   **Wearable Suit**: Detailed explanation of the teleoperation logic.
*   **ROS 2 Flow**: Analysis of SLAM, Mapping, and Gazebo integration.

---
*"Perfection is attained, not when there is nothing more to add, but when there is nothing left to take away."*
