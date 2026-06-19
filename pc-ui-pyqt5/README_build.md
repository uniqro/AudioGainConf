# Audio DSP Configuration - Build Instructions

## Prerequisites

- Python 3.8 or higher
- PyInstaller installed (`pip install pyinstaller`)

## Build Steps

### Windows

1. Open Command Prompt or PowerShell
2. Navigate to project directory:
   ```cmd
   cd pc-ui-pyqt5
   ```
3. Run build script (installs dependencies and builds EXE):
   ```cmd
   build.bat
   ```
4. Wait for build to complete (~1-2 minutes)
5. Find EXE in `dist\AudioDSPConfig.exe`

### Manual Build

```bash
# Install dependencies
pip install -r requirements.txt

# Clean build
pyinstaller --clean build_exe.spec
```

## Output

- Single EXE file: `dist\AudioDSPConfig.exe`
- Size: ~50-80 MB (includes Python runtime + PyQt5)
- No installation required - just run the EXE
- Works on Windows 7/8/10/11 (64-bit)

## File Distribution

Copy `dist\AudioDSPConfig.exe` to target machines. The EXE is fully self-contained:
- All Python dependencies bundled
- UI resources (style.qss) included
- No Python installation needed on target machine

## Troubleshooting

### Build Fails - Missing Module
Add to `hiddenimports` in `build_exe.spec`:
```python
hiddenimports=['module_name', ...]
```

### EXE Crashes on Start
Build with console mode to see errors:
1. Edit `build_exe.spec`
2. Change `console=False` to `console=True`
3. Rebuild (`pyinstaller --clean build_exe.spec`)

### Missing Resources at Runtime
Check that `resources/style.qss` is included in the spec file `datas` list.

## Quick Commands

```bash
# Quick build
pyinstaller build_exe.spec

# Clean build (recommended)
pyinstaller --clean build_exe.spec

# Analyze what PyInstaller included
pyinstaller --collect-all PyQt5 > collected_modules.txt
```
