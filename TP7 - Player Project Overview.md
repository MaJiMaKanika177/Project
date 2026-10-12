---
tags:
  - hardware
  - esp32
  - audio
  - tp7-player
date: 2026-09-06
---
# TP-7 Player Project Overview

Proyek ini bertujuan membuat replika MP3 player bergaya Teenage Engineering TP-7 menggunakan mikrokontroler ESP32-S3. Fokus utamanya adalah pada form factor *handheld* (100x70x18mm) dan menggunakan komponen berkualitas tinggi.

## Fitur Utama
- **MCU:** ESP32-S3-WROOM-1 (N16R8) - Memberikan memori yang cukup (PSRAM 8MB) untuk buffering audio dan cache UI (lihat [[TP7 - Memori dan Software|Manajemen Memori]]).
- **Audio Output:** [[PCM5102A DAC]] via I2S, menghasilkan kualitas suara Hi-Fi (32-bit/384kHz, 112dB SNR).
- **Mekanik:** [[Motor BLDC 2204]] dikendalikan dengan *Field Oriented Control* (FOC) sebagai roda kontrol utama (reel), memberikan *tactile feedback* (detent) yang bisa diatur via software.
- **Display:** Layar TFT bulat GC9A01 1.28 inci, menampilkan antarmuka yang berotasi sinkron dengan putaran motor fisik.

## Keterkaitan Sistem
- [[TP7 - Arsitektur Kode|Penjelasan Struktur Program]]
- [[TP7 - Wiring dan Pinout|Wiring Diagram Lengkap]]
- [[TP7 - BOM (Bill of Materials)|Daftar Belanja Komponen]]
- [[TP7 - Tantangan Desain Fisik|Kendala perakitan handheld]]
- [[TP7 - Panduan Tes Hardware|Panduan Tes Modul Satu-satu]]

## Status Terkini
Firmware utama (termasuk driver FOC, UI rotasi, dan manajemen I/O) telah dikompilasi (RAM 18.7%, Flash 40%). Menunggu perakitan perangkat keras untuk kalibrasi FOC fisik.