<div align="center">
  <img src="docs/assets/logo.png" width="220" alt="SarcoOS Logo">
  <h1>SarcoOS</h1>
  <p><strong>Elite Modular RTOS Framework for Teleoperation & Robotics</strong></p>

  [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
  [![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue?logo=espressif)](https://www.espressif.com/en/products/socs/esp32)
  [![Framework: ESP-IDF](https://img.shields.io/badge/Framework-ESP--IDF-red)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
  [![HMI: Node-RED](https://img.shields.io/badge/HMI-Node--RED-8F0000?logo=node-red)](https://nodered.org/)
</div>

---

## 📖 Project Essence

**SarcoOS** is a high-performance, modular firmware ecosystem designed to synchronize human kinetic movement with robotic precision. Developed with an emphasis on **Industrial IoT (IIoT)** standards, it facilitates a seamless teleoperation experience through a robust **ESP32-based suit** that communicates over a distributed MQTT mesh.

This repository serves as a complete solution—encompassing **Low-Level Embedded C++**, **Precision CAD Design**, and **Interactive HMI Dashboards**.

---

## 📁 Repository Structure

- `3d Desgin/` & `sarcos_design/`: CAD models and SolidWorks assemblies for the robotic platform and wearable suit.
- `PCB_Design/`: KiCad project files for the custom control hardware, including schematics and PCB layouts.
- `code/esp32_driver/`: ESP-IDF based firmware implementing the modular HAL/MCAL architecture.
- `code/node_red_HMI_IIot/`: Node-RED flows for the teleoperation dashboard (`inputs.json` & `outputs.json`).
- `docs/`: Technical manuals, software documentation, and AI prompts.
- `pdf/`: Compiled PDF datasheets and project overviews.

---

## 🏗️ System Architecture & Data Orchestration

SarcoOS is built on a strict **HAL/MCAL (Hardware/Microcontroller Abstraction Layer)** hierarchy, ensuring that high-level robotic logic remains decoupled from the underlying silicon.

### 🔌 Real-Time Telemetry Flow
Data originates from physical sensors on the operator's suit and is dispatched through a deterministic processing pipeline.

```mermaid
graph TD
    subgraph "Teleoperation Node (ESP32)"
        Sensors[Potentiometers & IMU] -->|Raw ADC/I2C| HAL[HAL: Pot, MPU6050]
        HAL -->|Normalized Data| Logic[App: Input Mapping]
        Logic -->|Commands| MCAL[MCAL: WiFi, MQTT, NVS]
    end

    MCAL --> |QoS 1 MQTT| Broker[Mosquitto Broker]
    
    subgraph "Execution & Monitoring"
        Broker -->|Control Packets| Robot[Robotic Platform: BTS7960]
        Broker -->|Telemetry| NodeRED[Node-RED IIoT Dashboard]
    end

    style Sensors fill:#f9f,stroke:#333,stroke-width:2px
    style Robot fill:#bbf,stroke:#333,stroke-width:2px
    style NodeRED fill:#dfd,stroke:#333,stroke-width:2px
```

---

## ⚙️ Engineering Deep-Dive

### 🏎️ Firmware Engineering (ESP-IDF)
The firmware is structured to support complex robotics tasks with minimal overhead.

*   **HAL (Hardware Abstraction Layer)**:
    - **BTS7960**: High-current DC motor driver.
    - **MPU6050**: 6-DOF Inertial Measurement Unit (IMU).
    - **PID Control**: Closed-loop regulation for precise positioning.
    - **TCRT5000**: Infrared sensors for line/position detection.
    - **Ultrasonic (HC-SR04)**: Distance measurement for obstacle avoidance.
    - **Servo/PWM**: Precision angle control for robotic joints.
    - **Potentiometer**: Analog input mapping for suit joints.
    - **DC Motor**: Generic motor control layer.
    - **LED & Button**: Basic HMI and status indicators.
*   **MCAL (Microcontroller Abstraction Layer)**:
    - **MQTT Client**: QoS-aware asynchronous messaging.
    - **WiFi**: Robust connectivity management.
    - **NVS (Non-Volatile Storage)**: Calibration and state persistence.
    - **UART**: Serial communication for debugging and peripherals.

### 📐 Mechanical & Electronic Design
- **SolidWorks (Mechanical)**: Detailed assemblies in `sarcos_design/` featuring high-torque motor mounts, 65mm tires, and the modular teleoperation suit frame.
- **KiCad (Electronic)**: Custom PCB designed to interface ESP32 with various drivers (TMC2209 compatible), featuring optimized power distribution for high-current motors.
- **Technical Manuals**: Comprehensive guides available in [docs/SarcoOS_Technical_Manual_v1.0.docx.pdf](file:///home/ziad/ziad_ws/sarcoos_project/docs/SarcoOS_Technical_Manual_v1.0.docx.pdf).

### 📊 IIoT Dashboard (Node-RED)
The dashboard provides an intuitive interface for mission control:
- **Rover Group**: Real-time directional switches with latency monitoring.
- **Arm Telemetry**: Synchronized gauges and sliders for dual-arm configurations.
- **Gripper Control**: Independent toggle states for precision manipulation.
- **Deployment**: Import `inputs.json` and `outputs.json` directly into your Node-RED instance.

---

## 🛠️ Deployment & Calibration

### 1. Embedded Firmware
```bash
# Clone and setup environment
git clone https://github.com/ziadmohamed0/sarcoos_project.git
cd code/esp32_driver

# Build and flash to ESP32
idf.py build
idf.py -p [PORT] flash monitor
```

### 2. HMI Configuration
- Import both `inputs.json` and `outputs.json` into Node-RED.
- Ensure the MQTT broker URI matches your environment (default: `mqtt://192.168.100.25`).

---

## 🔍 Troubleshooting & FAQ

**Q: ESP32 is flashing but MQTT is not connecting.**  
> **Check**: Verify the WiFi credentials in `main.h`. Ensure your MQTT broker is reachable at the defined IP and port 1883.

**Q: Potentiometer data is jittery.**  
> **Solution**: Check the ADC attenuation settings in the HAL. Ensure sensors have common ground with the ESP32.

**Q: Servo movement is stuttering.**  
> **Solution**: Verify the power supply; servos often require more current than the ESP32 internal regulator can provide.

---

## 🧑‍💻 Technical Leadership

**Ziad Mohammed Fathy**  
*Robotics & Embedded Systems Specialist*  
🔗 [GitHub Profile](https://github.com/ziadmohamed0) | [LinkedIn](https://www.linkedin.com/in/ziad-mohamed-fathy/)

---

## 🛡️ License & Acknowledgements

Authorized under the **MIT License**.  
*Special thanks to the Open Source robotics community for the inspiration behind the modular HAL design.*

> *"Perfection is attained, not when there is nothing more to add, but when there is nothing left to take away."* — Antoine de Saint-Exupéry
