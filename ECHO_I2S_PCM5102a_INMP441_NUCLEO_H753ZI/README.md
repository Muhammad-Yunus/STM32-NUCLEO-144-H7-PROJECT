# STM32H7 Real-time Audio System with Digital Parrot Mode

A real-time audio processing project using the **NUCLEO-H753ZI** to process input from an **INMP441** digital microphone and output it to a **PCM5102A** DAC. This project features high-performance audio synchronization, de-hiss DSP, and a 6-second "Digital Parrot" (Simplex Repeater) mode.

## 🚀 Key Features
- **Synchronous Audio Path**: Microphone and DAC share the same clock domain (SAI1 A+B) to eliminate periodic noise caused by clock drift.
- **Digital Parrot Mode**: Records 6 seconds of audio triggered by the USER button, then automatically plays it back once (prevents acoustic feedback loops on external speakers).
- **Real-time Volume Control**: Dynamic output volume adjustment via an external potentiometer.
- **Optional DSP De-hiss**: Integrated HPF, LPF, and Noise Floor Suppressor to minimize background hiss.
- **High-Fidelity 16 kHz Sampling**: Optimized sample rate for clear voice communication.

## 📌 Pin Configuration

| Component | Function | Nucleo Pin | Notes |
| :--- | :--- | :--- | :--- |
| **PCM5102A DAC** | BCLK | **PE5** | Master Clock |
| | LRCK | **PE4** | Frame Sync |
| | DIN | **PE6** | Data Out (SAI1_SD_A) |
| **INMP441 Mic** | SCK | **PE5** | Shares DAC Clock |
| | WS | **PE4** | Shares DAC Frame Sync |
| | SD | **PE3** | Data In (SAI1_SD_B) |
| **Potentiometer** | Wiper | **PA3** | Volume Control (ADC1) |
| **User Button** | B1 | **PC13** | Trigger Parrot Mode |

## 🛠 Build & Flash Instructions

### Prerequisites
- **STM32CubeIDE** (latest version recommended).
- **SEGGER J-Link** or ST-Link for flashing.
- **CMSIS-DSP** library (included in the project).

### Steps
1. Import the project into STM32CubeIDE.
2. Right-click the project -> **Build Project**.
3. Connect the Nucleo-H753ZI to your PC.
4. Use J-Link or ST-Link to flash the `.elf` file located in the `Debug/` folder.

## 🎤 Demonstration Guide
1. **Adjust Volume**: Rotate the potentiometer on pin **PA3** to set your desired output level.
2. **Standby Mode**: Upon boot, the system starts in `IDLE` mode.
3. **Recording**: Press the blue button (**User Button PC13**) once. The RTT log will show `RECORD` mode. Speak into the microphone for **6 seconds**.
4. **Playback**: After 6 seconds, the system automatically switches to `PLAY` mode and repeats your recording through the DAC.
5. **Monitoring**: Use **SEGGER RTT Viewer** to view real-time diagnostics (`RMS`, `Mode`, `Volume`).

---
*Developed with the assistance of Claude Code AI for DSP optimization and clock synchronization.*
