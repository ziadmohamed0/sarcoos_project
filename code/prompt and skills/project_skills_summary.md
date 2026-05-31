# Project Technical & Skills Summary: SARCUS Robot

## 1. System Architecture — Decoupled Multi-Node Design

Two **independent ESP32 nodes**, zero hardware/software overlap. Communicate only via MQTT through separate Mosquitto brokers.

| Node | Project Directory | Role | Hardware |
|------|-------------------|------|----------|
| **A** | `D:\Ziad\sarcoos_project\code\esp32_sensor` | Data Acquisition & DSP | MPU6050 IMU, NEO-7M GPS, 4× HC-SR04 Ultrasonic |
| **B** | `D:\Ziad\sarcoos_project\code\esp32_control` | High-Priority Actuator Control | BTS7960×2, Feetech servos×4, TB6600+NEMA23×2 |

**Inviolable rule:** No motor/actuator logic in Node A. No sensor loops in Node B.

### MQTT Brokers
- **Sensor broker:** `192.168.1.4:1883` — Node A publishes sensor data
- **Actuator broker:** `192.168.1.5:1883` — Node B subscribes to commands

---

## 2. Technical Skills & Stack

- **Microcontroller:** Espressif ESP32 (Dual-core Xtensa LX6)
- **Framework:** ESP-IDF v6.0.1
- **Language:** Modern C++17, OOP, no RTTI, no exceptions
- **RTOS:** FreeRTOS (ESP-IDF native fork)
- **MQTT Client:** `esp-mqtt` (IDF built-in or managed `espressif__mqtt`)
- **MQTT Broker:** Mosquitto (separate instances per node)
- **Dashboard:** Node-RED + Dashboard 2.0
- **Toolchain:** Windows Terminal + ESP-IDF CMD
- **IDE:** VS Code / Cursor
- **AI Pipeline:** Local AI-assisted embedded development and firmware auditing

---

## 3. Node B — `esp32_control` (Actuator Board)

### Project Structure
```
D:\Ziad\sarcoos_project\code\esp32_control/
├── CMakeLists.txt              # project: esp32_driver, partition: partitions.csv
├── partitions.csv               # 2 MB factory app, no OTA
├── main/
│   ├── CMakeLists.txt           # 2 SRCS, 13 REQUIRES
│   ├── Kconfig.projbuild        # WiFi, MQTT, Motor, Servo, Stepper, Motion menus
│   ├── inc/
│   │   ├── main.h               # Kconfig-aware defines with fallbacks
│   │   ├── json_parser.h        # Header-only JsonParser class
│   │   └── ProvisioningServer.h # ProvisioningServer + ProvisioningResult
│   └── src/
│       ├── main.cpp             # app_main, MQTT handlers, motion control task
│       └── ProvisioningServer.cpp # Captive portal: HTTP + DNS raw sockets
├── drivers/
│   ├── CMakeLists.txt           # 9 SRCS, 11 INCLUDE_DIRS, 14 REQUIRES
│   ├── inc/
│   │   ├── common.h             # Aggregate include for all IDF headers + ESP32 drivers
│   │   ├── hal/
│   │   │   ├── bts7960/         # BTS7960.h, BTS7960_position.h
│   │   │   ├── dc_motor/        # dc_motor.h
│   │   │   ├── pot/             # pot.h (ADC multisampling)
│   │   │   ├── pid/             # pid.h
│   │   │   ├── servo/           # Servo.h (270° range, LEDC)
│   │   │   └── stepper/         # stepper.h (task-based, queue-driven)
│   │   └── mcal/
│   │       ├── wifi/            # WiFiManager.h
│   │       ├── mqtt/            # MQTTClient.h
│   │       └── nvs/             # NVS.h (templated key-value)
│   └── src/
│       ├── hal/
│       │   ├── dc_motor/
│       │   ├── bts7960/         # bts7960.cpp, bts7960_position.cpp, bts7960_pot_position.cpp
│       │   ├── servo/           # servo.cpp
│       │   └── stepper/         # stepper.cpp
│       └── mcal/
│           ├── wifi/            # WiFi.cpp
│           ├── mqtt/            # MQTT.cpp
│           └── nvs/             # NVS.cpp
└── managed_components/
    └── espressif__mqtt/         # IDF component manager ESP-MQTT
```

