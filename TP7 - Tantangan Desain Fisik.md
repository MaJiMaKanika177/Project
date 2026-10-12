---
tags:
  - hardware
  - form-factor
---
# TP7 - Tantangan Desain Fisik

Membangun [[TP7 - Player Project Overview]] menjadi format genggam (*handheld*) memiliki kesulitan tersendiri, terutama karena target dimensi ketat: **100mm x 70mm x 18mm**.

## Analisis Ketebalan (Sumbu Z)
Batas 18mm adalah tantangan terberat. 
- [[Motor BLDC 2204]] tebalnya ~10-14mm.
- Baterai LiPo 1200mAh tebalnya ~5-6mm.
Jika motor dan baterai ditumpuk, total ketebalan fisik akan menembus angka 20mm+ (ditambah PCB dan dinding *casing*).
- **Solusi:** Baterai dan motor harus diletakkan bersebelahan di sumbu X/Y.

## Isu Modul "Breakout Board"
Menggunakan modul jadi yang dibeli terpisah sangat memakan ruang vertikal.
- Pin header (konektor plastik hitam) menambah tinggi 8-15mm.
- **Solusi:** Cabut seluruh pin header plastik. Gunakan metode *Direct Wiring* (solder kabel pipih berlapis enamel/AWG30 langsung dari *pad* ke *pad* modul).

## Integrasi Sensor Putar
Sensor sudut absolut MT6701 butuh membaca fluks magnet secara presisi.
- Poros (shaft) [[Motor BLDC 2204]] perlu dipastikan tidak tertutup total, sehingga sebuah magnet diametrik kecil bisa ditempelkan padanya. Sensor MT6701 wajib diletakkan sejajar persis menghadap magnet dengan jarak 1mm.

Jika ingin menekan ketebalan secara mutlak persis 18mm, perakitan model ini pada akhirnya menuntut pembuatan *Custom PCB* di masa depan.