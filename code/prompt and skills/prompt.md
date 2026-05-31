# SARCUS Robot — Master System Prompt

## PROJECT OVERVIEW

- **Name:** SARCUS Robot — Assistive Humanoid / Wheeled Exoskeleton
- **Goal:** Enable patients to control a robotic exoskeleton via Wearable Suit (ESP32), Node-RED MQTT Dashboard, and ROS2
- **Architecture:** Two independent ESP32 nodes, zero hardware/software overlap
  - **Node A** (`esp32_sensor`) — Data acquisition & DSP only
  - **Node B** (`esp32_control`) — Actuator control only
- **Stack:** ESP-IDF v6.0.1, C++17, FreeRTOS, no Arduino, no STM32, no RTTI, no exceptions
- **MQTT:** Mosquitto brokers — two separate instances
  - Sensor broker: `192.168.1.4:1883` (Node A publishes)
  - Actuator broker: `192.168.1.5:1883` (Node B subscribes)
- **Firmware locations:**
  - `D:\Ziad\sarcoos_project\code\esp32_control` — Node B
  - `D:\Ziad\sarcoos_project\code\esp32_sensor` — Node A
- **Current phase:** All code functional on two independent ESP32 boards. No STM32 in loop.

---

## NODE B: `esp32_control` — ACTUATOR BOARD

**Path:** `D:\Ziad\sarcoos_project\code\esp32_control`

### Pin Wiring

| GPIO | Function | Driver/Peripheral |
|------|----------|-------------------|
| **BTS7960 H-Bridge × 2 (Locomotion)** | | |
| 32 | Motor1 R_PWM (Right Wheel FWD) | LEDC_CH0, TIMER0 |
| 33 | Motor1 L_PWM (Right Wheel BWD) | LEDC_CH1, TIMER0 |
| 19 | Motor1 R_EN | GPIO out |
| 18 | Motor1 L_EN | GPIO out |
| 26 | Motor2 R_PWM (Left Wheel FWD) | LEDC_CH4, TIMER1 |
| 25 | Motor2 L_PWM (Left Wheel BWD) | LEDC_CH5, TIMER1 |
| 17 | Motor2 R_EN | GPIO out |
| 16 | Motor2 L_EN | GPIO out |
| **Servo × 4 (Shoulder/Elbow, Feetech, 50 Hz, 270°)** | | |
| 13 | Shoulder Left (95 kg·cm) | LEDC_CH2, TIMER2 |
| 14 | Shoulder Right (95 kg·cm) | LEDC_CH3, TIMER2 |
| 27 | Elbow Left (35 kg·cm) | LEDC_CH6, TIMER3 |
| 23 | Elbow Right (35 kg·cm) | LEDC_CH7, TIMER3 |
| **Stepper × 2 (Wrist, NEMA23 + TB6600/DM542)** | | |
| 4 | Left Wrist STEP | GPIO out |
| 21 | Left Wrist DIR | GPIO out |
| 22 | Left Wrist ENABLE | GPIO out, active-low |
| 15 | Right Wrist STEP | GPIO out |
| 2 | Right Wrist DIR | GPIO out |
| 5 | Right Wrist ENABLE | GPIO out, active-low |

**Free GPIOs (available):** 0, 12 (strapping — verify boot), 34–39 (input only)

### Build System

- **Project name:** `esp32_driver`
- **CMake minimum:** 3.16
- **Partition table:** Custom `partitions.csv` (2 MB factory, no OTA)
- **Extra component dirs:** `drivers`, `managed_components/espressif__mqtt`
- **Warnings suppressed:** `-w`

#### `main/CMakeLists.txt` (13 required components)
```
SRCS: main.cpp, ProvisioningServer.cpp
INCLUDE_DIRS: inc
REQUIRES: drivers, esp_wifi, esp_event, nvs_flash, esp_http_server, esp_netif, driver,
          esp_driver_gpio, esp_driver_uart, mqtt, freertos, esp_timer, log
```

