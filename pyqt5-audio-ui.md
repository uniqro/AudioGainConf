# Python + PyQt5 Audio DSP UI Uygulaması

## TL;DR

> **Özet**: Electron kurulum sorunu (kurumsal proxy) nedeniyle PyQt5'e geçiş. Audio DSP Configuration için PyQt5 masaüstü uygulaması. TCP port 5558, 4 command, 15 parametre, 6 mode set. PyInstaller ile EXE build.

**Deliverables**:
- PyQt5 UI (Connection Panel, Param Editor, File Operations)
- TCP client (socket wrapper, port 5558)
- Audio protocol (4 commands, 15 params, CRC32)
- PyInstaller EXE build script
- Dark theme QSS

**Estimated Effort**: MEDIUM — ~8 tasks, 2.5 saat
**Parallel Execution**: NO — Sequential (dependencies)
**Critical Path**: Cleanup → Basic → TCP → Protocol → UI → Test → EXE Build

---

## Context

### Original Request
96SK telsiz cihazı için Audio DSP (C5416) gain configuration UI. Electron ile başladı, kurumsal proxy nedeniyle Electron binary indirilemedi. PyQt5 mevcut ve çalışıyor.

### Technical Decisions
1. **PyQt5**: Electron yerine (zaten kurulu, network gerekmez)
2. **TCP Port 5558**: Runtime config (separate from 5518)
3. **Binary Protocol**: 4 commands, 15 params, 300 byte
4. **Dark Theme**: QSS stylesheet (Electron CSS benzeri)
5. **EXE Build**: PyInstaller (tek dosya dağıtım)

### Cleanup Required
- Electron `pc-ui/` dizini silinecek (100MB+ node_modules)
- PyQt5 yeni proje: `pc-ui-pyqt5/`

---

## Work Objectives

### Core Objective
Audio DSP Configuration için PyQt5 masaüstü uygulaması geliştir, EXE build script hazırla.

### Concrete Deliverables
1. `pc-ui-pyqt5/main.py` - QApplication, MainWindow
2. `pc-ui-pyqt5/tcp_client.py` - TCP socket wrapper
3. `pc-ui-pyqt5/audio_protocol.py` - 4 commands, 15 params
4. `pc-ui-pyqt5/ui/*.py` - Connection, Params, File Ops
5. `pc-ui-pyqt5/resources/style.qss` - Dark theme
6. `pc-ui-pyqt5/build_exe.spec` - PyInstaller config
7. `pc-ui-pyqt5/build.bat` - Windows build script

### Definition of Done
- [x] Electron `pc-ui/` silindi
- [ ] PyQt5 application çalışıyor
- [ ] TCP client connect/send/receive
- [ ] Audio protocol build/parse
- [ ] UI components (Connection, Params, File Ops)
- [ ] EXE build script çalışıyor
- [ ] `dist/AudioDSPConfig.exe` oluşturuldu

### Must Have
- PyQt5 (zaten kurulu)
- TCP port 5558
- 4 command types (STATUS, SET_PARAM, SET_GAIN_BLOCK, VERIFY)
- 15 parameters, 6 mode sets
- Dark theme QSS
- PyInstaller EXE build

### Must NOT Have (Guardrails)
- ❌ Electron bağımlılığı
- ❌ Network kurulum (pip install PyQt5 yeterli)
- ❌ Turkish code naming
- ❌ Manual TCP protocol (struct.pack/unpack kullan)

---

## Verification Strategy

### Test Decision
- **Infrastructure**: PyQt5 zaten kurulu
- **Automated tests**: Syntax check, import test
- **QA Verification**: Manual UI test, mock TCP server

### QA Policy
- `python -m py_compile *.py` - Syntax check
- `python main.py` - UI başlatma
- Mock server ile TCP test
- EXE build test

---

## Execution Strategy

### Sequential Execution (8 Tasks)

```
Task 0: Cleanup (Electron temizliği)
└── pc-ui/ dizinini sil

Task 1: Basic Structure
├── main.py (QApplication, MainWindow)
├── resources/style.qss (dark theme)
└── Test: python main.py

Task 2: TCP Client
├── tcp_client.py (socket wrapper)
├── Signals: connected, disconnected, data_received, error
└── Test: Mock server connection

Task 3: Audio Protocol
├── audio_protocol.py (4 commands, 15 params)
├── Binary pack/unpack (struct)
├── CRC32 calculation
└── Test: Command build/parse

Task 4: UI Components
├── ui/connection_panel.py (IP/Port, Connect/Disconnect)
├── ui/param_editor.py (15 params, 6 mode sets, sliders)
├── ui/file_ops.py (Upload/Download, Export/Import)
└── Test: Component instantiation

Task 5: Integration & Test
├── Connect all components
├── Signals/slots wiring
└── Test: End-to-end flow

Task 6: EXE Build Script
├── build_exe.spec (PyInstaller config)
├── build.bat (Windows build script)
├── build.sh (Linux/Mac build script)
└── README_build.md (dokümantasyon)

Task 7: Build Test
├── Run build.bat
├── Verify dist/AudioDSPConfig.exe
└── Test: EXE çalıştırma
```