### Pin Wiring (Node B)

| GPIO | Function | Driver |
|------|----------|--------|
| 32 | Motor1 R_PWM (Right Wheel FWD) | BTS7960 #1, LEDC_CH0, TIMER0 |
| 33 | Motor1 L_PWM (Right Wheel BWD) | BTS7960 #1, LEDC_CH1, TIMER0 |
| 19 | Motor1 R_EN | BTS7960 #1, GPIO out |
| 18 | Motor1 L_EN | BTS7960 #1, GPIO out |
| 26 | Motor2 R_PWM (Left Wheel FWD) | BTS7960 #2, LEDC_CH4, TIMER1 |
| 25 | Motor2 L_PWM (Left Wheel BWD) | BTS7960 #2, LEDC_CH5, TIMER1 |
| 17 | Motor2 R_EN | BTS7960 #2, GPIO out |
| 16 | Motor2 L_EN | BTS7960 #2, GPIO out |
| 13 | Shoulder Left Servo (95 kg·cm) | LEDC_CH2, TIMER2, 180-type |
| 14 | Shoulder Right Servo (95 kg·cm) | LEDC_CH3, TIMER2, 180-type |
| 27 | Elbow Left Servo (35 kg·cm) | LEDC_CH6, TIMER3, 180-type |
| 23 | Elbow Right Servo (35 kg·cm) | LEDC_CH7, TIMER3, 180-type |
| 4 | Left Wrist STEP | Stepper, GPIO out |
| 21 | Left Wrist DIR | Stepper, GPIO out |
| 22 | Left Wrist ENABLE | Stepper, active-low |
| 15 | Right Wrist STEP | Stepper, GPIO out |
| 2 | Right Wrist DIR | Stepper, GPIO out |
| 5 | Right Wrist ENABLE | Stepper, active-low |

### Kconfig Defaults
- **WiFi:** SSID `"WE_BF1790"`, password `"n7j05024"`
- **MQTT:** URI `"mqtt://192.168.1.5"`, port 1883
- **Motor pins:** 32/33/19/18 and 26/25/17/16
- **Servo pins:** 13/14/27/23
- **Stepper pins:** 4/21/22 and 15/2/5, STEPS_PER_REV=200
- **Motion control:** period 50 ms, motor slew 6.0, servo slew 4.0, filter alpha 0.7, default angle 135°

### Key Classes / APIs

#### `Stepper` (task-based, queue-driven)
- Constructor: `Stepper(step_pin, dir_pin, en_pin)`
- `init()`: config pins, creates queue (size 1) + FreeRTOS task (stack 2048, prio 6)
- `move(steps, speed_rpm)`: signed steps (positive=CW, negative=CCW). `xQueueOverwrite` for interrupt.
- `stop()`: aborts current move. `is_running()`, `getRemainingSteps()`, `getTotalSteps()`
- RPM → half-period: `30,000,000 / (rpm × STEPS_PER_REV)` µs, min 3 µs
- Hybrid delay: `vTaskDelay` if half-period > 2 ms, else `esp_rom_delay_us`

#### `ProvisioningServer`
- AP: `SARCUS_SETUP` / `sarcus2024`, WPA2_PSK, max 4 clients
- DNS spoof (UDP 53): all domains → `192.168.4.1`
- HTTP portal (TCP 80): HTML form → POST `/wifi` → saves to NVS `"sarcus"` namespace
- AP+STA dual mode: JS polls `/status` every 2s for IP display
- 30s STA timeout, falls back to AP on failure
- `ProvisioningResult { success, wifi_ssid, wifi_pass, mqtt_broker, sta_ip }`

