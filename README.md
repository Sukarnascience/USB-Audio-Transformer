# BeeVoice Changer — STM32 USB Audio Voice Modulator

A real-time USB microphone with onboard voice effects built on the STM32F407G-DISC1 discovery board. Plug it into any PC and it instantly appears as a standard USB microphone — no drivers needed — with four distinct voice effects selectable by pressing the onboard button.

---

## What Is This?

BeeVoice Changer is an embedded firmware project that turns an STM32F407G-DISC1 development board into a USB Audio Class (UAC 1.0) microphone with real-time DSP voice transformation. The onboard MEMS microphone (MP45DT02) captures audio, processes it through one of four voice effects entirely on the STM32, and streams the transformed audio to a connected PC over USB at 16kHz / 16-bit mono — all within a 1ms latency budget.

No PC-side software, no drivers, no audio middleware. The board enumerates as a standard USB microphone and works with any application that accepts microphone input: Discord, OBS, Teams, Zoom, Audacity, Voice Recorder, and so on.

---

## Why This Project?

Most voice changers are software applications running on a host PC, which means they introduce latency, consume CPU, and require installation. This project explores whether all of that can be pushed entirely onto a microcontroller — making the transformation happen in the hardware before any audio even reaches the PC.

The key challenges this project solves:

- Implementing UAC 1.0 isochronous audio streaming from scratch on STM32 without relying on ST's AUDIO class driver (which is speaker-only)
- Building a PDM-to-PCM decimation pipeline using a 3-stage CIC filter with pre-emphasis compensation directly in firmware
- Running real-time overlap-add (OLA) pitch shifting in fixed-point arithmetic within the 1ms USB audio frame budget on a Cortex-M4
- Making the device enumerate correctly on Windows 11 without a custom INF driver

---

## Applications

- **Online meetings** — join a call with a giant, chipmunk, Vader, or alien voice without any software on the host
- **Game streaming** — apply a character voice live in OBS or streaming software
- **Halloween / events** — plug into a speaker system and talk through the effect
- **Education** — learn how USB Audio Class, PDM microphones, and real-time DSP work together on embedded hardware
- **Rapid prototyping** — serves as a working reference implementation for anyone building a custom USB audio device on STM32

---

## How It Works

### Hardware

| Component | Detail |
|---|---|
| MCU | STM32F407VGT6, Cortex-M4 @ 168MHz with FPU |
| Board | STM32F407G-DISC1 (MB997E) |
| Microphone | MP45DT02 onboard PDM MEMS mic |
| USB | OTG Full-Speed, CN5 mini-USB connector |
| Control | PA0 USER button, PD12–PD15 LEDs |

### Signal Chain

```
MP45DT02 (PDM)
     |
     | 1.024 MHz PDM bitstream
     v
  I2S2 peripheral (master RX, DMA circular)
     |
     | 16-bit words @ 64kHz word rate
     v
  CIC decimation filter (3-stage, R=64)
     |
     | Decimates 64x -> 16kHz PCM
     v
  Pre-emphasis FIR  y[n] = x[n] - 0.97*x[n-1]
     |
     | Compensates CIC high-frequency droop
     v
  PCM ring buffer (256 samples = 16ms)
     |
     v
  DSP voice effect (OLA pitch shift / AM modulation)
     |
     v
  USB isochronous IN endpoint (EP 0x81)
     |
     | 32 bytes per 1ms USB frame
     v
  Windows USB Audio driver (usbaudio.sys)
     |
     v
  Any recording / communication application
```

### USB Audio Stack

The firmware implements a UAC 1.0 compliant audio device from scratch:

- **Device descriptor**: Standard USB 2.0 device, class defined at interface level
- **Configuration descriptor**: Two interfaces — AudioControl (IF 0) and AudioStreaming (IF 1)
- **Audio topology**: Input Terminal (Microphone) → Feature Unit → Output Terminal (USB streaming)
- **Streaming**: Isochronous IN endpoint, 32 bytes per frame, 16kHz / 16-bit / mono
- **Packet pump**: Double-buffer ping-pong driven by USB DataIn callback, no polling

### PDM Microphone Pipeline

