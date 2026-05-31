# SARCUS Robot — Full System Prompt
# Assistive Exoskeleton Robot for Mobility-Impaired Users

---

## PROJECT OVERVIEW

**Project Name:** SARCUS Robot
**Type:** Assistive Humanoid / Wheeled Exoskeleton Robot
**Purpose:** Enable mobility-impaired individuals (wheelchair-dependent) to control a robotic exoskeleton through multiple interfaces: a wearable suit, a Node-RED dashboard, and ROS2.
**Target Users:** People with physical disabilities who cannot walk independently.

---

## HARDWARE ARCHITECTURE

### 1. Locomotion System
| Component | Qty | Role |
|---|---|---|
| DC Motor 24V (Large) | 2 | Wheel drive — forward / backward / turn left / turn right / stop |
| BTS7960 Motor Driver | 3 | Drive large DC motors (H-bridge, high current) |

### 2. Upper Body Joints
| Component | Qty | Role |
|---|---|---|
| DC Motor 24V (Small) | 2 | Shoulder rotation — left / right |
| Feetech Servo 95kg | 2 | Shoulder joint — second axis (left / right) |
| Feetech Servo 35kg | 2 | Elbow joint — flexion / extension |
| NEMA23 Stepper Motor | 2 | Wrist rotation |
| L298N Motor Driver | 2 | Drive small DC motors |
| TB6600 Stepper Driver | 2 | Drive NEMA23 steppers |
| PCA9685 (16ch PWM) | 1 | PWM generation for all servo motors |

### 3. Power System
| Component | Specs | Powers |
|---|---|---|
| Power Supply #1 | 24V / 20A | Large + small DC motors |
| Power Supply #2 | 12V / 30A | Servos, controllers, sensors |
| Buck Converter | x2 | Step-down to 5V / 3.3V for logic |

### 4. Computing & Control Units
| Component | Qty | Role |
|---|---|---|
| ESP32 (Robot) | 1 | WiFi gateway + UART bridge to STM32 & RPi |
| ESP32 (Suit) | 1 | Read suit inputs (buttons + pots) → send to Robot ESP32 |
| STM32F405RGT6 | 1 | Real-time motor control via UART from Robot ESP32 |
| Raspberry Pi 4 (2GB) | 1 | ROS2 Humble host — UART with Robot ESP32 |

### 5. Sensors & Modules
| Component | Qty | Role |
|---|---|---|
| MPU6050 (IMU) | 1 | Balance & orientation (6-axis) |
| NEO-7M GPS | 1 | Location tracking |
| Ultrasonic HC-SR04 | 4 | Obstacle detection — front / back / left / right |

### 6. Exoskeleton Suit (Wearable Controller)
| Component | Qty | Role |
|---|---|---|
| Push Buttons | 20 | Discrete command inputs |
| Potentiometers | 20 | Analog joint angle control |
| ESP32 | 1 | Data acquisition → transmit to Robot ESP32 |

---

## COMMUNICATION ARCHITECTURE

```
[Suit ESP32]
     |
  WiFi / UART
     |
[Robot ESP32] ──── UART ────► [STM32F405]  → Motor Drivers → Motors
     |                              (BTS7960 x3, L298N x2, TB6600 x2, PCA9685)
  UART
     |
[RPi 4] → ROS2 Humble

[Node-RED + Mosquitto MQTT]
     ↕ WiFi/MQTT
[Robot ESP32]
```

### Internal UART Frame Protocol
```
[0xAA][DEVICE_ID][CMD_TYPE][DATA_LEN][DATA...][CHECKSUM][0x55]
```
- START: 0xAA | END: 0x55
- CHECKSUM: XOR of all bytes between START and END
- All devices share this frame format

### MQTT Topic Structure (Mosquitto)
```
sarcus/robot/movement        # Locomotion commands
sarcus/robot/joints/shoulder # Shoulder joint commands
sarcus/robot/joints/elbow    # Elbow joint commands
sarcus/robot/joints/wrist    # Wrist (stepper) commands
sarcus/sensors/imu           # MPU6050 data
sarcus/sensors/gps           # GPS coordinates
sarcus/sensors/ultrasonic    # 4x distance readings
sarcus/suit/buttons          # 20 button states (bitmask)
sarcus/suit/pots             # 20 potentiometer values (0–4095)
sarcus/debug/logs            # System logs
sarcus/debug/heartbeat       # Device heartbeat (every 500ms)
```

---

## CONTROL SYSTEMS

### Mode 0 — WiFi Provisioning (AP → STA)
- On first boot, Robot ESP32 starts in **Access Point** mode (SSID: "SARCUS_SETUP")
- User connects and opens a captive **web portal** (ESP-IDF HTTP server)
- User enters home/network WiFi credentials
- ESP32 saves credentials to **NVS (Non-Volatile Storage)**
- ESP32 reboots into **Station (STA)** mode and connects to target WiFi
- All other systems initialize after STA connection is confirmed
- On subsequent boots: skip AP mode if valid credentials exist in NVS

### Mode 1 — Node-RED Dashboard (via MQTT)
- Full robot control through a browser-based dashboard
- Broker: **Mosquitto** running locally or on RPi
- Dashboard panels:
  - Joystick / direction buttons for locomotion
  - Sliders for each joint (shoulder, elbow, wrist)
  - Sensor display: IMU angles, GPS coords, 4x ultrasonic distances
  - Debug console: live MQTT log stream
  - Emergency stop button (priority command)
