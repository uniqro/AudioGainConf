# AudioDspConfig — Başka Bilgisayarda Devam Kılavuzu

> **Dal:** `master`  
> **Son commit:** `f359ce9` (+ build commit bekliyor)  
> **Tarih:** 2026-06-22

---

## 1. Proje Özeti

VxWorks/MPC866 hedefli gömülü ses DSP yapılandırma sisteminin **PC'de test ortamı**:

| Bileşen | Dil | Konum | Açıklama |
| ------- | --- | ----- | -------- |
| Gömülü sunucu (PC port) | C (MSVC/MinGW) | `AudioConf/` + `pc_port/` | TCP 5558, 4 komut işler |
| Python UI istemcisi | Python 3 + PyQt5 | `pc-ui-pyqt5/` | Parametreleri gönderir |
| Derlenmiş EXE | PyInstaller | `dist/AudioDSPConfig.exe` | Konsol yok, uçak ikonu |

---

## 2. Çalışan Özellikler (Tümü Test Edildi)

| Özellik | Durum |
| ------- | ----- |
| STATUS — 6 mod × 15 param okuma | ✅ |
| SET_GAIN_BLOCK — tek mod yazma (APPLY) | ✅ |
| APPLY ALL MODES — 6 mod sıralı yazma | ✅ |
| VERIFY — flash CRC32 doğrulama | ✅ |
| Upload to Radio — .bin dosyasını cihaza yükle | ✅ |
| Download from Radio — cihazdan indir + kaydet | ✅ |
| Export Params — UI'dan .bin dosyasına kaydet | ✅ |
| Import Params — .bin dosyasını UI'a yükle | ✅ |
| READ FROM RADIO — parametreleri UI'a çek | ✅ |
| Timestamp korunması (upload→download döngüsü) | ✅ |

---

## 3. Dosya Haritası

```text
AudioDspConfig/
├── AudioConf/                    ← Orijinal gömülü C kodu
│   ├── AudioConfigTask.h/c       ← TCP sunucu döngüsü (AC_STATE_* state machine)
│   ├── AudioConfigHandler.h/c    ← 4 komut handler'ı + send_response()
│   ├── AudioParamDef.h/c         ← Parametre tanımları
│   ├── AudioFlashMgr.h/c         ← Flash okuma/yazma
│   └── AudioSpiGateway.h/c       ← SPI gateway
│
├── pc_port/
│   ├── include/
│   │   ├── vxWorks.h             ← STATUS, OK, ERROR, uint8_t typedef + Windows SDK pack fix
│   │   ├── logLib.h              ← logMsg → printf stub
│   │   ├── sockLib.h             ← Winsock include + fioRead makrosu
│   │   ├── ioLib.h               ← fioRead → _pcport_fioRead_dispatch yönlendirmesi
│   │   ├── taskLib.h             ← taskSpawn, taskDelay, sysClkRateGet
│   │   └── ParameterExp.h        ← Flash API prototipleri
│   └── src/
│       ├── dispatch_glue.c       ← recv → handler dispatch (KRİTİK)
│       ├── flash_stubs.c         ← audio_flash.bin dosyasına R/W
│       └── tasklib_stubs.c       ← thread + taskDelay + sysClkRateGet
│
├── pc-ui-pyqt5/
│   ├── main.py                   ← Ana uygulama
│   ├── tcp_client.py             ← Non-blocking TCP, 50ms QTimer poll
│   ├── audio_protocol.py         ← Paket inşa/çözümleme
│   ├── build_exe.spec            ← PyInstaller; console=False, plane.png ikonu
│   ├── resources/
│   │   ├── plane.png             ← Uygulama ikonu (512×512)
│   │   ├── app_icon.ico          ← EXE ikonu (16/32/48/256 px, plane.png'den)
│   │   └── style.qss             ← Karanlık tema
│   └── ui/
│       ├── connection_panel.py
│       ├── param_editor.py
│       └── file_ops.py
│
├── dist/
│   └── AudioDSPConfig.exe        ← Hazır EXE (konsol yok, uçak ikonu)
│
└── CMakeLists.txt                ← MSVC için CMake build
```

---

## 4. Build Talimatları

### C Sunucu (MSVC — CMake)

```powershell
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
# Çıktı: build/Release/audio_dsp_server.exe
```

### C Sunucu (MinGW)

```bash
gcc -Wall -I pc_port/include -I . \
    AudioConf/AudioConf.c AudioConf/AudioConfigTask.c \
    AudioConf/AudioConfigHandler.c AudioConf/AudioParamDef.c \
    AudioConf/AudioSpiGateway.c AudioConf/AudioFlashMgr.c \
    pc_port/src/dispatch_glue.c pc_port/src/flash_stubs.c \
    pc_port/src/tasklib_stubs.c \
    -o audio_dsp_server.exe -lws2_32 -lpthread
```

### Python EXE (PyInstaller)