The MP45DT02 outputs a 1-bit PDM bitstream at 1.024 MHz. I2S2 in master RX mode captures this as 16-bit words at 64kHz. A 3-stage CIC decimation filter (integrator → downsample × 64 → comb) converts every 1024 PDM bits into 16 PCM samples at 16kHz. A single-pole pre-emphasis filter then compensates the inherent high-frequency rolloff of the CIC, restoring intelligible speech.

### DSP Voice Effects

All four effects run in fixed-point arithmetic on the Cortex-M4 within each 1ms audio frame:

| LED | Effect | Technique | Character |
|---|---|---|---|
| Orange | Giant | OLA pitch shift ×0.5 (hop=8) | 1 full octave down, deep demon voice |
| Green | Chipmunk | OLA pitch shift ×1.8 (hop=29) | Very high and squeaky |
| Red | Vader | OLA ×0.75 (hop=12) + 8Hz AM | Slightly lower with slow ominous pulse |
| Blue | Alien | OLA ×1.3 (hop=21) + 6Hz vibrato | Slightly higher with fast wobble |

**Overlap-Add (OLA)** pitch shifting works by reading the input history ring buffer at a different rate than the output is consumed. A 64-point Hann window is applied and overlapped-added into an accumulator to avoid discontinuities. No FFT is required — the entire algorithm runs as fixed-point multiply-accumulates.

**Amplitude Modulation (Vader)** multiplies the already pitch-shifted signal by a triangle wave envelope at 8Hz, ranging 50%–100% amplitude. This creates the ominous breathing cadence associated with the character without affecting speech intelligibility.

**Vibrato (Alien)** reads from a short delay line at a position modulated by a 6Hz triangle LFO, varying 1–5 samples of delay. The periodic pitch wobble gives speech a non-human quality.

### Clock Configuration

```
HSI 16MHz
  └─ PLL (M=16, N=336, P=2, Q=7)
       ├─ SYSCLK  = 168 MHz
       ├─ USB clk = 48 MHz  (Q=7, mandatory for USB)
       └─ PLLI2S (N=192, R=5)
            └─ I2S2 clk = 38.4 MHz -> PDM bit clock ~1.024 MHz
```

---

## Project Structure

```
USB-Audio-Transformer/
├── platformio.ini
├── include/
│   ├── board.h              Pin definitions
│   ├── usbd_conf.h          USB HAL glue configuration
│   ├── usbd_desc.h          USB descriptor declarations
│   ├── usbd_audio_mic.h     UAC microphone class driver
│   ├── pdm_mic.h            PDM microphone driver
│   └── dsp_effects.h        Voice DSP effects
└── src/
    ├── main.cpp             Application: button, LED, audio callbacks
    ├── usbd_conf.cpp        USB HAL glue (PCD callbacks, LL interface)
    ├── usbd_desc.c          Device / string descriptors
    ├── usbd_audio_mic.c     UAC 1.0 mic class driver
    ├── pdm_mic.cpp          I2S2 DMA + CIC decimation + ring buffer
    ├── dsp_effects.cpp      OLA pitch shift, AM, vibrato
    └── usb_core/
        ├── usbd_core.c      ST USB device middleware (copied)
        ├── usbd_ctlreq.c
        └── usbd_ioreq.c
```

---

## Building and Flashing

Requirements: PlatformIO with the `ststm32` platform installed.

```bash
# Build
pio run

# Flash via ST-Link (CN1)
pio run --target upload

# Clean build
Remove-Item -Recurse -Force .pio   # Windows
rm -rf .pio                         # Linux/macOS
pio run --target upload
```

Connect the CN5 mini-USB port to your PC after flashing. The device appears under **Sound, Video and Game Controllers** as **BeeVoice Changer Mic**.

---

## Usage

1. Flash the firmware via CN1 (ST-Link)
2. Connect CN5 to PC — device enumerates automatically
3. Set **BeeVoice Changer Mic** as the input device in your application
4. Press the **USER button** to cycle through voice effects:
   - **Orange LED** → Giant (deep)
   - **Green LED** → Chipmunk (high)
   - **Red LED** → Vader (ominous)
   - **Blue LED** → Alien (wobble)

---

## Author

**Sukarna Jana** — BeeBotix Autonomous Systems

---

## License

MIT License. See LICENSE for details.