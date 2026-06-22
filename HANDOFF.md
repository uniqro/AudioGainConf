# AudioDspConfig — Başka Bilgisayarda Devam Kılavuzu

> **Dal:** `master`  
> **Son commit:** `feat: CMD_GET_META (0x06) tamamlandı`  
> **Tarih:** 2026-06-22

---

## 1. Proje Özeti

VxWorks/MPC866 hedefli gömülü ses DSP yapılandırma sisteminin **PC'de test ortamı**:

| Bileşen | Dil | Konum | Açıklama |
| ------- | --- | ----- | -------- |
| Gömülü sunucu (PC port) | C (MSVC/MinGW) | `AudioConf/` + `pc_port/` | TCP 5558, 6 komut işler |
| Python UI istemcisi | Python 3 + PyQt5 | `pc-ui-pyqt5/` | Parametreleri gönderir |
| Derlenmiş EXE | PyInstaller | `dist/AudioDSPConfig.exe` | Konsol yok, uçak ikonu |

---

## 2. Çalışan Özellikler

| Özellik | Durum |
| ------- | ----- |
| STATUS — 6 mod × 15 param okuma | ✅ |
| SET_GAIN_BLOCK — tek mod yazma (APPLY) | ✅ |
| APPLY ALL MODES — 6 mod sıralı yazma | ✅ |
| VERIFY — flash CRC32 doğrulama | ✅ |
| SET_META — header/version kalıcı flash yazma | ✅ |
| Startup apply — açılışta flash→DSP uygulama | ✅ |
| Upload to Radio — .bin dosyasını cihaza yükle | ✅ |
| Download from Radio — cihazdan indir + kaydet | ✅ |
| Export Params — UI'dan .bin dosyasına kaydet | ✅ |
| Import Params — .bin dosyasını UI'a yükle | ✅ |
| READ FROM RADIO — parametreleri UI'a çek | ✅ |
| Timestamp korunması (upload→download döngüsü) | ✅ |
| GET_META — bağlantıda header_id okunması | ✅ |

---

## 3. Dosya Haritası

```text
AudioDspConfig/
├── AudioConf/                    ← Orijinal gömülü C kodu
│   ├── AudioConfigTask.h/c       ← TCP sunucu döngüsü (AC_STATE_* state machine)
│   ├── AudioConfigHandler.h/c    ← Komut handler'ları + send_response()
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
# VS cmake yolu (Visual Studio 18 Community):
$cmake = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$vcvars = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cmd /c "`"$vcvars`" && `"$cmake`" --build build\msvc-debug --config Debug"
# Çıktı: build/msvc-debug/audio_dsp_server.exe
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
  STATUS         [0x01]                                    1 byte
  SET_PARAM      [0x02][mode][param_idx][value]            4 byte
  SET_GAIN_BLOCK [0x03][mode][gain×15]                    17 byte
  VERIFY         [0x04]                                    1 byte
  SET_META       [0x05][header_id:40][version:30]         71 byte
  GET_META       [0x06]                                    1 byte

Sunucu → Python (her komuta):
  [cmd|0x80][status(0=OK)][error_code][data[128]] = 131 byte

STATUS yanıtı data alanı:
  [0..89]   = 6 mod × 15 param (90 byte gain verileri)
  [90..119] = version[30] flash'ta saklanan

GET_META yanıtı data alanı (planlanıyor):
  [0..39]  = header_id[40]
  [40..69] = version[30]

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
# APPLY ALL MODES / Upload / Download: komutlar yanıt beklemeden gönderilmez
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

### set_version_parts / set_version_preview Sırası (ÖNEMLİ)

```python
# set_version_parts() → _refresh_version_preview() çağırır → timestamp'i siler!
# DOĞRU SIRA: önce parts, sonra preview
self.file_ops.set_version_parts(major, minor, fix)   # spinbox'ları set et
self.file_ops.set_version_preview(version_str)        # timestamp'i geri yaz
```

---

## 8. CMD_GET_META (0x06) — TAMAMLANDI

Bağlantı kurulduğunda, Read From Radio ve Download sırasında cihazdan
`header_id[40]` + `version[30]` okunur. STATUS'tan önce sıralı gönderilir.

---

## 9. Kalan Sorunlar

| # | Dosya | Sorun | Önem |
| - | ----- | ----- | ---- |
| 1 | `AudioFlashMgr.c:130-134` | CRC offset yanlış: hardcoded 141, olması gereken 161 | MAJOR |
| 2 | `AudioConfigTask.c:27` | `int` alana `NULL` atanıyor (C uyarısı) | MINOR |

---

## 10. Git Geçmişi

```text
de4acea  fix: Download from Radio — timestamp korunuyor, zaman güncellenmiyor
abee029  docs: HANDOFF güncelleme
0314f28  feat: startup'ta flash okunarak DSP'ye uygulanıyor
4c53870  feat: CMD_SET_META (0x05) — header/version kalıcı olarak flash'a yazılıyor
b0a6927  feat: ikonu güncelle, konsol kapat, HANDOFF yenile
f359ce9  fix: Export — upload/import'tan gelen timestamp korunur
1d566bb  fix: Download from Radio — QTimer.singleShot ile dialog ertele
72abc8f  feat: Download from Radio — parametreleri indir ve dosyaya kaydet
```

---

## 11. Yeni Bilgisayarda İlk Adımlar

```powershell
# 1. Repo klon
git clone <repo-url> AudioDspConfig
cd AudioDspConfig

# 2. Python bağımlılıkları
pip install pyqt5 pyinstaller

# 3. C sunucuyu derle (MSVC)
# VS cmake yolunu bul ve yukarıdaki build komutunu çalıştır

# 4. EXE hazır: dist/AudioDSPConfig.exe
# veya doğrudan çalıştır:
cd pc-ui-pyqt5
python main.py
```