#### `JsonParser` (header-only, zero external deps)
- `parse(json_str)` → builds linked list of `{ key, value_int, is_number, next }`
- `getInt("key", default)` → returns int or default
- Handles: numbers (+/-), strings (skipped), booleans (true/false, skipped), null (skipped)
- No nested objects or arrays

#### `BTS7960`
- `forward(speed)`, `backward(speed)`, `stop()`, `enable()`, `disable()`
- PWM via LEDC channels/timers

#### `Servo`
- `setPulse(pulse_us)`: 500-2500 µs, 50 Hz via LEDC
- Angle mapping: `pulse = 500 + (angle × 2000 / 270)` for 270° servos

#### `WiFiManager`
- `init_sta(ssid, pass, retry)`, `connect()`, `is_connected()`, `on_connected(cb)`, `on_disconnected(cb)`
- `startAP()`, `waitForIP(timeout)`

#### `MQTTClient`
- `init(MQTTConfig)`, `connect()`, `is_connected()`
- `subscribe(topic, qos)`, `publish(topic, payload, qos)`
- `on_topic(topic, cb)`, `on_connected(cb)`, `on_message(cb)`

### Motion Control

#### `motion_control_task` (period 50 ms, priority 5, stack 4096)
- Reads `MotionTargets` under mutex
- Smooths all servos via `smooth_angle()` (slew + low-pass with alpha=0.7)
- Direction change while speed > 1: slews to 0 first, then switches dir, then ramps

#### Direction Map
| dir | Action |
|-----|--------|
| 0 | STOP |
| 1 | FORWARD (both motors) |
| 2 | BACKWARD (both motors) |
| 3 | LEFT (motor1 BWD + motor2 FWD) |
| 4 | RIGHT (motor1 FWD + motor2 BWD) |

### MQTT Handlers
- `handle_movement_command` → `{"dir":0-4, "speed":0-100}`
- `handle_shoulder_command` → `{"side":0|1, "servo":angle}`
- `handle_elbow_command` → `{"side":0|1, "angle":deg}`
- `handle_wrist_command` → `{"side":0|1, "steps":N, "speed":60, "dir":0|1}`
  - dir=0 → CW (positive steps), dir=1 → CCW (negative steps)
- `handle_estop` → sets emergency_stop=true, disables all outputs

### Boot Sequence
1. `nvs_flash_init()` — erase+retry
2. Read NVS `"sarcus"`: wifi_ssid, wifi_pass, mqtt_uri
3. No saved SSID → `ProvisioningServer::run()` (captive portal)
4. Create mutex, init BTS7960×2 + Servo×4 + Stepper×2
5. `xTaskCreate(motion_control_task, ...)`
6. WiFi STA connect (blocking, 5 retries)
7. MQTT connect (client_id = `"ESP32_SARCUS_" + rand()%10000`, keepalive=60)
   - `on_connected` → subscribe to 5 topics (QoS 1)
8. Main loop: wrist status JSON every 1000ms on `sarcus/robot/joints/wrist`

### Known Issues (Node B)
- **Blocking startup:** `while(!wifi->is_connected())` and `while(!mqtt->is_connected())` spin before scheduler
- **No stepper limit switches:** no homing or endstop detection for wrist
- **No OTA partition:** only 2 MB factory
- **Dead MPU6050 files** at `drivers/inc/hal/mpu6050/` and `drivers/src/hal/mpu6050/` — not compiled
- **Build fixes applied:**
  - `common.h` include order: `nvs_flash.h` before custom `nvs.h`
  - `mqtt_broker` macro → `DEFAULT_MQTT_URI` to avoid collision
  - Missing `endmenu` in `Kconfig.projbuild` Stepper section
- **Binary size:** 0xe4c80 bytes (~938 KB), 55% free on 2 MB app partition

---

## 4. Node A — `esp32_sensor` (Sensor Board)

