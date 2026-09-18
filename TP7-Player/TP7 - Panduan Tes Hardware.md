---
tags:
  - testing
  - hardware
  - tp7-player
  - guide
date: 2026-09-11
---
# TP7 - Panduan Tes Hardware (Step-by-Step)

Panduan menghubungkan dan menguji modul satu per satu ke ESP32-S3.
Referensi pin: [[TP7 - Wiring dan Pinout]]
Referensi komponen: [[TP7 - BOM (Bill of Materials)]]
Referensi kode: [[TP7 - Arsitektur Kode]]

> ⚠️ **PENTING:** Selalu cabut USB sebelum menyambung/melepas kabel. Jangan solder saat ESP32 hidup.

---

## Tahap 0: Flash Firmware ke ESP32-S3

### Persiapan
1. Buka folder project `tp7-player` di VSCode.
2. Colok ESP32-S3 via kabel USB-C (pastikan kabel **data**, bukan kabel charger doang).

### Upload Firmware
- Klik ikon **→ (panah kanan)** di toolbar bawah VSCode (status bar PlatformIO).
- Atau tekan **Ctrl+Alt+U**.
- Atau buka terminal PlatformIO: klik ikon 🐜 di sidebar kiri → "Upload".

### Kalau Port Tidak Terdeteksi
- Windows: buka Device Manager → Ports (COM & LPT). Harusnya muncul `USB-SERIAL CH340` atau `CP2102` atau `USB JTAG/serial debug unit`.
- Kalau tidak muncul: install driver CH340 atau CP2102 (tergantung board).
- ESP32-S3 DevKit bawaan biasanya pakai USB native (tidak butuh driver tambahan di Windows 10/11).
- Kalau tetap tidak muncul: coba kabel USB lain. Banyak kabel USB-C yang cuma bisa charge.

### Buka Serial Monitor
- Klik ikon **🔌 (colokan)** di toolbar bawah VSCode.
- Atau tekan **Ctrl+Alt+S** (di beberapa versi).
- Atau PlatformIO sidebar → "Monitor".
- Baud rate harus **115200** (sudah di-set di `platformio.ini`).

### ✅ Sukses Jika
- Terminal menampilkan output `TP7-Player v0.1.0 booting...`

---

## Tahap 1: SD Card (SPI)

### Wiring
| Modul SD | ESP32-S3 |
|----------|----------|
| SCK      | GPIO10   |
| MOSI     | GPIO11   |
| MISO     | GPIO12   |
| CS       | GPIO13   |
| VCC      | 3V3      |
| GND      | GND      |

### Cara Tes
1. Format microSD sebagai **FAT32** (bukan exFAT).
2. Taruh 1-2 file MP3 di root SD. Nama file pendek, tanpa spasi/karakter aneh. Contoh: `test.mp3`, `song2.mp3`.
3. Colok SD ke modul, sambung kabel sesuai tabel.
4. Flash firmware: buka terminal PlatformIO → `pio run -t upload`.
5. Buka Serial Monitor (`pio device monitor`).

### Output Serial yang Diharapkan
```
TP7-Player v0.1.0 booting...
[SD] Mounted. Size: XXXX MB
[Playlist] Scanned 2 tracks                     
```

### Kalau Gagal
- `[SD] Mount failed` → Cek kabel MISO/MOSI tidak kebalik. Cek format FAT32. Coba SD card lain.
- Tidak ada output sama sekali → Cek USB, cek board dipilih benar di `platformio.ini`.

### ✅ Sukses Jika
- Serial print `[SD] Mounted` + jumlah tracks benar.

---

## Tahap 2: Display GC9A01 (SPI)

### Wiring
| Modul GC9A01 | ESP32-S3 |
|--------------|----------|
| SCK (SCL)    | GPIO42   |
| SDA (MOSI)   | GPIO41   |
| CS           | GPIO38   |
| DC           | GPIO39   |
| RST (RES)    | GPIO40   |
| BLK (BL)     | GPIO47   |
| VCC          | 3V3      |
| GND          | GND      |

