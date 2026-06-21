# AudioDspConfig — Başka Bilgisayarda Devam Kılavuzu

> **Dal:** `master`  
> **Son commit:** `ceafb6e` — stdout redirect kaldırıldı  
> **Tarih:** 2026-06-22

---

## 1. Proje Özeti

VxWorks/MPC866 hedefli gömülü ses DSP yapılandırma sisteminin **PC'de test ortamı**:

| Bileşen | Dil | Konum | Açıklama |
|---------|-----|-------|----------|
| Gömülü sunucu (PC port) | C (MSVC/MinGW) | `AudioConf/` + `pc_port/` | TCP 5558, 4 komut işler |
| Python UI istemcisi | Python 3 + PyQt5 | `pc-ui-pyqt5/` | Parametreleri gönderir |
| Derlenmiş EXE | PyInstaller | `dist/AudioDSPConfig.exe` | Konsol pencereli |

**TCP komutları (tümü çalışıyor):**
- `STATUS` (0x01) — 6 mod × 15 parametre = 90 byte veriyi okur
- `SET_PARAM` (0x02) — tek parametre günceller
- `SET_GAIN_BLOCK` (0x03) — 1 modun 15 parametresini yazar
- `VERIFY` (0x04) — flash CRC32 doğrular

---

## 2. Mevcut Açık Bug: "APPLY ALL MODES" — 85 Byte Batch

### Belirti
Kullanıcı **APPLY ALL MODES** butonuna bastığında:
- Gömülü sunucu (C) tek `recv()` çağrısında **85 byte** alıyor (`5 × 17 = 85`)
- Sunucu yalnızca ilk komutu işliyor, geri kalanları **kaybediyor**
- Python UI "Timeout: no response from device" gösteriyor

### Neden 85 byte?
6 mod için `SET_GAIN_BLOCK` gönderilir. Her biri 17 byte. Sunucu 5'ini tek seferde alıyor demek ki Python yanıt beklemeden **birden fazla komut** gönderiyor.

### Yapılan Düzeltmeler (hâlâ çözülmedi)
1. **TCP_NODELAY** — `tcp_client.py:131`'de eklendi. Nagle algoritmasının küçük paketleri birleştirmesini engeller.
2. **Sıralı gönderim (sequential dispatch)** — `_enqueue_sequential` / `_fire_next_pending`. Bir komut yanıt alınmadan bir sonraki gönderilmez.
3. **`_RSP_SIZE = 131`** — Sunucu her yanıtta tam 131 byte yollar; artık tüm 131 byte tüketiliyor.
4. **stdout redirect kaldırıldı** — `[DBG]` çıktısı artık konsol penceresinde görünür.

### Kök Neden Henüz Bilinmiyor
Yeni EXE ile `[DBG]` çıktısı **ilk kez** görülebilecek. Tanı adımları: bölüm 5.

---

## 3. Dosya Haritası

```
AudioDspConfig/
├── AudioConf/                    ← Orijinal gömülü C kodu (mümkünse değiştirme)
│   ├── AudioConfigTask.h/c       ← TCP sunucu döngüsü (AC_STATE_* state machine)
│   ├── AudioConfigHandler.h/c    ← 4 komut handler'ı + send_response()
│   ├── AudioParamDef.h/c         ← Parametre tanımları
│   ├── AudioFlashMgr.h/c         ← Flash okuma/yazma (READ/WRITE stub'a map)
│   └── AudioSpiGateway.h/c       ← SPI (stub'a map)
│
├── pc_port/
│   ├── include/
│   │   ├── vxWorks.h             ← STATUS, OK, ERROR, uint8_t... typedef'leri
│   │   ├── logLib.h              ← logMsg → printf stub
│   │   ├── sockLib.h             ← fioRead → recv makrosu, Winsock include
│   │   ├── ioLib.h               ← fioRead makrosunu _pcport_fioRead_dispatch'e yönlendirir
│   │   ├── taskLib.h             ← taskSpawn, taskDelay, sysClkRateGet
│   │   ├── inetLib.h             ← bzero stub
│   │   └── ParameterExp.h        ← Flash API prototipleri
│   └── src/
│       ├── dispatch_glue.c       ← *** KRİTİK: recv → handler dispatch ***
│       ├── flash_stubs.c         ← audio_flash.bin dosyasına R/W
│       └── tasklib_stubs.c       ← pthread/thread + taskDelay + sysClkRateGet
│
├── pc-ui-pyqt5/
│   ├── main.py                   ← Ana uygulama; sıralı gönderim burada
│   ├── tcp_client.py             ← Non-blocking TCP, 50ms QTimer poll
│   ├── audio_protocol.py         ← Paket inşa/çözümleme fonksiyonları
│   └── build_exe.spec            ← PyInstaller; console=True
│
├── dist/
│   └── AudioDSPConfig.exe        ← Derlenmiş EXE (son build: 2026-06-22 00:01)
│
└── CMakeLists.txt                ← MSVC için CMake build (C sunucu)
```

