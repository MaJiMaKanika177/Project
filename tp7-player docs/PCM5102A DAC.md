---
tags:
  - hardware
  - audio
  - dac
---
# PCM5102A DAC

Modul PCM5102A adalah I2S Digital-to-Analog Converter (DAC) andalan dalam proyek [[TP7 - Player Project Overview|TP-7 Player]]. Chip ini memproses data digital dari ESP32 menjadi sinyal audio analog untuk jack 3.5mm.

## Spesifikasi Kunci
- **Resolusi:** 32-bit / 384kHz
- **SNR (Signal-to-Noise Ratio):** 112 dB
- **Karakteristik:** Menghasilkan latar belakang audio yang sangat bersih tanpa *hiss*.

## Keunggulan (Dibanding Internal/MAX98357A)
- **DirectPath:** Memiliki *charge pump* internal, memungkinkan output analog murni (2.1V RMS) langsung ke earphone tanpa kapasitor pemblokir DC besar, membuat suara *bass* sangat solid.
- **Internal PLL:** Mengelola *clocking* internal, membebaskan ESP32 dari keharusan menyediakan sinyal *Master Clock* (MCLK) yang sangat presisi, tahan terhadap *jitter* I2S ESP32.

## Peringatan Perangkat Keras
**Kritis:** Pin **SCK** pada modul ini **WAJIB** dihubungkan ke `GND`. Jika mengambang (floating), suara tidak akan keluar atau akan terdengar penuh *glitch*.

Kembali ke: [[TP7 - Wiring dan Pinout]]