> ⚠️ Label di modul GC9A01 sering **beda-beda** antar penjual. Yang penting: pin data (SDA) = MOSI, pin clock (SCL) = SCK.

### Cara Tes
1. Sambung kabel sesuai tabel. SD card boleh tetap terpasang.
2. Flash firmware & buka Serial Monitor.

### Output Serial yang Diharapkan
```
[Display] — (tidak ada error)
```

### Output Visual yang Diharapkan
Layar menyala, tampil splash screen:
- Latar hitam
- Tulisan **"TP-7"** putih besar di tengah
- Versi **"0.1.0"** abu-abu di bawah
- Lingkaran tipis di pinggir

Setelah 1.5 detik, pindah ke layar **Now Playing** (teks "No track", lingkaran spoke reel).

### Kalau Gagal
- Layar putih/blank → Cek pin DC dan RST tidak kebalik.
- Layar hidup tapi gambar corrupt/warna salah → Cek pin SCK dan SDA/MOSI.
- Layar gelap total → Cek pin BLK (backlight) tersambung ke GPIO47.
- `[Display] GC9A01 init failed` → Cek semua pin, pastikan VCC 3.3V (BUKAN 5V).

### ✅ Sukses Jika
- Splash screen "TP-7" muncul → lalu pindah ke Now Playing screen.

---

## Tahap 3: DAC PCM5102A (I2S) + Audio

### Wiring
| Modul PCM5102A | ESP32-S3 |
|----------------|----------|
| BCK (BCLK)     | GPIO4    |
| LCK (LRCK/WS)  | GPIO5    |
| DIN (DATA)      | GPIO6    |
| SCK             | **GND** ⚠️ |
| VCC (VIN)       | **5V** (atau 3V3, cek modul) |
| GND             | GND      |

> ⚠️ **KRITIS:** Pin **SCK** pada modul PCM5102A **WAJIB disambung ke GND**. Kalau floating → tidak ada suara atau kresek. Beberapa modul sudah ada jumper solder di bagian belakang (cek pad bertuliskan "L" dekat SCK, solder bridging ke L/LOW).

### Cara Tes
1. Sambung kabel sesuai tabel. SD card harus terpasang dengan file MP3.
2. Colok earphone/headphone ke jack 3.5mm pada modul PCM5102A.
3. Flash firmware & buka Serial Monitor.
4. **Navigasi ke file MP3:** Tanpa tombol MCP23017, gunakan **Web UI**:
   - Serial Monitor akan print: `[WiFi] AP started: TP7-Player / IP: 192.168.4.1`
   - Sambung HP/laptop ke WiFi **TP7-Player** (password: `tp7player`).
   - Buka browser: `http://192.168.4.1/api/files` → harusnya tampil JSON list file.
   - Play via curl atau browser: `http://192.168.4.1/api/play` (POST, param `path=/test.mp3`).
   - Atau cara cepat — langsung test via Serial: modifikasi `setup()` sementara (lihat catatan di bawah).

#### Cara Cepat Test Audio (Tanpa WiFi)
Tambahkan baris ini **sementara** di akhir `setup()` di `main.cpp`:
```cpp
player.play("/test.mp3");
```
Flash ulang. Kalau file ada di SD, langsung putar otomatis saat boot.

### Output Serial yang Diharapkan
```
[Player] Initialized
[Player] Playing: /test.mp3
```

### Kalau Gagal
- Tidak ada suara → Cek pin SCK modul sudah di-GND. Cek earphone. Cek BCK/LCK/DIN.
- Suara kresek/noise → SCK floating. Solder pad SCK ke GND.
- `[Player] Can't open: /test.mp3` → Nama file salah atau SD tidak mount.
- Suara pecah/distorsi → Cek volume tidak terlalu tinggi (default 50%).

### ✅ Sukses Jika
- Musik terdengar jernih di earphone. Tidak ada kresek.

---

## Tahap 4: MCP23017 (I2C Expander — Tombol)

