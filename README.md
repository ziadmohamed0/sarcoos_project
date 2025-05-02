
# SarcoOS Project – Modular RTOS for Embedded Robotics & IoT

**SarcoOS** is a modular RTOS project combining real-time performance with flexibility for robotics, IoT, and embedded applications. Designed to run across multiple platforms—from STM32 to ESP32 to Raspberry Pi 4—SarcoOS is ideal for engineers seeking control, performance, and a deeper understanding of system internals.

---

## 🚀 Use Case Highlights

This project is used in a real-world robotics system with the following structure:

- 🤖 **Raspberry Pi 4 Model B**  
  - Handles main robot logic, Computer Vision (OpenCV), and IoT interface
  - Communicates with microcontrollers over UART/Wi-Fi

- ⚙️ **STM32F405RGT6 + NodeMCU ESP32S**  
  - Runs SarcoOS kernel with real-time control logic  
  - Wi-Fi handled via ESP32S module using custom protocol

- 🌐 **ESP32 (Standalone)**  
  - Used as a smart SUIT node for additional offloaded processing

---

## 🔍 Overview

SarcoOS is an educational RTOS written in C for bare-metal embedded systems. It includes:

- Manual context switching
- Stack-separated task creation
- Cooperative scheduling
- Modular ISR handling
- Cross-platform build targeting Cortex-M4, Xtensa, and Linux AArch64

---

## 🧠 Key Goals

- Understand low-level architecture (ARM Cortex-M, Xtensa, AArch64)  
- Enable flexible task scheduling across MCUs  
- Integrate MCU logic with higher-level Linux-based coordination  
- Provide networking with ESP32 over UART and native Wi-Fi stack  

---

## 🛠️ Target Hardware Platforms

| Platform                | Role                         |
|-------------------------|------------------------------|
| Raspberry Pi 4 Model B  | Main brain, CV, coordinator  |
| STM32F405RGT6           | Real-time control core       |
| NodeMCU ESP32S          | Wi-Fi gateway for STM32      |
| ESP32 DevKit (Standalone)| Offloaded suit features      |

---

## ⚙️ Features (In Progress)

- [x] Cooperative RTOS kernel (STM32)  
- [x] UART communication layer (Pi ↔ STM32)  
- [x] Task scheduler (basic round-robin)  
- [x] Inline ARM assembly context switch  
- [x] Basic interrupt handling  
- [ ] Preemptive scheduling (Coming)  
- [ ] ESP32 messaging stack  
- [ ] Computer Vision task dispatcher (Pi side)  
- [ ] OTA support for ESP-based nodes  

---

## 🧪 How to Build & Run

- For **STM32F405RGT6**:
  ```bash
  make TARGET=stm32
  make flash


* For **ESP32 (ESP-IDF)**:

  * Use `idf.py build && idf.py flash` inside the ESP32 component folder

* For **Raspberry Pi 4**:

  * Native Python/C++ code (OpenCV + MQTT + Serial Parser)

---

## 🧩 Project Structure

| Folder      | Description                         |
| ----------- | ----------------------------------- |
| `kernel/`   | Core SarcoOS scheduler & syscall    |
| `hal/`      | Hardware abstraction (STM32, ESP32) |
| `rpi/`      | High-level control scripts for RPi4 |
| `wifi/`     | ESP32 communication interface       |
| `examples/` | Task demos & use cases              |
| `Makefile`  | Target-specific build instructions  |

---

## 🧑‍💻 Maintainer

Developed by **Ziad Mohammed Fathy**
🔗 GitHub: [https://github.com/ziadmohamed0](https://github.com/ziadmohamed0)

---

## 🛡️ License

MIT License

> "RTOS development isn't just about multitasking—it's about mastering control."
