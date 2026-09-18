---
tags:
  - hardware
  - bldc
  - foc
---
# Motor BLDC 2204

Motor BLDC tipe Gimbal ukuran 2204 (Stator 22mm x 4mm) dipilih sebagai aktuator/input fisik (reel) pada [[TP7 - Player Project Overview|TP-7 Player]]. 

## Alasan Pemilihan
1. **Profil Tipis:** Tebal total motor di bawah 10mm, sangat krusial untuk memenuhi [[TP7 - Tantangan Desain Fisik|batas ketebalan 18mm]].
2. **Low KV:** Dirancang untuk torsi pada putaran sangat rendah. Lilitannya rapat dan resistansinya tinggi, sehingga driver motor tidak mudah kepanasan.
3. **Smooth (Low Cogging):** Pergerakan sangat halus, memungkinkan implementasi haptic feedback/detent tiruan yang terasa premium.

## Spesifikasi Terkait Firmware
- **Pole Pairs:** Umumnya dikonfigurasi sebagai 12N14P (14 magnet pole). Artinya **`REEL_POLE_PAIRS` bernilai 7** di dalam kode program (14 dibagi 2).

## Sensor Pasangan
Motor ini dikawinkan dengan sensor sudut absolut magnetik **MT6701** (komunikasi via I2C, resolusi 14-bit). Magnet harus dipasang tepat di tengah poros motor agar *Field Oriented Control* (FOC) dapat beroperasi.

Kembali ke: [[TP7 - BOM (Bill of Materials)]]