### Wiring
| MCP23017 Pin | Sambung ke |
|-------------|------------|
| SDA (pin 13)| GPIO8 + **Resistor 4.7kΩ ke 3V3** |
| SCL (pin 12)| GPIO9 + **Resistor 4.7kΩ ke 3V3** |
| VDD (pin 9) | 3V3 |
| VSS (pin 10)| GND |
| A0 (pin 15) | GND (alamat = 0x20) |
| A1 (pin 16) | GND |
| A2 (pin 17) | GND |
| RESET (pin 18) | 3V3 (active low, harus HIGH) |
| INTA (pin 20) | GPIO7 |
| GPA0 (pin 21) | Tombol PLAY → GND |
| GPA1 (pin 22) | Tombol MEMO → GND |
| GPA2 (pin 23) | Tombol MENU → GND |
| GPA3 (pin 24) | Tombol BACK → GND |
| GPA4 (pin 25) | Rocker UP → GND |
| GPA5 (pin 26) | Rocker DOWN → GND |
| GPA6 (pin 27) | LED Merah (+ resistor 330Ω ke GND) |

> Tombol tactile: satu kaki ke pin GPA, kaki lain ke GND. Internal pull-up aktif di firmware.

### Cara Tes
1. Sambung MCP23017 sesuai tabel. Minimal pasang 1 tombol (misal PLAY di GPA0).
2. Flash firmware & buka Serial Monitor.

### Output Serial yang Diharapkan
```
[Input] Initialized — MCP23017 + BLDC reel
```

### Tes Interaktif
- Tekan tombol PLAY → di layar, status harus berubah (play/pause).
- Tekan MENU → layar pindah ke File Browser.
- Tekan BACK → layar kembali.
- Kalau audio sudah jalan, tekan PLAY → musik pause/resume.

### Kalau Gagal
- `[Input] MCP23017 not found!` → Cek SDA/SCL. Cek A0/A1/A2 semua GND (alamat 0x20). Cek pull-up resistor 4.7kΩ. Cek pin RESET ke 3V3.
- Tombol tidak responsif → Cek kabel tombol ke GND. Cek nomor pin GPA benar.

### ✅ Sukses Jika
- Serial print `Initialized`. Tombol navigasi layar dan kontrol audio.

---

## Tahap 5: Battery ADC Reading

### Wiring
```
VBAT (3.7-4.2V) ──[ R1 = 100kΩ ]──┬── GPIO18
                                    │
                              [ R2 = 100kΩ ]
                                    │
                                   GND
```

> Pembagi tegangan 2:1. VBAT 4.2V → GPIO18 baca 2.1V (aman untuk ADC ESP32, max 3.3V).

### Cara Tes (Tanpa Baterai)
Jika belum ada baterai, **simulasi** dengan mencolok kabel jumper:
- GPIO18 ke **3V3** → pembagi tegangan baca ~1.65V → firmware kalkulasi ~3.3V → persen rendah.
- GPIO18 ke **GND** → 0V → persen 0%.

Atau kalau sudah ada baterai LiPo + TP4056:
- Sambung VBAT (output TP4056) ke R1. Rangkaian pembagi ke GPIO18.

### Output Serial yang Diharapkan
```
[Battery] ADC on GPIO18, initial X.XXV
```

### Cek di Layar
Pojok atas layar Now Playing, akan muncul angka persentase baterai (contoh: `78%`).

### Kalau Gagal
- Persen selalu 0% atau 100% → Cek resistor, cek GPIO18 benar tersambung ke titik tengah pembagi.
- Nilai lompat-lompat drastis → Normal sedikit (sudah ada averaging), tapi kalau lebih dari ±5% → cek solderan, kabel terlalu panjang, atau noise dari motor.

### ✅ Sukses Jika
- Serial print `initial X.XXV` dengan nilai masuk akal. Layar tampilkan persen baterai.

---

## Tahap 6: Motor BLDC 2204 + AS5600 + SimpleFOC Mini

> ⚠️ **INI TAHAP PALING SUSAH.** Baca semua catatan sebelum mulai.

