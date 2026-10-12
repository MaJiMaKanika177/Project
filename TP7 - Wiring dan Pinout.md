---
tags:
  - wiring
  - pinout
  - esp32
---

# TP7 - Wiring dan Pinout

Skema penyambungan kaki (pinout) dari ESP32-S3 ke semua komponen di [[TP7 - Player Project Overview]].

## Peta Pin ESP32-S3
```text
I2S (Audio)  : GPIO 4, 5, 6
SPI2 (SD)    : GPIO 10, 11, 12, 13
SPI3 (TFT)   : GPIO 38, 39, 40, 41, 42, 47
I2C 0 (MCP)  : GPIO 8, 9
I2C 1 (MT)   : GPIO 1, 2
ADC (Baterai): GPIO 18
Motor (PWM)  : GPIO 14, 15, 16, 21
Tombol Reel  : GPIO 3
NeoPixel     : GPIO 48
Interrupt    : GPIO 7
```

## Detail Sambungan

**[[PCM5102A DAC]] (Audio I2S)**
- BCK -> GPIO4, LCK -> GPIO5, DIN -> GPIO6
- **Kritis:** SCK -> GND, VIN -> 3V3, GND -> GND.

**Layar TFT GC9A01 (SPI3)**
- SCK -> GPIO42, MOSI -> GPIO41, CS -> GPIO38, DC -> GPIO39, RST -> GPIO40, BL -> GPIO47
- VCC -> 3V3.

**MicroSD (SPI2)**
- SCK -> GPIO10, MOSI -> GPIO11, MISO -> GPIO12, CS -> GPIO13

**Motor Driver DRV8313 (Untuk [[Motor BLDC 2204]])**
- IN1 -> GPIO14, IN2 -> GPIO15, IN3 -> GPIO16, EN -> GPIO21
- VM -> 5V/VBAT, GND -> GND
- M1, M2, M3 -> Kabel fasa motor.

**Sensor MT6701** (Angle I2C/Wire1) — **Ganti di Fase 2**.
- SDA -> GPIO1, SCL -> GPIO2
- **Ganti ke:** **Modul AS5600** (karena MT6701 susah beli).
- AS5600 terhubung **sama jalur kabel GPIO1/GPIO2** (sama-sama I2C, 0x36).
- Kalau pakai AS5600, wiring sama: SDA GPIO1, SCL GPIO2.

**MCP23017 (I2C Expander)**
- SDA -> GPIO8, SCL -> GPIO9 (Membutuhkan Resistor Pull-up 4.7k ke 3V3)
- INTA -> GPIO7
- A0, A1, A2 -> GND (Alamat 0x20). RESET -> 3V3.
- GPA0 - GPA5 -> Tombol (Sisi lain tombol ke GND).
- GPA6 -> LED Merah (via Resistor 330 ohm).

**Baterai ADC (Pembagi Tegangan)**
- Baterai + -> Resistor 100k -> GPIO 18 -> Resistor 100k -> GND.