#### `drivers/CMakeLists.txt` (9 sources, 11 include dirs, 14 reqs)
```
SRCS:
  src/hal/dc_motor/dc_motor.cpp
  src/hal/bts7960/bts7960.cpp
  src/hal/bts7960/bts7960_position.cpp
  src/hal/bts7960/bts7960_pot_position.cpp
  src/hal/servo/servo.cpp
  src/hal/stepper/stepper.cpp
  src/mcal/wifi/WiFi.cpp
  src/mcal/mqtt/MQTT.cpp
  src/mcal/nvs/NVS.cpp
```

### Kconfig Menuconfig (`main/Kconfig.projbuild`)

```
SARCUS Robot Configuration
├── WiFi Configuration
│   ├── SARCUS_WIFI_SSID       (string, default "WE_BF1790")
│   └── SARCUS_WIFI_PASSWORD   (string, default "n7j05024")
├── MQTT Configuration
│   ├── SARCUS_MQTT_BROKER_URI (string, default "mqtt://192.168.1.5")
│   └── SARCUS_MQTT_PORT       (int, default 1883, range 1-65535)
├── Motor PWM Pins
│   ├── SARCUS_MOTOR1_R_PWM  (int, default 32)
│   ├── SARCUS_MOTOR1_L_PWM  (int, default 33)
│   ├── SARCUS_MOTOR1_R_EN   (int, default 19)
│   ├── SARCUS_MOTOR1_L_EN   (int, default 18)
│   ├── SARCUS_MOTOR2_R_PWM  (int, default 26)
│   ├── SARCUS_MOTOR2_L_PWM  (int, default 25)
│   ├── SARCUS_MOTOR2_R_EN   (int, default 17)
│   └── SARCUS_MOTOR2_L_EN   (int, default 16)
├── Servo Pins
│   ├── SARCUS_SERVO_SHOULDER_LEFT   (int, default 13)
│   ├── SARCUS_SERVO_SHOULDER_RIGHT  (int, default 14)
│   ├── SARCUS_SERVO_ELBOW_LEFT      (int, default 27)
│   └── SARCUS_SERVO_ELBOW_RIGHT     (int, default 23)
├── Stepper Pins
│   ├── SARCUS_STEPPER_LEFT_STEP   (int, default 4)
│   ├── SARCUS_STEPPER_LEFT_DIR    (int, default 21)
│   ├── SARCUS_STEPPER_LEFT_EN     (int, default 22)
│   ├── SARCUS_STEPPER_RIGHT_STEP  (int, default 15)
│   ├── SARCUS_STEPPER_RIGHT_DIR   (int, default 2)
│   ├── SARCUS_STEPPER_RIGHT_EN    (int, default 5)
│   └── SARCUS_STEPPER_STEPS_PER_REV (int, default 200)
└── Motion Control
    ├── SARCUS_CTRL_PERIOD_MS      (int, default 50, range 10-500)
    ├── SARCUS_MOTOR_SLEW_STEP     (float, default 6.0)
    ├── SARCUS_SERVO_SLEW_STEP     (float, default 4.0)
    ├── SARCUS_SERVO_FILTER_ALPHA  (float, default 0.7)
    └── SARCUS_SERVO_DEFAULT_ANGLE (float, default 135.0)
```

### Software Architecture

#### Driver Layer (HAL + MCAL)

All in `drivers/`:

**BTS7960** (`drivers/inc/hal/bts7960/BTS7960.h`)
```cpp
BTS7960(r_pwm, l_pwm, r_en, l_en, ch_fwd, ch_bwd, timer);
void enable();
void disable();
void forward(int speed_pct);   // 0-100
void backward(int speed_pct);
void stop();
```

**Servo** (`drivers/inc/hal/servo/Servo.h`)
```cpp
enum class SERVO_TYPE { STANDARD_180, STANDARD_270 };
Servo(pin, type, ch, timer);
void setPulse(uint32_t pulse_us);  // 500-2500µs
```
Angle → pulse: `500 + (angle_deg × 2000 / 270)` for 270° type, `500 + (angle_deg × 2000 / 180)` for 180°.

