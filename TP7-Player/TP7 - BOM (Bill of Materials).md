---
tags:
  - bom
  - tp7-player
---
# TP7 - BOM (Bill of Materials)

Daftar lengkap perangkat keras untuk merakit [[TP7 - Player Project Overview]].

## Inti & Pemrosesan
- **ESP32-S3-WROOM-1 (Versi N16R8):** WAJIB versi ini karena membutuhkan 16MB Flash dan 8MB PSRAM.
- **MicroSD Module:** Breakout board standar dengan *level shifter*.
- **MicroSD Card:** 16GB/32GB Sandisk Original (FAT32).

## Antarmuka Manusia & Audio
- **DAC:** [[PCM5102A DAC]] (Modul warna ungu, I2S).
- **Layar:** GC9A01 (TFT IPS 1.28 inci, bulat, SPI).
- **Tombol Utama:** 4x Tactile switch (Play, Memo, Menu, Back).
- **Tombol Samping:** 1x Thumbwheel switch / 1-axis spring return joystick (untuk meniru fungsi rocker tuas asli).
- **Audio Jack:** 3.5mm female (PJ-320A).

## Haptic / Roda Kontrol (Reel)
- **Motor:** [[Motor BLDC 2204]] (Gimbal motor).
- **Motor Driver:** SimpleFOC Mini v1.0 (Chip DRV8313).
- **Sensor Sudut:** MT6701 (Modul I2C/SSI 14-bit) beserta magnet diametrik pasangannya.
- **Tombol Reel:** 1x Tactile switch (Diposisikan di bawah poros motor sebagai klik tekan tengah).

## Power & Tambahan
- **Baterai:** Li-Po 3.7V (~1000mAh - 1200mAh), usahakan yang paling tipis.
- **Charger:** TP4056 + DW01 (Modul Type-C dengan proteksi).
- **I/O Expander:** MCP23017 (DIP-28 atau modul, karena I/O ESP32 habis).
- **Pasif:** Resistor 4.7k (2x, untuk pull-up I2C), Resistor 100k (2x, pembagi tegangan baterai), Resistor 330 ohm, 1x LED Merah.

*(Opsional: Modul Radio FM RDA5807M).*