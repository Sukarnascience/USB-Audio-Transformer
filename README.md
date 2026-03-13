# STM32 Performance Comparison: Blue Pill vs. STM32F407G

This project explores the technical trade-offs between the entry-level **Blue Pill (F103)** and the high-performance **STM32F4 Discovery (F407G)**. It serves as a selection guide for choosing the right ARM Cortex-M microcontroller based on processing power, peripherals, and debugging needs.

### 🎯 Project Overview
The goal is to demonstrate the massive jump in capabilities between the two boards:
*   **The Blue Pill:** A minimalist, budget-friendly board for simple logic and "Arduino-style" DIY projects.
*   **The F407G-DISC1:** A professional-grade development kit designed for digital signal processing (DSP), audio handling, and complex automation.

### 📊 Key Technical Specs

| Feature | Blue Pill (F103C8T6) | STM32F407G-DISC1 |
| :--- | :--- | :--- |
| **Core** | Cortex-M3 (72 MHz) | Cortex-M4 + FPU (168 MHz) |
| **SRAM / Flash** | 20 KB / 64 KB | 192 KB / 1 MB |
| **On-board Tools** | Basic LEDs & Reset | ST-LINK Debugger, Accel, Mic, Audio DAC |
| **Ideal For** | Simple GPIO, I2C, SPI | Audio, USB OTG, Math-heavy tasks |

### 🚀 Summary
Choose the **Blue Pill** for small, low-power, and cost-sensitive applications. Choose the **STM32F407G** when you need hardware-accelerated math (FPU), massive memory, or built-in sensors for rapid prototyping without external modules.