**Stepper** (`drivers/inc/hal/stepper/stepper.h`)
```cpp
Stepper(step_pin, dir_pin, en_pin = GPIO_NUM_NC);
void init();              // config pins, create queue + task
void enable(bool on);     // active-low EN
void move(int steps, float speed_rpm);  // signed steps for direction
void stop();              // abort in-progress move
bool is_running() const;
int  getRemainingSteps() const;
int  getTotalSteps() const;
```
- Dedicated FreeRTOS task per axis, `xQueueOverwrite` single-slot command queue
- RPM → half-period: `30,000,000 / (rpm × STEPS_PER_REV)` µs (minimum 3 µs)
- Hybrid delay: `vTaskDelay` for half-period > 2 ms, `esp_rom_delay_us` for faster
- Inter-move abort via new command overwriting queue slot

**WiFiManager** (`drivers/inc/mcal/wifi/WiFi.h`)
```cpp
void init_sta(ssid, pass, retry);
void connect();
bool is_connected();
void on_connected(cb);
void on_disconnected(cb);
void startAP();  // SSID: SARCUS_SETUP
bool waitForIP(timeout);
```

**MQTTClient** (`drivers/inc/mcal/mqtt/MQTT.h`)
```cpp
void init(MQTTConfig cfg);
void connect();
bool is_connected();
void subscribe(topic, qos);
void publish(topic, payload, qos=0);
void on_topic(topic, cb);
void on_connected(cb);
void on_message(cb);
```

#### Globals (in `main.cpp`)

```cpp
BTS7960*    g_motor1, *g_motor2;
Servo*      g_shoulder_servo_left, *g_shoulder_servo_right;
Servo*      g_elbow_servo_left, *g_elbow_servo_right;
Stepper*    g_wrist_left, *g_wrist_right;
WiFiManager* wifi;
MQTTClient*  mqtt;
static SemaphoreHandle_t g_motion_mutex;
```

#### Structs

```cpp
struct MotionTargets {
    int dir, speed;
    float shoulder_left, shoulder_right;
    float elbow_left, elbow_right;
    bool emergency_stop;
};
// Defaults: dir=0, speed=0, shoulder/elbow=135.0f, emergency_stop=false

struct MotionSmoothState {
    int dir;
    float speed;
    float shoulder_left, shoulder_right;
    float elbow_left, elbow_right;
};
// Defaults: dir=0, speed=0.0f, shoulder/elbow=135.0f
```

#### Motion Control Task (`motion_ctrl`, stack 4096, priority 5, period 50 ms)
- Acquires mutex, reads `MotionTargets`
- E-stop: sets dir=0, speed=0, calls `apply_robot_motion(0,0)`
- Direction change while speed > 1: slews speed to 0 first, then changes dir, ramps to target
- All 4 servos smoothed via `smooth_angle()` each cycle (slew + low-pass)

#### Direction Mapping
| dir | Action |
|-----|--------|
| 0 | Both STOP |
| 1 | Both FORWARD |
| 2 | Both BACKWARD |
| 3 | Motor1 BWD + Motor2 FWD (rotate LEFT) |
| 4 | Motor1 FWD + Motor2 BWD (rotate RIGHT) |

#### Smoothing Functions
```cpp
static float clampf(float v, float lo, float hi);
static float slew_toward(float current, float target, float max_step);
static float smooth_angle(float current, float target);
  // slew_toward(..., SERVO_ANGLE_SLEW_STEP) then alpha filter SERVO_ANGLE_FILTER_ALPHA
static void apply_servo_angle_270(Servo* servo, float angle_deg);
  // clamp 0-270, pulse = 500 + (angle_deg × 2000 / 270)
```

#### MQTT Handlers

```cpp
handle_movement_command(topic, payload)
  // JSON: {"dir":0-4, "speed":0-100} → updates MotionTargets

handle_shoulder_command(topic, payload)
  // JSON: {"side":0|1, "servo":angle} → updates shoulder target

handle_elbow_command(topic, payload)
  // JSON: {"side":0|1, "angle":deg} → updates elbow target

handle_wrist_command(topic, payload)
  // JSON: {"side":0|1, "steps":N, "speed":60, "dir":0|1}
  //  dir=0 → steps positive (CW); dir=1 → steps negative (CCW)
  //  calls g_wrist_left/right->move(steps, speed)

handle_estop(topic, payload)
  // Sets emergency_stop = true, all outputs disabled
```