### Project Structure
```
D:\Ziad\sarcoos_project\code\esp32_sensor/
├── CMakeLists.txt              # project: SARCUS_Robot, no custom partition
├── main/
│   ├── CMakeLists.txt           # 10 SRCS, 12 REQUIRES
│   ├── inc/
│   │   ├── SarcusTypes.h        # Enums, structs, MQTT topics, uart frame protocol
│   │   ├── SensorManager.h      # Singleton, DSP filter state, callback types
│   │   ├── MqttManager.h        # Singleton, publish/subscribe API
│   │   ├── WifiManager.h
│   │   ├── WebServer.h
│   │   ├── UartBridge.h
│   │   ├── MotorController.h
│   │   ├── NVSManager.h
│   │   ├── HeartbeatManager.h
│   │   └── DebugManager.h
│   └── src/
│       ├── main.cpp             # app_main, suit button mapping, MQTT→MotorController bridge
│       ├── SensorManager.cpp    # IMU/gps/us read + DSP filters + tasks
│       ├── MqttManager.cpp      # mqtt client, JSON helper, publish formatting
│       ├── WifiManager.cpp
│       ├── WebServer.cpp
│       ├── UartBridge.cpp
│       ├── MotorController.cpp
│       ├── NVSManager.cpp
│       ├── HeartbeatManager.cpp
│       └── DebugManager.cpp
└── extions/
    └── prompt.md
```

### Pin Wiring (Node A)

| GPIO | Function | Peripheral |
|------|----------|------------|
| 17 | UART1 TX → STM32 (legacy, 115200 baud) | UART_NUM_1 |
| 16 | UART1 RX ← STM32 (legacy) | UART_NUM_1 |
| 18 | GPS RX ← NEO-7M TX | UART_NUM_2, 9600 baud |
| 19 | GPS TX → NEO-7M RX | UART_NUM_2 |
| 21 | I2C SDA → MPU6050 | I2C_NUM_0, 400 kHz |
| 22 | I2C SCL → MPU6050 | I2C_NUM_0 |
| 25 | HC-SR04 Front TRIG | GPIO out, 10 µs pulse |
| 26 | HC-SR04 Front ECHO | GPIO in, ~25 ms timeout |
| 27 | HC-SR04 Back TRIG | GPIO out |
| 14 | HC-SR04 Back ECHO | GPIO in |
| 12 | HC-SR04 Left TRIG | GPIO out |
| 13 | HC-SR04 Left ECHO | GPIO in |
| 32 | HC-SR04 Right TRIG | GPIO out |
| 33 | HC-SR04 Right ECHO | GPIO in |

### Architecture: Singleton Manager Pattern

No HAL/MCAL layer. All sensor drivers are inline in `SensorManager.cpp`.

### Key Code Architecture (`SarcusTypes.h`)

**Enums:**
```cpp
enum class DeviceID : uint8_t {
    ROBOT_ESP32 = 0x01, STM32 = 0x02, RPI = 0x03, SUIT_ESP32 = 0x04
};
enum class CmdType : uint8_t {
    CMD_MOVE = 0x10, CMD_STOP_ALL = 0x11,
    CMD_SHOULDER = 0x20, CMD_ELBOW = 0x21, CMD_WRIST = 0x22,
    DATA_IMU = 0x30, DATA_GPS = 0x31, DATA_ULTRASONIC = 0x32,
    CMD_HEARTBEAT = 0x40, CMD_DEBUG_LOG = 0x41,
    CMD_STATUS_REQ = 0x42, CMD_STATUS_RESP = 0x43
};
enum class ErrorCode : uint8_t {
    OK = 0x00, UART_TIMEOUT = 0x01, MOTOR_FAULT = 0x02,
    IMU_FAIL = 0x03, GPS_NO_FIX = 0x04, WIFI_DISCONNECT = 0x05,
    MQTT_DISCONNECT = 0x06, CHECKSUM_ERROR = 0x07, INVALID_FRAME = 0x08
};
```

