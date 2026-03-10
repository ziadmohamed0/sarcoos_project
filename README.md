# SarcoOS - Teleoperation Suit & Robotic System

**SarcoOS** is a modular system designed for teleoperation and robotic control, centering on an ESP32-powered suit that captures human motion and transmits control data to robotic platforms via MQTT.

---

## 🚀 System Architecture

This project integrates hardware design, firmware development, and IoT dashboards:

- 🧤 **Teleoperation Suit (ESP32)**  
  - Captures joint angles using potentiometers and button states for grippers/movement.
  - Processes data locally and publishes to an MQTT broker.
  
- ⚙️ **Modular Driver Architecture (HAL/MCAL)**  
  - Structured ESP-IDF firmware with Hardware Abstraction Layers (HAL) for sensors and actuators, and Microcontroller Abstraction Layers (MCAL) for peripherals (WiFi, MQTT, UART, NVS).

- 🌐 **IoT & HMI (Node-RED)**  
  - Real-time monitoring and control through a Node-RED dashboard.
  - Handles bidirectional communication between the suit and the robot.

- 🤖 **Mechanical Design**  
  - Custom 3D-printable components designed in SolidWorks for the robotic chassis and suit mounting.

---

## 🧩 Project Structure

| Folder | Description |
| :--- | :--- |
| [`code/esp32_driver`](file:///home/ziad/ziad_ws/sarcoos_project/code/esp32_driver) | ESP-IDF firmware for the ESP32 control suit. |
| [`sarcos_design`](file:///home/ziad/ziad_ws/sarcoos_project/sarcos_design) | Mechanical CAD files (SolidWorks) for robot parts. |
| [`code/node_red_HMI_IIot`](file:///home/ziad/ziad_ws/sarcoos_project/code/node_red_HMI_IIot) | Node-RED flow exports for the HMI dashboard. |
| [`pdf`](file:///home/ziad/ziad_ws/sarcoos_project/pdf) | Project documentation, schematics, and design diagrams. |

---

## 🛠️ Firmware Features (ESP32)

- [x] **Modular HAL/MCAL**: Clean separation between hardware-specific drivers and system services.
- [x] **WiFi Management**: Robust connection handling with auto-reconnect.
- [x] **MQTT Client**: Real-time publishing of sensor data (Potentiometers, Buttons).
- [x] **NVS Integration**: Non-volatile storage for configuration persistence.
- [x] **Device Drivers**: Support for BTS7960, MPU6050, Ultrasonic, Servo, and more.

---

## 🧪 Getting Started

### Prerequisites
- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
- [Node-RED](https://nodered.org/docs/getting-started/)
- [MQTT Broker](https://mosquitto.org/) (e.g., Mosquitto)

### Build & Flash Firmware
1. Navigate to the firmware directory:
   ```bash
   cd code/esp32_driver
   ```
2. Build the project:
   ```bash
   idf.py build
   ```
3. Flash to your ESP32:
   ```bash
   idf.py -p [PORT] flash monitor
   ```

---

## 🧑‍💻 Maintainer

Developed by **Ziad Mohammed Fathy**  
🔗 GitHub: [https://github.com/ziadmohamed0](https://github.com/ziadmohamed0)

---

## 🛡️ License

MIT License