#### JSON Parsing (`main/inc/json_parser.h`)
- Header-only `JsonParser` class — zero external deps, replaces cJSON
- Parses: `{"key":123, "key2":456}` — integers only, no nested objects/arrays
- `JsonParser::getInt("key", default_val)` returns int or default

#### NVS Credential Storage
Namespace: `"sarcus"`, keys: `wifi_ssid`, `wifi_pass`, `mqtt_uri`
```cpp
static std::string read_nvs_str(const char* ns, const char* key, const std::string& fallback);
```

#### Provisioning (`ProvisioningServer` class, `main/src/ProvisioningServer.cpp`)

**Flow:**
1. `startAP()` — AP: `SARCUS_SETUP` / `sarcus2024`, WPA2_PSK, max 4 clients
2. `startWebServer()` — raw TCP socket on port 80
3. `startDNSServer()` — raw UDP socket on port 53, all domains → `192.168.4.1`
4. Block until user submits form via HTTP POST `/wifi`
5. `startWiFiSTA(ssid, pass)` — AP+STA dual mode
6. `waitForSTA(30000)` — polls `/status` endpoint; JS shows spinner → IP
7. On success: save creds to NVS, return `ProvisioningResult`
8. On failure: `esp_restart()`

**HTTP Routes:**
| Route | Method | Behavior |
|-------|--------|----------|
| `/` | GET | Serve captive portal HTML page (CSS + JS) |
| `/wifi` | POST | Parse form (ssid, password, broker), save to NVS |
| `/status` | GET | Poll STA status — returns `{"status":"connected","ip":"..."}` or `{"status":"connecting"}` |
| `*` | GET | 302 redirect to `http://192.168.4.1/` |

**HTML pages:** `#setupPage` (form with ssid/password/broker), `#waitPage` (spinner), `#successPage` (IP display). JS polls `/status` every 2s.

#### Boot Sequence (`app_main`)

```
1. nvs_flash_init() — erase+retry on failure
2. Read NVS "sarcus": saved_ssid, saved_pass, saved_broker
3. If saved_ssid empty → ProvisioningServer::run() → returns creds
4. Create g_motion_mutex (SemaphoreHandle_t)
5. Hardware init:
   ├── BTS7960 × 2 (motor1, motor2) — call enable()
   ├── Servo × 4  (shoulder L/R, elbow L/R)
   ├── Stepper × 2 (wrist L/R) — call init()
   └── Main loop after MQTT connect: wrist status every 1000ms
6. xTaskCreate(motion_control_task, "motion_ctrl", 4096, NULL, 5, NULL)
7. WiFi STA connect (5 retries, blocking wait)
8. MQTT connect (MQTTConfig: broker_uri from NVS, port 1883,
     client_id = "ESP32_SARCUS_" + rand()%10000, keepalive=60)
   ├── on_connected → subscribe_robot_topics()
   └── on_topic → registered for all 5 topics
9. Main while(1): every 1000ms → publish wrist status JSON
```

#### MQTT Subscription Topics (all QoS 1)
| Topic | Handler |
|-------|---------|
| `sarcus/robot/movement` | `handle_movement_command` |
| `sarcus/robot/estop` | `handle_estop` |
| `sarcus/robot/joints/shoulder` | `handle_shoulder_command` |
| `sarcus/robot/joints/elbow` | `handle_elbow_command` |
| `sarcus/robot/joints/wrist` | `handle_wrist_command` |

Legacy topics (defined but not handled in current code):
`arm_right/shoulder1`, `arm_right/shoulder2`, `arm_left/shoulder1`, `arm_left/shoulder2`, `arm_left/elbow`, `arm_left/wrist`, `move/forward`, `move/backward`, `move/right`, `move/left`, `gripper/close`, `gripper/open`.

#### Wrist Status Publication (idle loop, 1 Hz)
```json
{"side":0, "remaining":N, "total":N}
{"side":1, "remaining":N, "total":N}
```
Published on `sarcus/robot/joints/wrist` (same topic as command subscription — `handle_wrist_command` safely ignores it because `getInt("steps")` returns 0 for status messages).