**Data structs:**
```cpp
struct MoveCmd     { MoveDir direction; uint8_t speed_percent; };
struct ShoulderCmd { int16_t dc_angle_deg; int16_t servo_angle_deg; uint8_t side; };
struct ElbowCmd    { int16_t servo_angle_deg; uint8_t side; };
struct WristCmd    { int32_t steps; uint16_t speed_rpm; uint8_t direction; uint8_t side; };
struct ImuData     { float ax,ay,az,gx,gy,gz,roll,pitch,yaw; bool valid; };
struct GpsData     { double lat,lon; float alt; uint8_t sats; bool fix; };
struct UltrasonicData { float front_cm,back_cm,left_cm,right_cm; };
struct SuitFrame   { uint32_t button_mask; uint16_t pot_values[20]; bool valid; };
struct UartFrame   { DeviceID src; CmdType cmd; uint8_t len; uint8_t data[64]; uint8_t csum; bool valid; };
```

### Sensor Tasks

| Task function | Name | Stack | Priority | Period | Reads |
|---------------|------|-------|----------|--------|-------|
| `imuTask` | `"imu_task"` | 3072 | IDLE+5 | 100 ms (10 Hz) | MPU6050 → publish IMU |
| `gpsTask` | `"gps_task"` | 3072 | IDLE+4 | 1000 ms (1 Hz) | UART2 NMEA → publish GPS |
| `ultrasonicTask` | `"us_task"` | 2048 | IDLE+4 | 200 ms (5 Hz) | 4× HC-SR04 → publish US |

All tasks publish via `MqttManager::getInstance().publish*(...)`.

### DSP Filters: MPU6050 IMU

#### Gyro Bias Calibration (`calibrateGyro()`)
- 200 stationary samples (`IMU_GYRO_CALIB_SAMPLES = 200`)
- Raw → deg/s: `raw / 131.0f`
- Averages to `m_gx_bias`, `m_gy_bias`, `m_gz_bias`
- 5 ms `vTaskDelay` between samples

#### Accelerometer EMA Low-Pass Pre-Filter
- Coefficient: `IMU_ACCEL_LP_ALPHA = 0.15`
- `m_ax_filt = ALPHA × raw + (1 - ALPHA) × m_ax_filt`
- Applied to ax, ay, az before trig angle computation
- Purpose: reject motor vibration / sensor spike noise

#### Complementary Filter (Roll/Pitch)
- Coefficient: `IMU_CF_ALPHA = 0.98`
- Accel-based roll: `atan2f(ay_filt, az_filt) × 180 / M_PI`
- Accel-based pitch: `atan2f(-ax_filt, sqrt(ay_filt² + az_filt²)) × 180 / M_PI`
- `roll = CF_ALPHA × (roll + gx_raw × dt) + (1 - CF_ALPHA) × roll_acc`
- `pitch = CF_ALPHA × (pitch + gy_raw × dt) + (1 - CF_ALPHA) × pitch_acc`
- `yaw += gz_raw × dt` — pure integration, drifts long-term
- DT from `esp_timer_get_time()`, clamped ≤ 0.5 s

#### MPU6050 Hardware Constants
- Address: `0x68`, I2C timeout: 1000 ms
- Registers: PWR_MGMT_1=0x6B (write 0x00 to wake), ACCEL_XOUT_H=0x3B (14 bytes)
- Accel scale: `raw / 16384.0f` (g), Gyro scale: `raw / 131.0f` (deg/s)

### DSP Filters: GPS NEO-7M

#### NMEA-0183 Parser (`parseNMEA`)
- Sentences: `$GPRMC` / `$GNRMC`
- 12-field comma tokenization
- Converts `DDMM.MMMM` → decimal degrees
- `satellites` hardcoded to 4 (RMC lacks satellite count)