### Dependency Chain

```
Task 0 → Task 1 → Task 2 → Task 3 → Task 4 → Task 5 → Task 6 → Task 7
   ↓        ↓        ↓        ↓        ↓        ↓        ↓        ↓
Cleanup → Basic → TCP → Protocol → UI → Integration → EXE Script → Build Test
```

---

## TODOs

- [x] **0. Electron Temizliği**

  **What to do**:
  - `pc-ui/` dizinini sil (node_modules dahil)
  - package.json, tsconfig.json sil
  - Temizliği doğrula

  **Command**:
  ```powershell
  cd D:\9681_96SK_TYY
  Remove-Item -Recurse -Force "pc-ui"
  ```

  **Recommended Agent Profile**:
  - **Category**: quick
  - **Skills**: []

  **Parallelization**: Sequential (first task)

- [x] **1. PyQt5 Temel Yapı**

  **What to do**:
  - `main.py` oluştur (QApplication, MainWindow)
  - `resources/style.qss` oluştur (dark theme)
  - `ui/__init__.py` oluştur
  - Test: `python main.py` → Pencere açılmalı

  **Code Pattern**:
  ```python
  from PyQt5.QtWidgets import QApplication, QMainWindow, QWidget, QVBoxLayout
  import sys

  class MainWindow(QMainWindow):
      def __init__(self):
          super().__init__()
          self.setWindowTitle("Audio DSP Configuration")
          self.setMinimumSize(1200, 800)
          # Load QSS, add components...

  if __name__ == "__main__":
      app = QApplication(sys.argv)
      window = MainWindow()
      window.show()
      sys.exit(app.exec_())
  ```

  **Recommended Agent Profile**:
  - **Category**: quick
  - **Skills**: []

  **Parallelization**: Sequential (after Task 0)

- [x] **2. TCP Client Implementasyonu**

  **What to do**:
  - `tcp_client.py` oluştur
  - Socket wrapper (port 5558)
  - PyQt signals: `connected`, `disconnected`, `data_received`, `error`
  - Non-blocking I/O (QThread)

  **Code Pattern**:
  ```python
  import socket
  from PyQt5.QtCore import QObject, pyqtSignal, QThread

  class TCPClient(QObject):
      connected = pyqtSignal()
      disconnected = pyqtSignal()
      data_received = pyqtSignal(bytes)
      error = pyqtSignal(str)

      def connect(self, ip: str, port: int): ...
      def send(self, data: bytes): ...
      def disconnect(self): ...
  ```

  **Recommended Agent Profile**:
  - **Category**: quick
  - **Skills**: []

  **Parallelization**: Sequential (after Task 1)

- [x] **3. Audio Protocol Implementasyonu**

  **What to do**:
  - `audio_protocol.py` oluştur
  - 4 command types: STATUS (0x01), SET_PARAM (0x02), SET_GAIN_BLOCK (0x03), VERIFY (0x04)
  - 15 parameter definitions (ranges, labels, scaling)
  - 6 mode sets (AM/FM/HQII/9600/9600A/SK2)
  - Binary pack/unpack (struct module)
  - CRC32 calculation

  **Parameters**:
  1. INPUT_LEVEL_CONTROL_GAIN (0-8, discrete)
  2. ADC_PGA_GAIN (0-119, ×2 scaling, 0.5dB steps)
  3. SIDETONE_ATTENUATION_GAIN (0-120, ×2)
  4. OUTPUT_LEVEL_CONTROL_GAIN (0-9, discrete)
  5. SPEAKER_ATTENUATION_GAIN (0-127, ×2)
  6. SOFT_AGC_ACTIVE (0-1, bool)
  7. SOFT_AGC_TARGET_MOD_INDEX (0-100, %)
  8. SOFT_AGC_MAX_GAIN (0-30, dB)
  9. HARD_AGC_ACTIVE (0-1, bool)
  10. HARD_AGC_TARGET_GAIN (0-7, discrete)
  11. HARD_AGC_ATTACK_TIME (0-3, discrete)
  12. HARD_AGC_DECAY_TIME (0-3, discrete)
  13. HARD_AGC_MAX_GAIN (0-119, ×2)
  14. HARD_AGC_NOISE_HYSTERESIS (0-3, discrete)
  15. HARD_AGC_NOISE_THRESHOLD (0-31, discrete)

  **Recommended Agent Profile**:
  - **Category**: quick
  - **Skills**: []

  **Parallelization**: Sequential (after Task 2)

