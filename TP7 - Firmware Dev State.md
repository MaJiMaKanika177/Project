# TP7 - Firmware Dev State

Terakhir update: 2026-09-27. Status firmware `v0.1.0` di `~/Downloads/Project-main/tp7-player`. Terkait: [[TP7 - Arsitektur Kode]], [[TP7 - Memori dan Software]], [[TP7 - Wiring dan Pinout]]

## Status sekarang: BOOT STABIL ✅

Boot clean sampai `[Boot] Ready — N tracks`, serial format: `[Src] E:/W: pesan` lewat error logger.

## Fix history (boot-loop TASK_WDT → PANIC → stabil)

1. **TASK_WDT** — setup() di loopTask (Core1, WDT 5s) diblokir init >5s. Penyebab: `LittleFS.begin(true)`/`SPIFFS.begin(true)` format-on-mount (10-30s, no yield). Fix: `begin(false)` + format manual, dan **TWDT mati total**: `esp_task_wdt_delete` untuk loopTask + idle task kedua core di awal setup(), tanpa re-subscribe.
2. **PANIC deterministik di scan SD** — dua bug FatFs:
   - Dua dir handle open bareng → semua subdir open gagal (`ROOT OPEN FAILED`). Fix: kumpulin nama subdir, `root.close()`, baru rekursi. Tanpa trailing slash.
   - `/Android` (bekas kartu HP) punya LFN rusak → crash `openNextFile()` di entry ke-3. Fix: skip junk dirs by prefix (`android`, `system volume information`, `$recycle.bin`, `lost.dir`, `.trash*` dkk) — musik player gak butuh.
   - Stack overflow rekursi `subDirs[8][256]`×depth → `-DCONFIG_ARDUINO_LOOP_STACK_SIZE=16384`.
3. **SD mount gagal pasca-crash** — kartu stuck mid-I/O. Fix: 3× retry + `SD.end()` + 200ms + SPI re-init.
4. **UI beku "No track"** — TWDT mati = stack overflow task jadi silent death (no panic). UI task 8K→16K, audio 16K, lainnya 8K.

## Error Logger (fitur wajib per fitur baru)

`include/errorlog.h` + `src/system/errorlog.cpp`. API `Log::error/warn(src, fmt, ...)`.
- 3 jalur: Serial, ring RAM 24×128B → `GET /api/errors`, persist `/errors.log` di LittleFS (replay tail saat boot berikutnya — crash-loop terdiagnosa tanpa kabel).
- Init: `Log::init()` setelah Serial up, `Log::attachFs()` setelah LittleFS mount.
- Badge `!N` merah kiri-atas display = jumlah E-level boot ini.
- Konvensi: `E` = fitur mati/partial failure; `W` = degradasi normal/fallback; file absen = bukan error.
- **Aturan user: SETIAP fitur baru wajib ada error logger di semua failure path.**

Terpasang di: SD, Player, Playlist, LittleFS, Art, Input(MCP), Reel(AS5600/DRV8313/FOC), WiFi, Upload, Boot(panic marker).

## UI Display (GC9A01 240px bulat)

Now Playing **statis** (bukan reel animasi — sesuai request): album art disc `ART_D=112` persis tengah, ring abu-abu `C_DIM` radius aR+15 tebal 10px, progress **hitam** menutupi ring clockwise dari jam 12 (`fillArc`, start 90°, end 90−360·prog). Judul di y=32, vol/format di y=206 — semua di dalam chord lingkaran. Render di-throttle: redraw hanya saat progress naik ≥0.5% (hemat SPI/CPU karena statis).

**Album art:** sidecar file, urutan cari `<nama>.jpg` → `<nama>.png` → `<folder>/cover.jpg` → `cover.png`. Decode via LovyanGFX `drawJpgFile`/`drawPngFile` (decoder sudah built-in, no dep baru), ke sprite PSRAM, mask `fillArc`, push dengan black=transparent. ID3 APIC belum (upgrade path: AudioFileSourceID3 + lgfx TJpg).

## Web UI (single-page app)

Serve dari `APP_HTML` PROGMEM di `web_server.cpp`, di `/` dan `/upload`. 3 kartu: Player (status poll 2s, pause/stop, seek bar, vol slider, error banner dari /api/errors), Library (browse + play via /api/*), Upload.
- Upload validasi server-side: musik `.mp3/.wav/.flac` saja; cover `.jpg/.jpeg/.png` (cover.* atau <songname>.*). Ekstensi lain → chunk dibuang, 400 + alasan, tercatat `W Upload: rejected`.
- `beginResponse_P` biar HTML gak makan RAM.

## Build & Flash (kerja di mesin ini)

- Build: `~/.platformio/penv/bin/platformio run` (~21s incremental)
- Upload butuh download mode: hold BOOT, tap RESET, release BOOT → `platformio run -t upload --upload-port /dev/ttyACM1`. User upload sendiri via PIO biasanya.
- Serial: user prefer Arduino IDE (PIO lemot); `/dev/ttyACM1`, 115200. Port busy = Arduino serial-monitor respawn, kill by PID spesifik (jangan pkill -f).
- `cat /dev/ttyACM1` kehilangan panic dump (USB CDC re-enumerate saat reboot) → andalkan error logger + `/api/errors`, atau scrollback Arduino IDE.
- Partitions `huge_app.csv` (punya slot coredump, aktif tapi decoder belum dipakai). TWDT off permanen — keputusan sadar buat dev board.
- RAM 20.3%, Flash ~40%.

## Hardware belum kepasang (W-level di log = normal)

- `MCP23017` belum ke-probe @0x20 (Wire0 GPIO8/9) → tombol disabled
- `AS5600` belum di Wire1 (GPIO1/2) → motor reel disabled
- Battery ADC GPIO18 baca 0.7–1.7V tidak stabil — cek divider / TP4056

## Known-good test artifacts

- `test_cover.png` (gradient 140×140) di root repo tp7-player — generator PNG stdlib ada di riwayat sesi.
- Kartu SD 30GB bekas Android = kasus uji crash LFN.
