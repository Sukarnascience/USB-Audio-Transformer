# USB-Audio-Transformer

The **USB-Audio-Transformer** is a hardware-based, plug-and-play voice changer built on the **STM32F407G-DISC1**. This project transforms the discovery board into a standard USB Microphone that any computer can recognize without extra drivers.

### **Project Goal**
The goal is to provide a seamless, low-latency audio experience where a user can speak into the onboard mic and, with a single button press, instantly modify their voice (such as pitch-shifting or robotic effects) before the audio is sent to the PC over USB.

### **Key Features**
*   **Plug-and-Play:** Uses the USB Audio Device Class for universal compatibility.
*   **Real-Time DSP:** Processes audio instantly using the Cortex-M4 processor.
*   **One-Touch Control:** Uses the onboard blue button to cycle through voice effects.
*   **No Latency:** Direct hardware processing ensures no lag during live calls or gaming.

### **Hardware Requirements**
*   STM32F407G-DISC1 Discovery Board
*   Mini-USB cable (for programming)
*   Micro-USB cable (for the PC Audio connection)