#### Known Issues (Node B)
- **Blocking startup:** `while(!wifi->is_connected())` and `while(!mqtt->is_connected())` spin before scheduler
- **No stepper limit switches:** no homing or endstop detection for wrist axes
- **No OTA partition:** 2 MB factory only
- **Dead MPU6050 files** on disk (`drivers/inc/hal/mpu6050/`, `drivers/src/hal/mpu6050/`) — not compiled
- **Build fix history:**
  - `common.h` include ordering: `nvs_flash.h` moved before custom `nvs.h` (resolved `nvs_open_mode_t`/`nvs_handle_t` undeclared)
  - Macro collision: `mqtt_broker` → `DEFAULT_MQTT_URI` in `main.h` + `main.cpp`
  - Missing `endmenu` after Stepper Pins in `Kconfig.projbuild`
  - Binary size: 0xe4c80 bytes (55% free on 2 MB app partition)

---

## NODE A: `esp32_sensor` — SENSOR BOARD

**Path:** `D:\Ziad\sarcoos_project\code\esp32_sensor`

### Pin Wiring

| GPIO | Function | Peripheral |
|------|----------|------------|
| **UART1 — STM32F405 (legacy, not currently used)** | | |
| 17 | UART1 TX | 115200 baud |
| 16 | UART1 RX | 115200 baud |
| **UART2 — NEO-7M GPS** | | |
| 18 | GPS RX (from GPS TX) | 9600 baud, NMEA-0183 |
| 19 | GPS TX (to GPS RX) | 9600 baud |
| **I2C — MPU6050 IMU** | | |
| 21 | SDA | I2C_NUM_0, 400 kHz |
| 22 | SCL | I2C_NUM_0, 400 kHz |
| **GPIO — HC-SR04 Ultrasonic (4×)** | | |
| 25 | Front TRIG | 10 µs pulse |
| 26 | Front ECHO | input, ~25 ms timeout |
| 27 | Back TRIG | |
| 14 | Back ECHO | |
| 12 | Left TRIG | |
| 13 | Left ECHO | |
| 32 | Right TRIG | |
| 33 | Right ECHO | |

### Build System

- **Project name:** `SARCUS_Robot`
- **CMake minimum:** 3.22
- **No Kconfig.projbuild** (no menuconfig)
- **No managed components** (uses IDF built-in `mqtt`)
- **No custom partition table**

#### `main/CMakeLists.txt` (10 sources, 12 requirements)
```
SRCS:
  main.cpp, src/WifiManager.cpp, src/NVSManager.cpp, src/WebServer.cpp,
  src/MqttManager.cpp, src/UartBridge.cpp, src/MotorController.cpp,
  src/SensorManager.cpp, src/HeartbeatManager.cpp, src/DebugManager.cpp
INCLUDE_DIRS: ., inc
REQUIRES: esp_wifi, esp_event, nvs_flash, esp_http_server, esp_netif,
          driver, esp_driver_gpio, esp_driver_uart, mqtt, freertos,
          esp_timer, log
```

### Software Architecture

Uses **singleton manager pattern** — no separate HAL/MCAL layer. All sensor driver code is inline in `SensorManager.cpp`.

#### `SarcusTypes.h` (`main/inc/SarcusTypes.h`)

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

**Control structs:**
```cpp
enum class MoveDir : uint8_t { STOP=0, FORWARD=1, BACKWARD=2, LEFT=3, RIGHT=4 };
struct MoveCmd     { MoveDir direction; uint8_t speed_percent; };
struct ShoulderCmd { int16_t dc_angle_deg; int16_t servo_angle_deg; uint8_t side; };
struct ElbowCmd    { int16_t servo_angle_deg; uint8_t side; };
struct WristCmd    { int32_t steps; uint16_t speed_rpm; uint8_t direction; uint8_t side; };
```