---

## 4. Build Talimatları

### C Sunucu (MSVC — CMake)

```powershell
# Proje kökünde
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
# Çıktı: build/Release/audio_dsp_server.exe
```

### C Sunucu (MinGW)

```bash
# pc_port/src/dispatch_glue.c, flash_stubs.c, tasklib_stubs.c dahil
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
# Çıktı: dist/AudioDSPConfig.exe (konsol pencereli)
```

### Python Doğrudan Çalıştırma (debug için tercih edilen)

```powershell
cd pc-ui-pyqt5
python main.py
# [DBG] çıktısı doğrudan terminal'de görünür
```

---

## 5. 85-Byte Bug Tanısı — Adım Adım

### Adım 1: Yeni EXE ile [DBG] Çıktısını Al

`dist/AudioDSPConfig.exe`'yi çalıştır. Açılan **siyah konsol penceresi**nde:

1. Sunucuyu başlat (`audio_dsp_server.exe`)
2. Bağlan → STATUS otomatik gider
3. **APPLY ALL MODES** bas
4. Konsol penceresindeki çıktıyı not al

### Adım 2: Beklenen Normal Çıktı (sıralı gönderim çalışıyorsa)

```
[DBG] SEND SET_GAIN_BLOCK cmd=0x03 len=17  queue_before=[]  pending_left=6
[DBG] RECV 131 bytes  queue=['SET_GAIN_BLOCK']  pending=6
[DBG] SEND SET_GAIN_BLOCK cmd=0x03 len=17  queue_before=[]  pending_left=5
[DBG] RECV 131 bytes  queue=['SET_GAIN_BLOCK']  pending=5
... (6 mod + 1 VERIFY = 7 tur)
```

**Her SEND, bir RECV'den sonra gelmeli.**

### Adım 3: Bozuk Çıktı (hâlâ bug varsa)

```
[DBG] SEND SET_GAIN_BLOCK cmd=0x03 len=17  queue_before=[]  pending_left=6
[DBG] SEND SET_GAIN_BLOCK cmd=0x03 len=17  queue_before=[]  pending_left=5  ← YANLIŞ: RECV yok
```

Eğer böyle görünürse → `_fire_next_pending` çok erken çağrılıyor. `main.py`'deki `_on_data_received`'i incele.

### Adım 4: Sunucu Tarafı Log

Sunucu konsolunda (C tarafında) şunu ara:

```
[LOG] AudioConfig: Received 85 bytes from client 0   ← BUG
[LOG] AudioConfig: Received 17 bytes from client 0   ← NORMAL
```

---

## 6. Kritik Kod Konumları

### `pc-ui-pyqt5/main.py` — Sıralı Gönderim

```python
_RSP_SIZE = 131  # sizeof(AudioResponse) = 1+1+1+128

def _enqueue_sequential(self, cmd_bytes, response_type):
    self._pending_cmds.append((cmd_bytes, response_type))
    if not self._cmd_queue:          # sadece boşsa gönder
        self._fire_next_pending()

def _fire_next_pending(self):
    if self._pending_cmds and not self._cmd_queue:
        cmd_bytes, rtype = self._pending_cmds.popleft()
        print(f"[DBG] SEND {rtype} ...", flush=True)
        self._enqueue_and_send(cmd_bytes, rtype)
```

`_on_data_received` içinde her yanıt işlendikten sonra:
```python
del self._response_buffer[:_RSP_SIZE]   # tam 131 byte tüket
self._cmd_queue.popleft()
self._fire_next_pending()               # bir sonrakini gönder
```

### `pc_port/src/dispatch_glue.c` — Sunucu Komut Dağıtımı