- [x] **4. UI Components**

  **What to do**:
  - `ui/connection_panel.py` - IP/Port, Connect/Disconnect, status LED
  - `ui/param_editor.py` - 15 params, 6 mode sets, sliders/dropdowns
  - `ui/file_ops.py` - Upload/Download, Export/Import, file dialogs

  **UI Layout**:
  ```
  ┌─────────────────────────────────────────┐
  │ CONNECTION PANEL                        │
  │ IP: [192.168.1.100] Port: [5558]       │
  │ [CONNECT] [DISCONNECT] ● Status        │
  ├─────────────────────────────────────────┤
  │ PARAMETER EDITOR                        │
  │ Mode: [AM ▼]                            │
  │ [Input] [Output] [Soft AGC] [Hard AGC] │
  │ [READ] [APPLY]                          │
  ├─────────────────────────────────────────┤
  │ FILE OPERATIONS                         │
  │ [Upload] [Download] [Export] [Import]  │
  └─────────────────────────────────────────┘
  ```

  **Recommended Agent Profile**:
  - **Category**: visual-engineering
  - **Skills**: ["frontend-ui-ux"]

  **Parallelization**: Sequential (after Task 3)

- [x] **5. Integration & Test**

  **What to do**:
  - Tüm component'leri `main.py`'de birleştir
  - Signals/slots bağla (TCP ↔ UI ↔ Protocol)
  - Test: `python main.py`
  - Mock server ile TCP test

  **Recommended Agent Profile**:
  - **Category**: unspecified-high
  - **Skills**: []

  **Parallelization**: Sequential (after Task 4)

- [x] **6. EXE Build Script**

  **What to do**:
  - `build_exe.spec` - PyInstaller configuration
  - `build.bat` - Windows build script
  - `build.sh` - Linux/Mac build script
  - `README_build.md` - Build dokümantasyon

  **PyInstaller Config**:
  ```python
  a = Analysis(
      ['main.py'],
      datas=[('resources/style.qss', 'resources'), ('ui', 'ui')],
      hiddenimports=['PyQt5', 'PyQt5.QtCore', 'PyQt5.QtWidgets'],
      ...
  )
  exe = EXE(..., name='AudioDSPConfig', console=False)
  ```

  **Recommended Agent Profile**:
  - **Category**: quick
  - **Skills**: []

  **Parallelization**: Sequential (after Task 5)

- [x] **7. Build Test**

  **What to do**:
  - `build.bat` çalıştır
  - `dist/AudioDSPConfig.exe` oluştur
  - EXE'yi çalıştır, test et
  - Boyut kontrolü (~50-70MB bekleniyor)

  **Recommended Agent Profile**:
  - **Category**: quick
  - **Skills**: []

  **Parallelization**: Sequential (after Task 6, final task)

---

## Commit Strategy

| Task | Commit Message | Files |
|------|---------------|-------|
| 0 | `chore: remove Electron project (proxy issues)` | pc-ui/ (delete) |
| 1 | `feat(pyqt5): basic app structure` | main.py, resources/style.qss |
| 2 | `feat(pyqt5): TCP client wrapper` | tcp_client.py |
| 3 | `feat(pyqt5): audio protocol implementation` | audio_protocol.py |
| 4 | `feat(pyqt5): UI components` | ui/*.py |
| 5 | `feat(pyqt5): integration & signals` | main.py (update) |
| 6 | `feat(build): PyInstaller EXE build scripts` | build_exe.spec, build.bat, build.sh |
| 7 | `test(build): EXE build verification` | dist/AudioDSPConfig.exe |

---

## Success Criteria

### Verification Commands
```bash
# Syntax check
python -m py_compile main.py tcp_client.py audio_protocol.py

# UI test
python main.py

# Build test
build.bat
```

### Final Checklist
- [ ] Electron `pc-ui/` silindi
- [ ] PyQt5 application başlıyor
- [ ] TCP client connect/send/receive çalışıyor
- [ ] Audio protocol build/parse doğru
- [ ] UI components (Connection, Params, File Ops) çalışıyor
- [ ] EXE build script çalışıyor
- [ ] `dist/AudioDSPConfig.exe` oluşturuldu ve çalışıyor

---

## Notes

**Electron Sorunu:** Kurumsal proxy nedeniyle Electron binary indirilemedi (SSL certificate mismatch: `*.aselsan.com.tr`). PyQt5 zaten kurulu olduğu için geçiş yapıldı.

**PyQt5 Avantajları:**
- Network kurulum gerekmez (zaten kurulu)
- Daha hafif (50MB vs 150MB)
- Daha hızlı startup (1sn vs 5sn)
- Native Windows görünümü

**EXE Build:** PyInstaller ile tek dosya dağıtım (~50-70MB), no installation required.
