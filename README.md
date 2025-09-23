# esp32-lvgl-eis
Portable Electrochemical Impedance Spectroscopy (EIS) device using ESP32-S3, LVGL, and SD logging.

# Portable EIS Measurement Device (ESP32-S3 + LVGL)

## 📖 Overview
This project implements a **Portable Electrochemical Impedance Spectroscopy (EIS) Measurement Device** using **ESP32-S3**, **DNB110xA modules**, and **LVGL 9 UI**.  
It enables real-time impedance measurement, visualization (Nyquist & 1 kHz plots), and CSV logging to SD card for **battery quality diagnostics**.

Core features:
- 6 measurement modes (single scan, average, continuous, loop, 1 kHz live, 1 kHz averaged).
- Real-time plots (Nyquist & time series) with LVGL.
- User-friendly **Button Panel UI** (mode selection, averaging slider, start/stop controls).
- SD card logging with organized folders.
- Dual-core FreeRTOS:  
  - Core 0 → UI + Logging  
  - Core 1 → Measurement tasks  

---

## 🗂 Project Structure




---

## ⚡ Measurement Modes
Defined in `MeasurementMode.h`:

| Mode | Name             | Description |
|------|------------------|-------------|
| 1    | Single Scan      | One-time 19 frequency sweep |
| 2    | Average Scan     | Multiple sweeps averaged (user selectable X) |
| 3    | Continuous       | Sweep every 60 s until stopped |
| 4    | Average Loop     | Averaging + repeat every 60 s |
| 5    | 1 kHz Live       | Real-time impedance at 1 kHz (continuous stream) |
| 6    | 1 kHz Averaged   | Fixed number of 1 kHz samples, averaged |

---

## 🎨 User Interface (LVGL)
- **Button Panel (`ButtonPanel`)**:  
  - Mode selection (radio buttons)  
  - Averaging slider (for modes 2, 4, 6)  
  - Start/Stop buttons  
  - Progress dialog with status updates  

- **Plots (`EISPlot`)**:  
  - Nyquist plot (mΩ, real vs imaginary)  
  - 1 kHz time series plot (mΩ over time)  
  - Voltage & temperature display  
  - Popup data tables for detailed values  

---

## 💾 Data Logging
- **Thread-safe logging** via `EISLogger` and `LoggingQueue`:  
  - Each measurement saved as CSV into `/data/modeX/` directory  
  - Filenames auto-increment with counter + timestamp  
  - Example:  
    ```
    /data/mode1/EIS_SingleScan_1_120530.csv
    ```
- Supports:  
  - Full dataset (Modes 1–4)  
  - Chunked 1 kHz dataset (Modes 5–6, large sample support)  
  - Individual sample streaming  

---

## 🔧 Hardware Setup
- **ESP32-S3 Dev Module** (dual-core, PSRAM enabled)  
- **DNB110xA impedance modules** (UART daisy-chain)  
- **JC4827W543 4.3″ LVGL touchscreen display**  
- **MicroSD card** for logging  
- **Battery fixture (18650 cell)** for testing  

Connections:  
- ESP32-S3 ↔ NXPPoint (UART1)  
- ESP32-S3 ↔ LCD (SPI + I2C GT911 for touch)  
- ESP32-S3 ↔ SD card (SPI)  

---

## ▶️ How to Run
1. Clone project into Arduino IDE / PlatformIO.  
2. Select **ESP32-S3 Dev Module**, enable **PSRAM**.  
3. Upload `enum.ino` to ESP32-S3.  
4. Insert SD card and connect a battery.  
5. On touchscreen UI:  
   - Select mode → adjust averaging if applicable.  
   - Press **START** → measurement begins.  
   - Press **STOP** (where available) to terminate.  
   - View results in plots & data tables.  

---

## 📊 Example Output
- **Nyquist Plot (Modes 1–4):** Real vs Imaginary impedance across 19 frequencies.  
- **1 kHz Plot (Modes 5–6):** Time series of impedance at 1 kHz.  
- **CSV Log Example:**  

