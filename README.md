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
