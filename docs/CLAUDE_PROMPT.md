# Claude AI Documentation Prompt

Copy and paste the following prompt into Claude AI to generate your project book.

---

**Task:** Write a comprehensive, highly detailed technical manual/book for my graduation project called "**SarcoOS**". The manual should be professional, academic, and extremely thorough ("التفاصيل الممله").

### 1. Project Identity & Team
- **Project Name:** SarcoOS (Elite Modular RTOS Framework for Teleoperation & Robotics).
- **University:** Helwan International Technology University.
- **Supervisor:** Dr. Ahmed Suwaidan (د. احمد سويدان).
- **The Team:**
  - Ziad Mohamed Fathy (زياد محمد فتحي)
  - Abdelrahman Ashraf (عبد الرحمن اشرف)
  - Abdelrahman Ahmed (عبد الرحمن احمد)
  - Abdelrahman Alaa (عبد الرحمن علاء)
  - Ahmed Zaghloul (احمد زغلول)
  - Ziad Hussein Mohamed Salah (زياد حسين محمد صلاح)
  - Seif Ayman (سيف ايمن)
  - Ammar Yasser (عمار ياسر)
  - Sondos Mohamed (سندس محمد)

### 2. Software Architecture & Technology Stack
Explain in detail **WHY** we used each technology:
- **Programming Language:** Modern C++ (for high-performance, object-oriented modularity, and memory safety in critical sections).
- **Framework:** ESP-IDF (Espressif IoT Development Framework) for native performance and full control over ESP32 features.
- **Build System:** CMake (for cross-platform standard build automation).
- **Operating System:** FreeRTOS (to handle deterministic real-time multitasking, task prioritization, and semaphore-based synchronization).
- **Architecture:** Layered Architecture (HAL/MCAL) to decouple high-level robotic logic from low-level hardware drivers, ensuring portability and maintainability.
- **IIoT Stack:** Node-RED (for the HMI dashboard), Mosquitto Broker (MQTT) for lightweight, low-latency asynchronous communication.

### 3. ESP32 Firmware & Drivers
Deep-dive into every driver implemented in the `code/esp32_driver/drivers` folder:
- **BTS7960:** High-current H-bridge driver control using PWM (LEDC peripheral) for DC motors.
- **MPU6050:** 6-DOF IMU for orientation and motion tracking using I2C.
- **TMC2209:** Silent stepper motor drivers using STEP/DIR interface for precision movement.
- **GPS (Neo-6M):** Position tracking using UART communication.
- **Potentiometer:** Analog input processing using ADC with attenuation and multi-sampling/calibration.
- **PID Control:** Software implementation of Proportional-Integral-Derivative loops for motor position/speed stabilization.
- **System Services:** NVS (Non-Volatile Storage) for saving calibration, WiFi Manager, and MQTT Client for data orchestration.

### 4. Custom PCB & Hardware Integration
Analyze the PCB design based on the following connections:
- **Microcontroller:** ESP32 DevKitC-32E.
- **Power System:** DCDC Buck Converter (Step Down) to provide stable 5V from a 24V source.
- **Ground Strategy:** Dedicated 2-layer ground plane for EMI reduction and signal integrity.
- **Pin Mapping (from Schematics):**
  - **TMC2209 (1):** STEP1 (IO25), DIR1 (IO26).
  - **TMC2209 (2):** STEP2 (IO35), DIR2 (IO12).
  - **BTS7960 (1):** PWML1 (IO27), PWMR1 (IO14), EN pins (R_EN1/L_EN1).
  - **BTS7960 (2):** PWML2 (IO32), PWMR2 (IO33), EN pins (R_EN2/L_EN2).
  - **I2C Bus:** SCL (IO22), SDA (IO21) connecting MPU6050 and PCA9685.
  - **GPS:** TX (IO17), RX (IO16).
  - **External Links:** Header for RPi 4 (GPIO 4-pin), PCA9685 (4-pin), and BTS drivers (6-pin).

### 5. Mechanical & 3D Design
- **CAD Software:** SolidWorks.
- **Components:**
  - 2x 24V DC Motors for drivetrain.
  - 2x Feetech 95kg High-Torque Servos for heavy-duty arm joints.
  - 2x Feetech 35kg Servos for secondary joints.
  - 2x Nema23 Stepper Motors for the gripper mechanism.
  - Power Supplies: 24V (Main) and 12V (Auxiliary).
  - Computational Core: Raspberry Pi 4 Model B for high-level ROS processing.

### 6. Teleoperation Wearable Suit
- **Purpose:** A human-to-robot interface "suit" worn by a person to control the main robot wirelessly.
- **Hardware:** ESP32, 8x Potentiometers (mapping human joint angles), and a physical button for gripper toggle.
- **Communication:** Sends real-time telemetry via MQTT to the Mosquitto broker, which the main robot subscribes to for motion execution.

### 7. ROS 2, Simulation & SLAM
Explain the workflow of bringing the mechanical design to life:
- **URDF Export:** SolidWorks design converted to URDF (Unified Robot Description Format).
- **Gazebo:** Simulation environment with standard plugins for differential drive, IMU, and joint control.
- **Sensor Fusion:** Combining Vision Sensors (Camera) with IMU data.
- **SLAM (Simultaneous Localization and Mapping):** Creating a map of the environment and estimating the robot's position within it.
- **Navigation:** Path planning and localization using ROS 2 navigation stacks.

### 8. Writing Instructions
- Use a professional, technical tone.
- Add code snippets where appropriate (C++ for drivers, Python for Launch files).
- Include detailed explanations of the "Mathematics" behind things like PID and SLAM.
- Format the output into distinct chapters (e.g., Chapter 1: Introduction, Chapter 2: Hardware Design, Chapter 3: Software Implementation, Chapter 4: Mechanical & Control, Chapter 5: ROS 2 & Navigation).

---
