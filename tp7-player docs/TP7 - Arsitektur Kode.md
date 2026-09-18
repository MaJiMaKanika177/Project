---
tags:
  - software
  - cpp
  - architecture
---
# TP7 - Arsitektur Kode

Penjelasan struktur folder dan fungsi kode sumber proyek [[TP7 - Player Project Overview]].

## Struktur Utama
Basis kode terbagi dalam modul independen. Semua disatukan via FreeRTOS *tasks* di `src/main.cpp`.

### 1. Inti & Konfigurasi (`/include`)
- **`config.h`**: Konstanta sistem (resolusi layar, kecepatan motor, batas voltase).
- **`pins.h`**: Definisi GPIO. Sinkron dengan [[TP7 - Wiring dan Pinout]].
- **`util.h`**: Fungsi utilitas bersama. Contoh: `isAudioFile()` untuk cek ekstensi file.

### 2. Driver Perangkat Keras (`/src/drivers`)
Akses langsung ke fisik.
- **`display.cpp`**: Konfigurasi LovyanGFX untuk layar GC9A01. Alokasi memori *sprite* 240x240 di PSRAM.
- **`reel_motor.cpp`**: Logika SimpleFOC. Baca sensor MT6701. Atur motor [[Motor BLDC 2204]]. Dua mode: `SPIN` (putar otomatis saat play) dan `DETENT` (haptic *klik-klik* seperti kenop fisik).
- **`battery.cpp`**: Baca ADC GPIO18 dari sirkuit pembagi tegangan (lihat [[TP7 - Wiring dan Pinout]]). Konversi ke persentase.

### 3. Pemrosesan Audio (`/src/audio`)
- **`player.cpp`**: Otak pemutar musik. Pakai `ESP8266Audio`. Dekode MP3/WAV/FLAC. Tulis *stream* PCM ke [[PCM5102A DAC]] via I2S. Punya *Mutex* (`xSemaphoreCreateMutex`) agar aman dipanggil dari *thread* UI/Input dan Web.
- **`eq.cpp`**: Equalizer 10-band. Manipulasi sinyal digital sebelum masuk DAC.

### 4. Input & Antarmuka (`/src/input` & `/src/ui`)
- **`input_handler.cpp`**: Polling tombol via I2C MCP23017. Baca *delta detent* dari `reel_motor.cpp`. Kirim *command* (Play, Next, Vol Up) ke sistem.
- **`ui.cpp`**: *State machine* layar (Now Playing, Menu, Settings). Semua gambar digambar di memori lalu di-*push* ke layar.
    - **Rotasi Sinkron:** Fungsi `drawReel()` baca sudut aktual motor FOC, lalu gambar garis (spoke) roda miring sesuai sudut fisik motor.

### 5. Manajemen File & Jaringan (`/src/playlist` & `/src/wifi`)
- **`playlist.cpp`**: *Scan* SD Card. Simpan daftar lagu pakai pointer di PSRAM (`ps_calloc`). Hindari RAM internal penuh (lihat [[TP7 - Memori dan Software]]).
- **`web_server.cpp`**: *ESPAsyncWebServer*. Sedia API REST (JSON) dan antarmuka web. Bisa kontrol *play/pause* dan unggah file via WiFi AP `TP7-Player`.

## Hubungan Antar Modul (Alur Kerja)
1. Motor diputar user -> `reel_motor` ubah sudut.
2. `input_handler` deteksi perubahan sudut -> kirim sinyal navigasi menu.
3. `ui` terima sinyal -> ganti layar -> render ulang.
4. User tekan "Play" -> `player` jalankan dekoder audio -> kirim I2S ke DAC.
5. Saat lagu jalan -> `ui` perintah `reel_motor` masuk mode `SPIN` (putar otomatis).
6. Saat lagu jeda -> `ui` perintah `reel_motor` masuk mode `DETENT` (jadi kenop volume).