**Sensor structs:**
```cpp
struct ImuData         { float accel_x,accel_y,accel_z; float gyro_x,gyro_y,gyro_z; float roll,pitch,yaw; bool valid; };
struct GpsData         { double latitude,longitude; float altitude; uint8_t satellites; bool fix; };
struct UltrasonicData  { float front_cm, back_cm, left_cm, right_cm; };
struct SuitFrame       { uint32_t button_mask; uint16_t pot_values[20]; bool valid; };
struct UartFrame       { DeviceID src_device; CmdType cmd_type; uint8_t data_len; uint8_t data[64]; uint8_t checksum; bool valid; };
```

#### `SensorManager` (singleton)

**`init()` signature:** 15 parameters
```cpp
void SensorManager::init(
    i2c_port_t  i2c_port, int sda_pin, int scl_pin,       // MPU6050
    uart_port_t gps_uart, int gps_rx,  int gps_tx,          // NEO-7M GPS
    int trig_front, int echo_front,                         // ultrasonic front
    int trig_back,  int echo_back,                          // ultrasonic back
    int trig_left,  int echo_left,                          // ultrasonic left
    int trig_right, int echo_right)                         // ultrasonic right
```

**`start()` signature:**
```cpp
void start(uint32_t imu_interval_ms = 100,
           uint32_t gps_interval_ms = 1000,
           uint32_t ultrasonic_interval_ms = 200);
```

**Task Architecture:**

| Task | Name | Stack | Priority | Period |
|------|------|-------|----------|--------|
| `imuTask` | `"imu_task"` | 3072 | IDLE+5 | 100 ms (10 Hz) |
| `gpsTask` | `"gps_task"` | 3072 | IDLE+4 | 1000 ms (1 Hz) |
| `ultrasonicTask` | `"us_task"` | 2048 | IDLE+4 | 200 ms (5 Hz) |

**DSP Filters (IMU):**

1. **Gyro bias calibration** (`calibrateGyro()`):
   - 200 stationary samples at init (`IMU_GYRO_CALIB_SAMPLES = 200`)
   - Scale: `raw / 131.0f` (deg/s)
   - Averages to `m_gx_bias`, `m_gy_bias`, `m_gz_bias`
   - 5 ms delay between samples

2. **Accelerometer EMA low-pass pre-filter** (`IMU_ACCEL_LP_ALPHA = 0.15`):
   - `m_ax_filt = IMU_ACCEL_LP_ALPHA * ax_raw + (1 - IMU_ACCEL_LP_ALPHA) * m_ax_filt`
   - Applied to ax, ay, az before angle computation
   - Rejects motor/spike vibration noise

3. **Complementary filter** (`IMU_CF_ALPHA = 0.98`):
   - Accel-based roll: `atan2f(ay_filt, az_filt) × 180 / M_PI`
   - Accel-based pitch: `atan2f(-ax_filt, sqrt(ay_filt² + az_filt²)) × 180 / M_PI`
   - `roll = CF_ALPHA × (roll + gx × dt) + (1 - CF_ALPHA) × roll_acc`
   - `pitch = CF_ALPHA × (pitch + gy × dt) + (1 - CF_ALPHA) × pitch_acc`
   - `yaw += gz × dt` (pure integration, drifts long-term)
   - dt from `esp_timer_get_time()`, clamped to max 0.5 s

4. **MPU6050 constants:**
   - Addr: `0x68`, I2C timeout: 1000 ms
   - Accel scale: `raw / 16384.0f` (g)
   - Gyro scale: `raw / 131.0f` (deg/s)

**DSP Filters (GPS):**

1. **Moving median filter** (window = `GPS_MEDIAN_WINDOW = 5`):
   - Ring buffer: `m_gps_lat_buf[5]`, `m_gps_lon_buf[5]`, `m_gps_hist_idx`
   - `applyMedianFilter(val, buffer)` — copies to temp array, bubble-sort, returns middle element
   - Eliminates single-sample coordinate spikes