```c
int _pcport_fioRead_dispatch(int fd, char *buf, int n) {
    int r = recv(fd, buf, n, 0);   // bloklayan recv
    if (r <= 0) return r;
    // buf[0] = komut byte'ı
    // SORUN: 85 byte gelirse sadece buf[0]'a bakılıyor,
    //        geri kalan 68 byte KAYBOLUR
    switch ((uint8_t)buf[0]) {
        case AUDIO_CMD_SET_GAIN_BLOCK:
            AudioCmdHandleSetGainBlock(fd, (uint8_t*)buf);
            break;
        // ...
    }
    return r;
}
```

### `AudioConf/AudioConfigHandler.h` — Yanıt Yapısı

```c
// sizeof(AudioResponse) = 131 byte (padding YOK — tüm uint8_t)
typedef struct {
    uint8_t cmd_type;      // [0]  komut | 0x80
    uint8_t status;        // [1]  0=OK
    uint8_t error_code;    // [2]
    uint8_t data[128];     // [3..130]
} AudioResponse;

// VERIFY yanıtında:
// data[0..3] = stored_crc32
// data[4..7] = computed_crc32
// data[8]    = crc_match  → Python'da buf[11]
```

---

## 7. Kalan Bug'lar (plan dosyasından)

Plan detayları: `C:\Users\Tolgahan\.claude\plans\pc-ui-pyqt5-dizininde-...`

| # | Dosya | Satır | Sorun | Önem |
|---|-------|-------|-------|------|
| 1 | `AudioFlashMgr.c` | 130-134 | CRC offset yanlış: hardcoded 141 yerine `AUDIO_CFG_CRC32_COVERED_SIZE` (161) olmalı | MAJOR |
| 2 | `dispatch_glue.c` | 46-85 | ✅ Zaten düzeltildi: min_len kontrolü var | — |
| 3 | `AudioConfigTask.c` | 27 | `int` alana `NULL` atanıyor (C uyarısı) | MINOR |
| 4 | `AudioConfigHandler.c` | — | Derleme-zamanı buffer boyut assertion eksik | MINOR |

---

## 8. TCP Protokol Özeti

```
Python → Sunucu (17 byte, SET_GAIN_BLOCK):
  [0]    = 0x03 (AUDIO_CMD_SET_GAIN_BLOCK)
  [1]    = mode (0-5)
  [2..16] = 15 gain değeri (uint8_t)

Sunucu → Python (131 byte, her komuta yanıt):
  [0]    = cmd | 0x80
  [1]    = status (0=OK)
  [2]    = error_code
  [3..130] = data (STATUS için 90 byte kullanılır: 6mod×15param)

Python → Sunucu (1 byte, STATUS):
  [0]    = 0x01

Python → Sunucu (4 byte, SET_PARAM):
  [0]    = 0x02
  [1]    = mode
  [2]    = param_index
  [3]    = value

Python → Sunucu (1 byte, VERIFY):
  [0]    = 0x04
```

---

## 9. Git Geçmişi (Son Commitler)

```
ceafb6e  fix: stdout redirect kaldırıldı — [DBG] çıktısı görünür
0ffb422  fix: TCP_NODELAY + _RSP_SIZE=131 + console=True EXE
dbcbfb8  fix: /Zp1 + Winsock2 struct alignment çakışması
9c06fef  fix: /Zp1 kaldırıldı
03cc30c  fix: MSVC __attribute__((packed)) uyumsuzluğu
1c6daf6  fix: CMakePresets.json version 6→3
6404ef1  build: CMake desteği eklendi
```

---

## 10. Yeni Bilgisayarda İlk Adımlar

```powershell
# 1. Repo klon
git clone <repo-url> AudioDspConfig
cd AudioDspConfig

# 2. Python bağımlılıkları
pip install pyqt5 pyinstaller

# 3. C sunucuyu derle (MinGW varsa)
gcc -Wall -I pc_port/include -I . \
    AudioConf/AudioConf.c AudioConf/AudioConfigTask.c \
    AudioConf/AudioConfigHandler.c AudioConf/AudioParamDef.c \
    AudioConf/AudioSpiGateway.c AudioConf/AudioFlashMgr.c \
    pc_port/src/dispatch_glue.c pc_port/src/flash_stubs.c \
    pc_port/src/tasklib_stubs.c \
    -o audio_dsp_server.exe -lws2_32 -lpthread

# 4. Python UI debug modunda çalıştır
cd pc-ui-pyqt5
python main.py
```

---

*Sorun takibi: Bu repo içinde `HANDOFF.md`.*