#### Moving Median Filter (Coordinate Spike Rejection)
- Window: `GPS_MEDIAN_WINDOW = 5`
- Ring buffers: `m_gps_lat_buf[5]`, `m_gps_lon_buf[5]`, `m_gps_hist_idx`
- `applyMedianFilter(val, buffer)`: copies to temp array, bubble-sort, returns middle element
- Purpose: eliminates single-sample coordinate jumps from atmospheric/multipath noise

### Ultrasonic (`measureDistanceCm`)
- 10 µs trigger pulse, echo timeout 25 ms (~430 cm max)
- Distance = `duration_us × 0.0343f / 2.0f` (cm)
- Returns `-1.0f` on timeout
- 4 sensors sequential, 10 ms gap between each

### MQTT Publish Payloads

**IMU (10 Hz, topic `sarcus/sensors/imu`):**
```json
{"roll":%.2f,"pitch":%.2f,"yaw":%.2f,"ax":%.3f,"ay":%.3f,"az":%.3f,"gx":%.2f,"gy":%.2f,"gz":%.2f,"ts":%lu}
```

**GPS (1 Hz, topic `sarcus/sensors/gps`):**
```json
// With fix:
{"fix":true,"lat":%.7f,"lon":%.7f,"alt":%.1f,"sats":%u,"ts":%lu}
// No fix:
{"fix":false,"ts":%lu}
```

**Ultrasonic (5 Hz, topic `sarcus/sensors/ultrasonic`):**
```json
{"front":%.1f,"back":%.1f,"left":%.1f,"right":%.1f,"ts":%lu}
```

**Heartbeat (2 Hz, topic `sarcus/debug/heartbeat`):**
```json
{"device":"%s","uptime":%lu,"ts":%lu}
```

### MQTT Subscription Topics (all QoS 1)

| Topic | Direction | Handler |
|-------|-----------|---------|
| `sarcus/robot/movement` | Subscribe | `motor.submitMove()` |
| `sarcus/robot/joints/shoulder` | Subscribe | `motor.submitShoulder()` |
| `sarcus/robot/joints/elbow` | Subscribe | `motor.submitElbow()` |
| `sarcus/robot/joints/wrist` | Subscribe | `motor.submitWrist()` |
| `sarcus/suit/buttons` | Subscribe | Button bitmask → MoveCmd |
| `sarcus/suit/pots` | Subscribe | Array of 20 values |
| `sarcus/robot/estop` | Subscribe | Emergency stop |

### Suit Button → MoveCmd Mapping
- Bit 0: FWD, Bit 1: BWD, Bit 2: LEFT, Bit 3: RIGHT, Bit 4: STOP, Bit 5: E-STOP
- Default speed: 60%
- Pot 0 → shoulder_left_dc: `(pots[0]/4095.0)*360 - 180`
- Pot 1 → shoulder_left_servo: `(pots[1]/4095.0)*270`
- Pot 4 → elbow_left_servo: `(pots[4]/4095.0)*270`

### Boot Sequence
1. `NVSManager::getInstance().init()`
2. `readDeviceId()` — MAC address as `XX:XX:XX:XX:XX:XX`
3. `WifiManager::init()` + `connectWifi()`
   - No NVS creds: AP mode (`SARCUS_SETUP` / `sarcus2024`) + WebServer → POST /wifi → STA+AP dual → 30s wait
   - Has creds: STA mode → 15s timeout → fallback AP
4. MQTT → register callbacks → MotorController
5. `MotorController::init(STM32_UART_PORT)` + `UartBridge::init(...)`
6. `SensorManager::init(I2C_NUM_0,21,22, UART_NUM_2,18,19, 25,26,27,14,12,13,32,33)` + `start(100,1000,200)`
7. `DebugManager::init()` + `HeartbeatManager::start(device_id)`
8. Main loop: heartbeat tick every 5000ms

