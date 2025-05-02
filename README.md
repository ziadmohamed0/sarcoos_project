# SarcoOS Project – Minimal RTOS for Embedded Systems

A modular and educational real-time operating system (RTOS) kernel written in C, built from scratch for learning and experimentation with low-level embedded systems concepts such as context switching, scheduling, tasks, synchronization, and memory management.

---

## 🔍 Overview

**SarcoOS** is a lightweight RTOS tailored for embedded developers and engineering students seeking deep understanding of:

- Context Switching  
- Cooperative & Preemptive Scheduling  
- Stack Management & Thread Isolation  
- Basic Inter-process Communication  
- System Timer Interrupt Handling

The architecture is fully modular to allow step-by-step development, testing, and scaling.

---

## 📁 Project Structure

| Folder/File       | Description                                       |
|-------------------|---------------------------------------------------|
| `kernel/`         | Core RTOS source (scheduling, threading, ISR)    |
| `hal/`            | Hardware Abstraction Layer (ARM Cortex-M, STM32) |
| `examples/`       | Demo tasks to validate scheduler and task switching |
| `startup/`        | Assembly startup code for STM32F4                 |
| `include/`        | Header files (OS API, structs, macros)           |
| `Makefile`        | Compiles kernel using GCC and STM32 toolchain    |
| `linker.ld`       | Linker script for STM32F401 target board         |

---

## 🔧 Toolchain

- `arm-none-eabi-gcc` (GNU Arm Embedded Toolchain)  
- STM32F401 + STM32Cube Programmer  
- OpenOCD or ST-Link utility  
- VSCode + Cortex-Debug extension

---

## 🚀 Getting Started

### 🔨 Build the Project

```bash
make clean && make all
````

### 🧪 Flash to Target Board

Ensure the board is connected, then:

```bash
make flash
```

---

## ⚙️ Features

* [x] Task creation with stack isolation
* [x] Round-robin scheduler (cooperative)
* [x] Cortex-M SVC (Supervisor Call) based context switching
* [x] SysTick-based scheduling
* [ ] Mutex/Semaphore (planned)
* [ ] Preemptive scheduling (coming)
* [ ] Priority queue for task execution

---

## 🧾 Example Demos

Located in `examples/`:

* Two simple tasks switching using SysTick interrupts
* Context switch handling via SVC and PendSV
* UART logging enabled (if connected)

---

## 📦 Supported Hardware

| Board Name               | Status                  |
| ------------------------ | ----------------------- |
| STM32F401RE (Nucleo)     | ✅ Fully supported       |
| STM32F103C8T6 (Bluepill) | ⚠️ Minor edits required |

---

## 🎯 Educational Goals

This project helps embedded engineers:

* Understand ARM Cortex-M processor modes & stack usage
* Learn how real RTOS context switching works
* Debug memory, ISR nesting, and scheduling behavior
* Build a functioning RTOS core without third-party dependencies

---

## 🧠 Inspiration

* ARM Cortex-M Architecture Manual
* FreeRTOS & CMSIS-RTOS designs
* Embedded OSDev tutorials
* Personal learning journey in low-level development

---

## 📎 Resources

* [STM32F4 Reference Manual](https://www.st.com/resource/en/reference_manual/dm00031020.pdf)
* [ARM Cortex-M Architecture](https://developer.arm.com/documentation/ddi0403/latest/)
* [GNU Arm Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm)
* [OpenOCD](http://openocd.org/)

---

## 🤝 Contributing

Pull requests and feedback are welcome — feel free to fork, test, and suggest:

* Scheduler improvements (priority/EDF)
* Synchronization primitives
* Porting to new STM32 boards

---

## 👤 Maintainer

**Ziad Mohammed Fathy**
GitHub: [https://github.com/ziadmohamed0](https://github.com/ziadmohamed0)

---

## 🛡️ License

MIT License

> “Building an RTOS from scratch teaches you not only how systems work — but why they fail.”