```powershell
cd pc-ui-pyqt5
pip install pyqt5 pyinstaller
pyinstaller --clean build_exe.spec
# Çıktı: dist/AudioDSPConfig.exe
```

### Python Doğrudan (debug için)

```powershell
cd pc-ui-pyqt5
python main.py
```

---

## 5. TCP Protokol Özeti

```text
Python → Sunucu:
  STATUS       [0x01]                              1 byte
  SET_PARAM    [0x02][mode][param_idx][value]      4 byte
  SET_GAIN_BLOCK [0x03][mode][gain×15]            17 byte
  VERIFY       [0x04]                              1 byte

Sunucu → Python (her komuta):
  [cmd|0x80][status(0=OK)][error_code][data[128]] = 131 byte

STATUS yanıtı data alanı:
  [0..89] = 6 mod × 15 param (90 byte gain verileri)

VERIFY yanıtı data alanı:
  [0..3]  = stored_crc32
  [4..7]  = computed_crc32
  [8]     = crc_match (1=OK)  → Python'da buf[11]
```

---

## 6. .bin Dosya Formatı (300 byte)

```text
Offset   0 : header_id[40]   "AUDIO_CFG_V1\0..." (null-padded)
Offset  40 : version[30]     "V01.00.00 22.06.2026 00:21:57\0..."
Offset  70 : set_count(1)    = 6
Offset  71 : sets[6×15]      90 byte gain parametreleri
Offset 161 : crc32(4)        CRC32(byte 0-160), big-endian
Offset 165 : padding[135]
Toplam     : 300 byte
```

---

## 7. Önemli Kod Notları

### Sıralı Komut Gönderimi (`main.py`)

```python
# APPLY ALL MODES / Upload: komutlar yanıt beklemeden gönderilmez
def _enqueue_sequential(self, cmd_bytes, response_type):
    self._pending_cmds.append((cmd_bytes, response_type))
    if not self._cmd_queue:
        self._fire_next_pending()   # sadece kuyruk boşsa gönder
```

### Download from Radio → Otomatik Export

```python
# _on_download: bayrağı set et
self._export_after_download = True
self._send_status_request()

# _process_status_response: yanıt gelince export dialog aç
if self._export_after_download:
    self._export_after_download = False
    QTimer.singleShot(0, self._on_export)   # event loop tick'inden sonra
```

### Timestamp Korunması

```python
# _on_export: version_preview'de gerçek tarih varsa onu kullan
preview_text = self.file_ops.get_version_preview()
if preview_text and '──' not in preview_text:
    # "V01.00.00 22.06.2026 00:21:57" → datetime nesnesine ayrıştır
    ts = parse_from_preview(preview_text)
else:
    ts = datetime.now()
data = build_config_file(..., timestamp=ts)
```

---

## 8. Kalan Sorunlar

| # | Dosya | Sorun | Önem |
| - | ----- | ----- | ---- |
| 1 | `AudioFlashMgr.c:130-134` | CRC offset yanlış: hardcoded 141, olması gereken 161 (`AUDIO_CFG_CRC32_COVERED_SIZE`) | MAJOR |
| 2 | `AudioConfigTask.c:27` | `int` alana `NULL` atanıyor (C uyarısı) | MINOR |
| 3 | `AudioConfigHandler.c` | Derleme-zamanı buffer boyut assertion eksik | MINOR |

---

## 9. Git Geçmişi

```text
f359ce9  fix: Export — upload/import'tan gelen timestamp korunur
1d566bb  fix: Download from Radio — QTimer.singleShot ile dialog ertele
72abc8f  feat: Download from Radio — parametreleri indir ve dosyaya kaydet
a35c28f  fix: Upload to Radio — build_upload_request yerine sıralı SET_GAIN_BLOCK
68c30ff  chore: pc-ui-pyqt5/param/ gitignore'a eklendi
ceafb6e  fix: stdout redirect kaldırıldı — [DBG] çıktısı görünür
0ffb422  fix: TCP_NODELAY + _RSP_SIZE=131 + console=True EXE
```

---

## 10. Yeni Bilgisayarda İlk Adımlar

```powershell
# 1. Repo klon
git clone <repo-url> AudioDspConfig
cd AudioDspConfig

# 2. Python bağımlılıkları
pip install pyqt5 pyinstaller

# 3. C sunucuyu derle
gcc -Wall -I pc_port/include -I . \
    AudioConf/AudioConf.c AudioConf/AudioConfigTask.c \
    AudioConf/AudioConfigHandler.c AudioConf/AudioParamDef.c \
    AudioConf/AudioSpiGateway.c AudioConf/AudioFlashMgr.c \
    pc_port/src/dispatch_glue.c pc_port/src/flash_stubs.c \
    pc_port/src/tasklib_stubs.c \
    -o audio_dsp_server.exe -lws2_32 -lpthread

# 4. EXE hazır: dist/AudioDSPConfig.exe
# veya doğrudan çalıştır:
cd pc-ui-pyqt5
python main.py
```
