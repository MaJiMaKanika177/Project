---
tags:
  - software
  - ram
  - optimization
---
# TP7 - Memori dan Software

Optimalisasi perangkat lunak dan manajemen memori (RAM) pada proyek [[TP7 - Player Project Overview]].

## Krisis SRAM Statis (Masalah Awal)
Mikrokontroler ESP32-S3 memiliki SRAM internal sekitar 512KB. Sebelumnya, program mengalokasikan memori statis secara berlebihan untuk daftar putar (Playlist) dan cache UI, memakan hingga ~186KB, menyebabkan masalah *heap fragmentation* dan gagal kompilasi.

## Solusi: Pemindahan ke PSRAM
Karena ESP32-S3 versi **N16R8** memiliki PSRAM eksternal 8MB, beban memori telah digeser:
- Cache *Sprite* 240x240 dari layar GC9A01 dipindahkan ke PSRAM (`setPsram(true)` pada LovyanGFX). Memakan sekitar ~115KB.
- Array statis besar (Daftar file, lintasan, dll.) dipindahkan menggunakan alokasi dinamis `ps_calloc`.
- **Hasil:** Penggunaan RAM internal (SRAM) turun drastis menjadi hanya **18.7%**.

## Arsitektur Threading (FreeRTOS)
Pemutaran audio (I2S) dan *Field Oriented Control* (FOC) motor memiliki tuntutan ketat terhadap *timing*. 
Oleh sebab itu beban dibagi ke beberapa Core (*Pinned To Core*):
- **Core 0:** Komunikasi Jaringan (Web Server/WiFi), *Input polling* dari tombol MCP23017, Pembaruan UI.
- **Core 1:** Tugas prioritas tinggi. Loop utama Audio I2S, dan Loop kalkulasi FOC Motor (~4kHz). Kedua ini membutuhkan CPU cycle tanpa *interruption* dari tugas I/O atau I2C layar.