### Wiring Motor → SimpleFOC Mini
| SimpleFOC Mini | Sambung ke |
|---------------|------------|
| IN1           | GPIO14     |
| IN2           | GPIO15     |
| IN3           | GPIO16     |
| EN (Enable)   | GPIO21     |
| VM            | **5V** (atau VBAT lewat step-up jika pakai baterai) |
| GND           | GND        |
| M1, M2, M3    | 3 kabel fasa motor (urutan bisa acak, nanti tuning) |

### Wiring AS5600
| AS5600 | ESP32-S3 |
|--------|----------|
| SDA    | GPIO1    |
| SCL    | GPIO2    |
| VCC    | 3V3      |
| GND    | GND      |
| DIR    | GND atau NC (arah default CW) |

> ⚠️ **Magnet:** AS5600 butuh magnet **diametrik** (bukan aksial) ditempel di ujung poros (shaft) motor, **sejajar tepat di atas chip sensor**. Jarak magnet ke chip: **0.5mm - 3mm**.

### Cara Tes
1. Sambung semua kabel. Pastikan motor tidak terhalang secara mekanis.
2. Flash firmware & buka Serial Monitor.

### Output Serial yang Diharapkan (SUKSES)
```
[Reel] BLDC + AS5600 ready
[Boot] Ready — X tracks, http://...
```

### Output Serial yang Diharapkan (GAGAL — umum terjadi)
```
[Reel] AS5600 not found on Wire1 — motor disabled
```
→ Sensor tidak terdeteksi. Cek SDA/SCL. Cek power 3.3V.

```
[Reel] initFOC failed (check phase wiring / pole pairs)
```
→ **Ini SANGAT UMUM.** Artinya:
1. Urutan kabel fasa motor (M1/M2/M3) salah → **Tukar dua kabel fasa** mana saja, flash ulang, coba lagi. Ada 6 kombinasi, 2 yang benar.
2. `REEL_POLE_PAIRS` salah → Buka `config.h`, ganti nilai. Motor 2204 biasanya 7, tapi bisa 11 atau 14. Cara hitung: putar rotor pelan satu putaran penuh dengan tangan, hitung berapa kali terasa "menempel" (cogging point), bagi 2.
3. Magnet tidak terdeteksi AS5600 → Terlalu jauh, atau magnet tipe aksial (salah). Harus **diametrik**.

### Cara Debug Motor
Tambahkan sementara di `setup()` setelah `ReelMotor::init()`:
```cpp
// Test: cetak sudut sensor tiap 500ms
// Hapus setelah tes!
while(true) {
    Serial.printf("Angle: %.1f deg\n", ReelMotor::getAngleDeg());
    delay(500);
}
```
Putar rotor dengan tangan → sudut harus berubah 0°-360° smooth. Kalau nilainya lompat atau stuck → masalah magnet/sensor.

### Tuning PID (Kalau Motor Bergetar/Kasar)
Buka `reel_motor.cpp`, ubah:
```cpp
motor.PID_velocity.P = 0.25f;  // turunkan jadi 0.1f kalau getar
motor.PID_velocity.I = 2.0f;   // turunkan jadi 1.0f kalau overshoot
motor.LPF_velocity.Tf = 0.02f; // naikkan jadi 0.05f kalau noisy
```

### ✅ Sukses Jika
- Serial print `BLDC + AS5600 ready`.
- Motor bisa ditahan dan diputar tangan (ada resistensi lembut / haptic detent).
- Layar menampilkan spoke reel yang berputar sesuai putaran motor fisik.

---

## Urutan Tes Selesai — Selanjutnya

Setelah semua 6 tahap lolos:
1. Lepas kode test sementara (`player.play(...)`, loop debug angle).
2. Rakit semua modul bersama.
3. Tes integrasi: putar motor → navigasi menu → pilih lagu → play → suara keluar.
4. Tes EQ: masuk layar EQ, putar knob, dengar perubahan.
5. Tes WiFi: sambung HP, buka web UI, kontrol dari browser.
6. [[TP7 - Tantangan Desain Fisik|Mulai pikirkan casing]].

---

*Catatan: Panduan ini dibuat berdasarkan firmware v0.1.0. Lihat [[TP7 - Arsitektur Kode]] untuk detail kode.*