- QoS level: 1 for commands, 0 for sensor data

### Mode 2 — Exoskeleton Suit Control
- Suit ESP32 reads 20 buttons + 20 pots at 50Hz
- Data packed into a compact binary frame, sent to Robot ESP32
- Robot ESP32 decodes and maps:
  - Buttons → discrete joint commands / locomotion
  - Pots → proportional joint angle targets
- Communication: WiFi UDP (primary) / UART (fallback)

### Mode 3 — ROS2 Humble (on RPi 4)
- RPi runs ROS2 Humble with a custom robot package
- Communicates with Robot ESP32 via UART (micro-ROS or custom bridge)
- ROS2 Topics:
  - /sarcus/cmd_vel          (locomotion Twist)
  - /sarcus/joint_states     (current joint angles)
  - /sarcus/joint_commands   (target joint angles)
  - /sarcus/imu              (sensor_msgs/Imu)
  - /sarcus/gps              (sensor_msgs/NavSatFix)
  - /sarcus/ultrasonic       (sensor_msgs/Range x4)
- Supports Navigation2 integration for autonomous obstacle avoidance
- Debug: rqt, ros2 topic echo, ros2 bag record

---

## DEBUGGING REQUIREMENTS

Every firmware layer must implement:

1. **Serial Logging** — structured log levels: [DEBUG] [INFO] [WARN] [ERROR]
2. **MQTT Debug Topic** — sarcus/debug/logs receives all logs from all devices
3. **Heartbeat System** — every device publishes to sarcus/debug/heartbeat every 500ms
4. **Unified Error Codes** — shared enum across all devices:
   - 0x01: UART timeout
   - 0x02: Motor driver fault
   - 0x03: IMU read failure
   - 0x04: GPS no fix
   - 0x05: WiFi disconnected
   - 0x06: MQTT disconnected
5. **LED Status Indicators** (per device):
   - Solid ON: normal operation
   - Slow blink (1Hz): connecting/initializing
   - Fast blink (5Hz): error state
6. **ROS2 Diagnostics** — /diagnostics topic with hardware status
7. **Node-RED Debug Nodes** — inline debug nodes on all flows

---

## DEVELOPMENT ENVIRONMENT

| Device | IDE | Language | Notes |
|---|---|---|---|
| ESP32 (Robot) | ESP-IDF | C++ | FreeRTOS tasks, NVS, HTTP server, MQTT, UART |
| ESP32 (Suit) | ESP-IDF | C++ | ADC multisampling, GPIO ISR, UDP/UART TX |
| STM32F405RGT6 | STM32CubeIDE + CubeMX | C | HAL, TIM PWM, UART DMA, GPIO |
| Raspberry Pi 4 | ROS2 Humble | C++ | ament_cmake, rclcpp, micro-ROS or serial bridge |
| Dashboard | Node-RED | JavaScript | Mosquitto broker, dashboard 2.0 nodes |

---

## CODE GENERATION RULES

When generating code for this project, always follow these rules:

1. **ESP-IDF (C++)**: Use FreeRTOS tasks for concurrent subsystems. Use `ESP_LOGI/LOGW/LOGE` for all logging. Never use Arduino-style `delay()`.
2. **STM32 (C)**: Use HAL drivers only. Configure peripherals in CubeMX. Use DMA for UART. Use TIM PWM for motor control.
3. **ROS2 (C++)**: Use rclcpp. Follow ROS2 naming conventions. Use lifecycle nodes where appropriate.
4. **UART Protocol**: Always use the shared frame format defined above. Include checksum validation on receive.
5. **Safety**: Always implement an emergency stop that cuts all motor drivers instantly.
6. **Modularity**: Each subsystem (locomotion, joints, sensors, comms) must be a separate file/module.
7. **Debugging**: Every function must have entry/exit log statements at DEBUG level.
8. **No magic numbers**: Use named constants / enums for all command types, device IDs, and error codes.

---

## SUBSYSTEM TASK BREAKDOWN (ESP32 FreeRTOS)

```
Task: wifi_manager_task      — handles AP/STA mode, reconnection
Task: mqtt_client_task       — MQTT connect, subscribe, publish queue
Task: uart_tx_task           — sends frames to STM32
Task: uart_rx_task           — receives frames from STM32
Task: ros_bridge_task        — UART bridge to RPi ROS2
Task: suit_rx_task           — receives suit data from Suit ESP32
Task: sensor_publish_task    — publishes IMU, GPS, ultrasonic to MQTT
Task: heartbeat_task         — publishes heartbeat every 500ms
Task: debug_task             — routes logs to Serial + MQTT
```

---

## CURRENT DEVELOPMENT PRIORITY

1. WiFi provisioning system (AP → Web Portal → STA) on Robot ESP32
2. UART frame protocol implementation (ESP32 ↔ STM32)
3. Motor control firmware on STM32 (locomotion + joints)
4. MQTT integration + Node-RED dashboard
5. Suit ESP32 firmware (button + pot acquisition)
6. ROS2 package on RPi + serial bridge
7. Full system integration + debugging layer