### Known Issues (Node A)
- **Legacy I2C driver:** `driver/i2c.h` is EOL in ESP-IDF v6.0 — plan migration to `driver/i2c_master.h`
- **GPS satellite count hardcoded** to 4 — need `$GPGGA` parser for real count
- **No IMU temperature** in MQTT payload (temp registers read but discarded)
- **Ultrasonic readings unfiltered** — consider EMA smoothing on distance values
- **No i2c device probe** before gyro calibration — ~1 s timeout if MPU6050 absent

---

## 5. Key Decisions & History

| Decision | Rationale |
|----------|-----------|
| **Two-node architecture** | Sensor filtering cannot miss deadlines due to motor pulse generation; clean separation of concerns |
| **Dual MQTT brokers** | Separate `192.168.1.4` (sensor) and `192.168.1.5` (control) — prevents topic cross-contamination, isolates failure domains |
| **cJSON replaced with `JsonParser`** | cJSON not available as bundled component in ESP-IDF v6.0.1. Custom header-only parser is zero-dependency, sufficient for flat integer JSON |
| **Provisioning over raw sockets** | Avoids `WiFiManager` library re-init conflicts; AP stays alive during STA connection for polling |
| **Stepper via dedicated FreeRTOS task** | Pulse generation is time-critical; decoupling from 20 Hz motion control loop prevents jitter |
| **Complementary filter (IMU)** | Standard embedded approach: gyro handles dynamics, accel corrects drift. CF_ALPHA=0.98 gives good response without noticeable lag |
| **GPS median filter** | NMEA positions can have single-sample outliers; ring buffer + median is lightweight and effective |
| **Servo as 180-type in code** | Feetech 270° servos work with 180-type initialization; angle mapping still covers 0-270° |

## 6. Next Steps

### Immediate (Build & Flash)
1. Flash Node B to hardware — find correct USB-to-UART port (not COM1)
2. Flash Node A to hardware — fix ESP-IDF Python venv
3. Verify stepper wrist control via Node-RED
4. Verify IMU complementary filter + GPS median filter output on MQTT

### Short-term
5. Fix GPS satellite count — add `$GPGGA` parser to `parseNMEA()` in Node A
6. Migrate I2C driver from `driver/i2c.h` → `driver/i2c_master.h` in Node A
7. Add ultrasonic EMA smoothing in Node A
8. Add i2c device probe before gyro calibration in Node A

### Medium-term
9. Add stepper limit switches / homing in Node B
10. Implement OTA partition scheme (2 MB factory + 2 MB OTA)
11. Fix blocking startup — use event-driven initialization in Node B
12. Add ROS2 MQTT bridge for Phase 2 integration

## 7. MQTT Topic Summary

| Topic | Pub/Sub | Node | Payload |
|-------|---------|------|---------|
| `sarcus/robot/movement` | Sub | B | `{"dir":0-4,"speed":0-100}` |
| `sarcus/robot/estop` | Sub | B | any (empty payload OK) |
| `sarcus/robot/joints/shoulder` | Sub | B | `{"side":0|1,"servo":angle}` |
| `sarcus/robot/joints/elbow` | Sub | B | `{"side":0|1,"angle":deg}` |
| `sarcus/robot/joints/wrist` | Sub+Pub | B | Cmd: `{"side":0|1,"steps":N,"speed":60,"dir":0|1}`; Status: `{"side":N,"remaining":N,"total":N}` |
| `sarcus/sensors/imu` | Pub | A | `{"roll","pitch","yaw","ax","ay","az","gx","gy","gz","ts"}` |
| `sarcus/sensors/gps` | Pub | A | `{"fix","lat","lon","alt","sats","ts"}` |
| `sarcus/sensors/ultrasonic` | Pub | A | `{"front","back","left","right","ts"}` |
| `sarcus/debug/heartbeat` | Pub | A | `{"device","uptime","ts"}` |
| `sarcus/debug/logs` | Pub | A | Raw log string |
| `sarcus/suit/buttons` | Sub | A | uint32 bitmask |
| `sarcus/suit/pots` | Sub | A | JSON array [20 values] |