2. **NMEA parser** (`parseNMEA`):
   - Parses `$GPRMC` or `$GNRMC` sentences
   - 12-field tokenization by comma
   - Fields: `status[A/V]`, `lat DDMM.MMMM`, `NS`, `lon DDDMM.MMMM`, `EW`
   - Converts to decimal degrees, applies median filter
   - `satellites` hardcoded to 4 (RMC doesn't provide count)
   - If status != `'A'`, sets `fix = false`

**Ultrasonic measurement** (`measureDistanceCm`):
- 10 µs trigger pulse, echo timeout 25 ms (~430 cm max)
- Distance = `duration_us × 0.0343f / 2.0f` (cm)
- Returns `-1.0f` on timeout
- 4 sensors measured sequentially, 10 ms gap between each

#### `MqttManager` (singleton)

**`start(broker_uri, username, password)`:**
- Client ID hardcoded: `"sarcus-robot-esp32"`
- Keepalive: 30, reconnect timeout: 5000 ms
- Uses `esp_mqtt_client_init` + `esp_mqtt_client_register_event` + `esp_mqtt_client_start`

**MQTT Topics (namespace `SARCUS::Topics`):**

| Constant | Topic | Direction |
|----------|-------|-----------|
| `MOVEMENT` | `sarcus/robot/movement` | Subscribe |
| `SHOULDER` | `sarcus/robot/joints/shoulder` | Subscribe |
| `ELBOW` | `sarcus/robot/joints/elbow` | Subscribe |
| `WRIST` | `sarcus/robot/joints/wrist` | Subscribe |
| `SUIT_BUTTONS` | `sarcus/suit/buttons` | Subscribe |
| `SUIT_POTS` | `sarcus/suit/pots` | Subscribe |
| `EMERGENCY_STOP` | `sarcus/robot/estop` | Subscribe |
| `IMU` | `sarcus/sensors/imu` | Publish |
| `GPS` | `sarcus/sensors/gps` | Publish |
| `ULTRASONIC` | `sarcus/sensors/ultrasonic` | Publish |
| `HEARTBEAT` | `sarcus/debug/heartbeat` | Publish |
| `DEBUG_LOGS` | `sarcus/debug/logs` | Publish |

**Publish payloads:**

IMU (10 Hz):
```json
{"roll":%.2f,"pitch":%.2f,"yaw":%.2f,"ax":%.3f,"ay":%.3f,"az":%.3f,"gx":%.2f,"gy":%.2f,"gz":%.2f,"ts":%lu}
```

GPS (1 Hz):
```json
// With fix:
{"fix":true,"lat":%.7f,"lon":%.7f,"alt":%.1f,"sats":%u,"ts":%lu}
// No fix:
{"fix":false,"ts":%lu}
```

Ultrasonic (5 Hz):
```json
{"front":%.1f,"back":%.1f,"left":%.1f,"right":%.1f,"ts":%lu}
```

Heartbeat:
```json
{"device":"%s","uptime":%lu,"ts":%lu}
```

**Dispatch** (`dispatchMessage`): null-terminates topic (128 buf) and data (512 buf), skips fragmented (offset > 0), dispatches to typed callbacks via topic string match.

**JSON helper** (inline, no external lib):
```cpp
static int32_t json_get_int(const char* json, const char* key, int32_t default_val);
```
Searches for `"key"`, skips `:` and whitespace, calls `strtol`.

**Boot Sequence (`app_main`):**

```
1. NVSManager::getInstance().init()
2. readDeviceId() — MAC as XX:XX:XX:XX:XX:XX
3. WifiManager::init() + connectWifi()
   ├── No NVS creds → AP mode (SARCUS_SETUP/sarcus2024) + WebServer
   │   → wait for POST /wifi → startSTA_keepAP → waitForIP(30s)
   │   → success: return; fail: fallback AP
   └── Has creds → startSTA → waitForIP(15s) → success/fail
4. MQTT → register callbacks → MotorController
   ├── onMove → motor.submitMove(cmd, ControlSource::MQTT)
   ├── onShoulder → motor.submitShoulder
   ├── onElbow → motor.submitElbow
   └── onWrist → motor.submitWrist
5a. MotorController::init(STM32_UART_PORT) + start()
5b. UartBridge::init(STM32_UART_PORT, 17, 16, 115200) + start()
6. SensorManager::init(
       I2C_NUM_0, 21, 22,
       UART_NUM_2, 18, 19,
       25,26, 27,14, 12,13, 32,33) + start(100, 1000, 200)
7. DebugManager::init() + HeartbeatManager::start(device_id)
8. Main loop: heartbeat tick every 5000ms
```

**Suit button → MoveCmd mapping:**
- Bit 0: FWD
- Bit 1: BWD
- Bit 2: LEFT
- Bit 3: RIGHT
- Bit 4: STOP
- Bit 5: E-STOP (immediate)
- Speed: 60% default
- Pot 0 → shoulder_left DC angle: `(pots[0]/4095.0)*360 - 180`
- Pot 1 → shoulder_left servo angle: `(pots[1]/4095.0)*270`
- Pot 4 → elbow_left servo angle: `(pots[4]/4095.0)*270`

#### Known Issues (Node A)
- **Legacy I2C driver** (`driver/i2c.h`) EOL in ESP-IDF v6.0 — plan migration to `driver/i2c_master.h`
- **GPS satellite count** hardcoded to 4 — add `$GPGGA` parser for real count
- **No IMU temperature** in MQTT payload (raw temp registers read but discarded)
- **Ultrasonic readings** unfiltered — consider EMA smoothing
- **No i2c device probe** before gyro calibration — ~1 s timeout on every boot if MPU6050 absent

---

## CROSS-NODE COMMUNICATION

### MQTT Flow

| Node A publishes → Sensor Broker | Node B subscribes → Actuator Broker |
|-----------------------------------|--------------------------------------|
| `sarcus/sensors/imu` (10 Hz) | `sarcus/robot/movement` (from Node-RED) |
| `sarcus/sensors/gps` (1 Hz) | `sarcus/robot/joints/shoulder` |
| `sarcus/sensors/ultrasonic` (5 Hz) | `sarcus/robot/joints/elbow` |
| `sarcus/debug/heartbeat` (2 Hz) | `sarcus/robot/joints/wrist` |
| `sarcus/debug/logs` | `sarcus/robot/estop` |

Node B also publishes **wrist status** on `sarcus/robot/joints/wrist` (1 Hz) — handler ignores its own messages.

### No direct UART between the two ESP32s
Node A has UART bridge code targeting STM32F405 (legacy, not active). Two ESP32s communicate only via MQTT through separate brokers.

---

## DEVELOPMENT ENVIRONMENT

| Layer | Tool |
|-------|------|
| Microcontroller | ESP32 (Dual-core Xtensa LX6) |
| Framework | ESP-IDF v6.0.1 |
| Language | C++17 |
| RTOS | FreeRTOS (ESP-IDF fork) |
| MQTT Client | `esp-mqtt` (IDF component or managed `espressif__mqtt`) |
| MQTT Broker | Mosquitto (separate instances) |
| Dashboard | Node-RED + Dashboard 2.0 |
| Toolchain | Windows Terminal + ESP-IDF CMD |
| IDE | VS Code / Cursor |
| Build | `idf.py build` / `idf.py -p COMx flash monitor` |

---

## CODE GENERATION RULES

1. **No `delay()` or `arduino.h`** — always `vTaskDelay(pdMS_TO_TICKS(x))`
2. **Logging:** `ESP_LOGI/LOGW/LOGE/LOGD(TAG, ...)` — each function with ENTRY/EXIT at LOGD level
3. **Thread safety:** shared state → `SemaphoreHandle_t` or `QueueHandle_t`
4. **Servo angles:** clamp 0-270, map to pulse: `500 + (angle × 2000 / 270)`
5. **Motor speed:** 0-100%, always slew-rate limited
6. **MQTT payloads:** JSON — `JsonParser` header for Node B, inline `json_get_int` for Node A
7. **WiFi credentials:** always from NVS, never hardcoded in production
8. **Emergency stop:** immediate, bypass queues, apply all outputs
9. **No magic numbers:** constants in `main.h` (Node B) or `SarcusTypes.h` (Node A)
10. **Modularity:** Node B uses HAL/MCAL driver layer; Node A uses singleton managers
11. **Architecture rule:** no sensor code in Node B, no actuator code in Node A
