#!/usr/bin/env python3
"""
MPC866 Audio DSP Configuration Device Emulator
===============================================
Emulates the VxWorks/MPC866 TCP server (port 5558) on a PC.
Protocol details derived from AudioConf/ embedded C source.

Usage
-----
    python device_emulator.py               # CLI mode (coloured log)
    python device_emulator.py --gui         # GUI mode (PyQt5)
    python device_emulator.py --host 0.0.0.0 --port 5558

Response packet (always 131 bytes)
-----------------------------------
    [cmd|0x80]  [status]  [error_code]  [data:128]

VERIFY response data layout
----------------------------
    data[0:4]  = stored_crc32  (big-endian, matches PC export format)
    data[4:8]  = computed_crc32 (big-endian)
    data[8]    = match (1=OK, 0=MISMATCH)
    → full response byte [11] = data[8] = match
"""

import sys
import os
import socket
import threading
import struct
import binascii
import argparse
import time
from datetime import datetime
from typing import Callable, List, Optional, Tuple

# ─── Protocol constants ───────────────────────────────────────────────────────

DEFAULT_HOST = "0.0.0.0"
DEFAULT_PORT = 5558

CMD_STATUS         = 0x01
CMD_SET_PARAM      = 0x02
CMD_SET_GAIN_BLOCK = 0x03
CMD_VERIFY         = 0x04

STATUS_OK    = 0
STATUS_ERROR = 1

ERR_OK            = 0
ERR_GENERAL       = 1
ERR_INVALID_CMD   = 2
ERR_INVALID_MODE  = 3
ERR_INVALID_PARAM = 4
ERR_INVALID_VALUE = 5
ERR_FLASH_ERROR   = 6
ERR_CRC_ERROR     = 7
ERR_SPI_ERROR     = 8
ERR_BUSY          = 9

NUM_MODES  = 6
NUM_PARAMS = 15

MODE_NAMES = ["AM", "FM", "HQII", "9600", "9600A", "SK2"]

PARAM_NAMES = [
    "Input Level Control",    # 0
    "ADC PGA Gain",           # 1
    "Sidetone Attenuation",   # 2
    "Output Level Control",   # 3
    "Speaker Attenuation",    # 4
    "Soft AGC Active",        # 5
    "Soft AGC Target Mod",    # 6
    "Soft AGC Max Gain",      # 7
    "Hard AGC Active",        # 8
    "Hard AGC Target Gain",   # 9
    "Hard AGC Attack Time",   # 10
    "Hard AGC Decay Time",    # 11
    "Hard AGC Max Gain",      # 12
    "Hard AGC Noise Hyst.",   # 13
    "Hard AGC Noise Thr.",    # 14
]

# (min_raw, max_raw) per parameter — matches AudioConfigHandler.c validate_gain_value()
PARAM_LIMITS: List[Tuple[int, int]] = [
    (0,   8),   # 0  INPUT_LEVEL_CONTROL_GAIN
    (0, 119),   # 1  ADC_PGA_GAIN
    (0, 120),   # 2  SIDETONE_ATTENUATION_GAIN
    (0,   9),   # 3  OUTPUT_LEVEL_CONTROL_GAIN
    (0, 127),   # 4  SPEAKER_ATTENUATION_GAIN
    (0,   1),   # 5  SOFT_AGC_ACTIVE
    (0, 100),   # 6  SOFT_AGC_TARGET_MOD_INDEX
    (0,  30),   # 7  SOFT_AGC_MAX_GAIN
    (0,   1),   # 8  HARD_AGC_ACTIVE
    (0,   7),   # 9  HARD_AGC_TARGET_GAIN
    (0,   3),   # 10 HARD_AGC_ATTACK_TIME
    (0,   3),   # 11 HARD_AGC_DECAY_TIME
    (0, 119),   # 12 HARD_AGC_MAX_GAIN
    (0,   3),   # 13 HARD_AGC_NOISE_HYSTERESIS_LEVEL
    (0,  31),   # 14 HARD_AGC_NOISE_THRESHOLD
]

# Default parameter values (from AudioParamDef.c)
DEFAULT_PARAMS: List[int] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 1, 0, 0, 0]
#                                                          ^  ^  ^
#              hard_agc_target=-10dB──────────────────────┘  │  │
#              hard_agc_attack=16ms ─────────────────────────┘  │
#              hard_agc_decay=200ms ────────────────────────────┘

# Expected incoming command sizes (bytes)
CMD_SIZES = {
    CMD_STATUS:         1,
    CMD_SET_PARAM:      4,   # [0x02][mode][param][value]
    CMD_SET_GAIN_BLOCK: 17,  # [0x03][mode][v0..v14]
    CMD_VERIFY:         1,
}

# 300-byte config file layout (matches pc-ui-pyqt5/audio_protocol.py)
_CFG_HDR_LEN      = 40
_CFG_VER_LEN      = 30
_CFG_FILE_SIZE    = 300
_CFG_CRC_OFFSET   = 161   # CRC32 field at byte 161
_CFG_CRC_COVERED  = 161   # CRC covers bytes 0–160
_CFG_PARAMS_OFFSET = 71   # gain sets start at byte 71


# ─── DeviceState ─────────────────────────────────────────────────────────────

class DeviceState:
    """Simulates the 300-byte Flash region on the embedded device."""

    def __init__(self):
        self._lock = threading.Lock()
        self._params: List[List[int]] = [
            list(DEFAULT_PARAMS) for _ in range(NUM_MODES)
        ]

    def reset(self):
        """Restore firmware default values."""
        with self._lock:
            self._params = [list(DEFAULT_PARAMS) for _ in range(NUM_MODES)]

    # ── Read ──────────────────────────────────────────────────────────────────

    def get_all_as_bytes(self) -> bytes:
        """Return 90 bytes: 6 modes × 15 params (STATUS response payload)."""
        with self._lock:
            return bytes(v for mode in self._params for v in mode)

    def snapshot(self) -> List[List[int]]:
        with self._lock:
            return [list(m) for m in self._params]

    # ── Write ─────────────────────────────────────────────────────────────────

    def set_param(self, mode: int, param: int, value: int):
        with self._lock:
            self._params[mode][param] = value

    def set_gain_block(self, mode: int, values: List[int]):
        with self._lock:
            self._params[mode] = list(values[:NUM_PARAMS])

    # ── Config file ───────────────────────────────────────────────────────────

    def _build_config_bytes(self) -> bytes:
        """Serialise current state to 300-byte binary (big-endian CRC, PC format)."""
        hdr       = b"AUDIO_CFG_V99"
        hdr       = hdr[:_CFG_HDR_LEN].ljust(_CFG_HDR_LEN, b'\x00')
        ver       = b"V01.00.00 emulator\x00".ljust(_CFG_VER_LEN, b'\x00')
        set_count = bytes([NUM_MODES])
        params_b  = self.get_all_as_bytes()          # 90 bytes

        covered   = hdr + ver + set_count + params_b  # 161 bytes
        crc_val   = binascii.crc32(covered) & 0xFFFFFFFF
        crc_bytes = struct.pack('>I', crc_val)        # big-endian (PC format)
        padding   = bytes(_CFG_FILE_SIZE - _CFG_CRC_OFFSET - 4)

        result = covered + crc_bytes + padding
        assert len(result) == _CFG_FILE_SIZE
        return result

    def verify_crc(self) -> Tuple[int, int, bool]:
        """Return (stored_crc, computed_crc, match) for VERIFY command."""
        data     = self._build_config_bytes()
        stored   = struct.unpack('>I', data[_CFG_CRC_OFFSET:_CFG_CRC_OFFSET + 4])[0]
        computed = binascii.crc32(data[:_CFG_CRC_COVERED]) & 0xFFFFFFFF
        return stored, computed, stored == computed

    def load_from_file(self, path: str):
        """Load parameters from a 300-byte .bin exported by the PC application."""
        with open(path, 'rb') as f:
            data = f.read()
        if len(data) < _CFG_FILE_SIZE:
            raise ValueError(f"File too small: {len(data)} B (expected {_CFG_FILE_SIZE})")

        stored   = struct.unpack('>I', data[_CFG_CRC_OFFSET:_CFG_CRC_OFFSET + 4])[0]
        computed = binascii.crc32(data[:_CFG_CRC_COVERED]) & 0xFFFFFFFF
        if stored != computed:
            raise ValueError(
                f"CRC32 mismatch — stored {stored:#010x}, computed {computed:#010x}"
            )
        with self._lock:
            for i in range(NUM_MODES):
                off = _CFG_PARAMS_OFFSET + i * NUM_PARAMS
                self._params[i] = list(data[off: off + NUM_PARAMS])

    def save_to_file(self, path: str):
        """Save current state to a 300-byte .bin compatible with the PC application."""
        data = self._build_config_bytes()
        tmp  = path + ".tmp"
        with open(tmp, 'wb') as f:
            f.write(data)
        os.replace(tmp, path)


# ─── ProtocolHandler ─────────────────────────────────────────────────────────

LogEntry = Tuple[str, str, str, str]   # (timestamp, direction, message, error)


class ProtocolHandler:
    """Parses one command and returns the 131-byte response."""

    RESP_SIZE = 131  # 1 + 1 + 1 + 128

    def __init__(self, state: DeviceState,
                 on_command: Optional[Callable[[LogEntry], None]] = None):
        self._state      = state
        self._on_command = on_command

    def handle(self, data: bytes) -> bytes:
        ts  = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        cmd = data[0] if data else 0xFF

        if cmd == CMD_STATUS:
            resp = self._do_status()
            self._log(ts, "→", "STATUS", "")
            self._log(ts, "←", "STATUS OK  (90 B params)", "")

        elif cmd == CMD_SET_PARAM:
            resp, detail, err = self._do_set_param(data)
            self._log(ts, "→", f"SET_PARAM  {detail}", "")
            self._log(ts, "←",
                      "SET_PARAM OK" if not err else f"SET_PARAM ERROR: {err}",
                      err)

        elif cmd == CMD_SET_GAIN_BLOCK:
            resp, detail, err = self._do_set_gain_block(data)
            self._log(ts, "→", f"SET_GAIN_BLOCK  {detail}", "")
            self._log(ts, "←",
                      "SET_GAIN_BLOCK OK" if not err else f"SET_GAIN_BLOCK ERROR: {err}",
                      err)

        elif cmd == CMD_VERIFY:
            resp, detail = self._do_verify()
            self._log(ts, "→", "VERIFY", "")
            self._log(ts, "←", f"VERIFY {detail}", "")

        else:
            self._log(ts, "→", f"UNKNOWN cmd={cmd:#04x}", "INVALID_CMD")
            self._log(ts, "←", "ERROR: INVALID_CMD", "INVALID_CMD")
            resp = self._response(cmd, STATUS_ERROR, ERR_INVALID_CMD)

        return resp

    # ── Command implementations ───────────────────────────────────────────────

    def _do_status(self) -> bytes:
        payload = self._state.get_all_as_bytes()   # 90 bytes
        return self._response(CMD_STATUS, STATUS_OK, ERR_OK, payload)

    def _do_set_param(self, data: bytes):
        if len(data) < 4:
            return self._response(CMD_SET_PARAM, STATUS_ERROR, ERR_GENERAL), "short packet", "short packet"

        mode, param, value = data[1], data[2], data[3]
        mode_name = MODE_NAMES[mode] if mode < NUM_MODES else "?"
        detail    = f"mode={mode}({mode_name}) param={param}({PARAM_NAMES[param] if param < NUM_PARAMS else '?'}) val={value}"

        if mode >= NUM_MODES:
            return self._response(CMD_SET_PARAM, STATUS_ERROR, ERR_INVALID_MODE), detail, f"INVALID_MODE ({mode})"
        if param >= NUM_PARAMS:
            return self._response(CMD_SET_PARAM, STATUS_ERROR, ERR_INVALID_PARAM), detail, f"INVALID_PARAM ({param})"

        lo, hi = PARAM_LIMITS[param]
        if not (lo <= value <= hi):
            err = f"INVALID_VALUE val={value} range=[{lo},{hi}]"
            return self._response(CMD_SET_PARAM, STATUS_ERROR, ERR_INVALID_VALUE), detail, err

        self._state.set_param(mode, param, value)
        return self._response(CMD_SET_PARAM, STATUS_OK, ERR_OK), detail, ""

    def _do_set_gain_block(self, data: bytes):
        if len(data) < 2 + NUM_PARAMS:
            return self._response(CMD_SET_GAIN_BLOCK, STATUS_ERROR, ERR_GENERAL), "short packet", "short packet"

        mode   = data[1]
        values = list(data[2: 2 + NUM_PARAMS])
        mode_name = MODE_NAMES[mode] if mode < NUM_MODES else "?"
        detail    = f"mode={mode}({mode_name})"

        if mode >= NUM_MODES:
            return self._response(CMD_SET_GAIN_BLOCK, STATUS_ERROR, ERR_INVALID_MODE), detail, f"INVALID_MODE ({mode})"

        for i, v in enumerate(values):
            lo, hi = PARAM_LIMITS[i]
            if not (lo <= v <= hi):
                err = f"INVALID_VALUE param={i}({PARAM_NAMES[i]}) val={v} range=[{lo},{hi}]"
                return self._response(CMD_SET_GAIN_BLOCK, STATUS_ERROR, ERR_INVALID_VALUE), detail, err

        self._state.set_gain_block(mode, values)
        return self._response(CMD_SET_GAIN_BLOCK, STATUS_OK, ERR_OK), detail, ""

    def _do_verify(self):
        stored, computed, match = self._state.verify_crc()
        # data layout (matches embedded spec + fixed PC parser):
        # data[0:4] = stored CRC32  (big-endian)
        # data[4:8] = computed CRC32 (big-endian)
        # data[8]   = match byte  → response byte [11]
        payload = bytearray(9)
        struct.pack_into('>I', payload, 0, stored)
        struct.pack_into('>I', payload, 4, computed)
        payload[8] = 1 if match else 0

        result = f"OK ✓ (CRC={stored:#010x})" if match else f"MISMATCH stored={stored:#010x} computed={computed:#010x}"
        return self._response(CMD_VERIFY, STATUS_OK, ERR_OK, bytes(payload)), result

    # ── Helpers ───────────────────────────────────────────────────────────────

    @staticmethod
    def _response(cmd: int, status: int, error: int, payload: bytes = b"") -> bytes:
        data = bytearray(128)
        data[:min(len(payload), 128)] = payload[:128]
        return bytes([cmd | 0x80, status, error]) + bytes(data)

    def _log(self, ts: str, direction: str, msg: str, err: str):
        if self._on_command:
            self._on_command((ts, direction, msg, err))


# ─── EmulatorServer ──────────────────────────────────────────────────────────

class EmulatorServer:
    def __init__(self,
                 state: DeviceState,
                 on_command: Optional[Callable] = None,
                 on_client_change: Optional[Callable] = None):
        self._state            = state
        self.on_command        = on_command
        self.on_client_change  = on_client_change
        self._sock: Optional[socket.socket] = None
        self._running          = False
        self._clients: set     = set()
        self._lock             = threading.Lock()

    def start(self, host: str = DEFAULT_HOST, port: int = DEFAULT_PORT):
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.bind((host, port))
        self._sock.listen(4)
        self._sock.settimeout(1.0)
        self._running = True
        threading.Thread(target=self._accept_loop, daemon=True,
                         name="emu-accept").start()

    def stop(self):
        self._running = False
        if self._sock:
            try:
                self._sock.close()
            except Exception:
                pass

    def client_count(self) -> int:
        with self._lock:
            return len(self._clients)

    def _accept_loop(self):
        while self._running:
            try:
                conn, addr = self._sock.accept()
            except socket.timeout:
                continue
            except Exception:
                break
            with self._lock:
                self._clients.add(addr)
            if self.on_client_change:
                self.on_client_change(addr, True, self.client_count())
            threading.Thread(target=self._handle_client, args=(conn, addr),
                             daemon=True, name=f"emu-{addr[0]}").start()

    def _handle_client(self, conn: socket.socket, addr: tuple):
        try:
            conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            conn.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
            conn.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 0x10000)
            conn.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 0x10000)
        except Exception:
            pass

        handler = ProtocolHandler(self._state, on_command=self.on_command)
        buf = bytearray()

        try:
            while self._running:
                try:
                    chunk = conn.recv(256)
                except Exception:
                    break
                if not chunk:
                    break
                buf.extend(chunk)

                # Reassemble complete commands before processing
                while buf:
                    cmd_byte = buf[0]
                    needed   = CMD_SIZES.get(cmd_byte, 1)
                    if len(buf) < needed:
                        break
                    msg      = bytes(buf[:needed])
                    del buf[:needed]
                    try:
                        resp = handler.handle(msg)
                        conn.sendall(resp)
                    except Exception:
                        break
        finally:
            try:
                conn.close()
            except Exception:
                pass
            with self._lock:
                self._clients.discard(addr)
            if self.on_client_change:
                self.on_client_change(addr, False, self.client_count())


# ─── CLI mode ─────────────────────────────────────────────────────────────────

_R = "\033[0m"
_B = "\033[94m"   # blue   → request
_G = "\033[92m"   # green  → OK response
_Y = "\033[93m"   # yellow → client events
_E = "\033[91m"   # red    → error
_C = "\033[96m"   # cyan   → server info
_D = "\033[90m"   # grey   → timestamp


def _cli_log(entry: LogEntry):
    ts, direction, msg, err = entry
    color = _B if direction == "→" else (_E if err else _G)
    print(f"{_D}[{ts}]{_R} {color}{direction} {msg}{_R}", flush=True)


def run_cli(host: str, port: int):
    state  = DeviceState()

    def on_client(addr, connected, count):
        tag = "CONNECT" if connected else "DISCONNECT"
        col = _Y
        print(f"{col}[{tag}] {addr[0]}:{addr[1]}  clients={count}{_R}", flush=True)

    server = EmulatorServer(state, on_command=_cli_log, on_client_change=on_client)
    server.start(host, port)
    print(f"{_C}[EMU] MPC866 Audio DSP emulator listening on {host}:{port}{_R}")
    print(f"{_D}Press Ctrl+C to stop.{_R}")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print(f"\n{_Y}[EMU] Stopping…{_R}")
        server.stop()


# ─── Display helpers (shared between GUI and table) ──────────────────────────

def _display(param_idx: int, raw: int) -> str:
    if param_idx == 0:
        return f"{raw}  ({0 if raw == 0 else -raw * 1.5:.1f} dB)"
    if param_idx in (1, 12):
        return f"{raw}  ({raw * 0.5:.1f} dB)"
    if param_idx == 2:
        return f"{raw}  (-{raw * 0.5:.1f} dB)"
    if param_idx == 3:
        return f"{raw}  (+{raw:.0f} dB)"
    if param_idx == 4:
        return f"{raw}  (-{raw * 0.5:.1f} dB)"
    if param_idx in (5, 8):
        return ["PASSIVE", "ACTIVE"][raw] if raw <= 1 else str(raw)
    if param_idx == 6:
        return f"{raw}%"
    if param_idx == 7:
        return f"{raw} dB"
    if param_idx == 9:
        m = {0: "-5.5", 1: "-8", 2: "-10", 3: "-12", 4: "-14", 5: "-17", 6: "-20", 7: "-24"}
        return f"{raw}  ({m.get(raw, '?')} dB)"
    if param_idx == 10:
        return f"{raw}  ({[8, 11, 16, 20][raw]} ms)" if raw < 4 else str(raw)
    if param_idx == 11:
        return f"{raw}  ({[100, 200, 400, 500][raw]} ms)" if raw < 4 else str(raw)
    if param_idx == 13:
        return ["Disabled", "1 dB", "2 dB", "3 dB"][raw] if raw < 4 else str(raw)
    if param_idx == 14:
        return "Disabled" if raw == 0 else f"{raw}  ({-30 - (raw - 1) * 2} dB)"
    return str(raw)


# ─── GUI mode ─────────────────────────────────────────────────────────────────

def run_gui(host: str, port: int):
    try:
        from PyQt5.QtWidgets import (
            QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
            QPushButton, QLabel, QTableWidget, QTableWidgetItem,
            QTextEdit, QSplitter, QFileDialog, QTabBar,
        )
        from PyQt5.QtCore import Qt, QTimer, QObject, pyqtSignal
        from PyQt5.QtGui import QFont, QColor
    except ImportError:
        print("PyQt5 is not installed. Run without --gui for CLI mode.")
        sys.exit(1)

    state = DeviceState()

    # ── Signal bridge: worker threads → Qt GUI thread ─────────────────────────
    class _Bridge(QObject):
        command_sig      = pyqtSignal(tuple)
        client_change_sig = pyqtSignal(tuple, bool, int)

    bridge = _Bridge()
    server = EmulatorServer(
        state,
        on_command       = lambda e: bridge.command_sig.emit(e),
        on_client_change = lambda addr, conn, cnt: bridge.client_change_sig.emit(addr, conn, cnt),
    )

    # ── Main window ───────────────────────────────────────────────────────────
    class EmulatorWindow(QMainWindow):
        def __init__(self):
            super().__init__()
            self.setWindowTitle(f"MPC866 Audio DSP Emulator  —  port {port}")
            self.setMinimumSize(1020, 620)
            self.resize(1120, 700)
            self._cur_mode = 0
            self._setup_ui()
            self._load_theme()
            self._refresh_table()

            bridge.command_sig.connect(self._on_cmd)
            bridge.client_change_sig.connect(self._on_client)

            server.start(host, port)
            self._status_lbl.setText(f"Listening on {host}:{port}  |  Clients: 0")

            # Refresh table every 500 ms (params change from worker threads)
            t = QTimer(self)
            t.setInterval(500)
            t.timeout.connect(self._refresh_table)
            t.start()

        # ── Theme ─────────────────────────────────────────────────────────────

        def _load_theme(self):
            candidates = [
                os.path.join(os.path.dirname(__file__), "..", "pc-ui-pyqt5",
                             "resources", "style.qss"),
                os.path.join(os.path.dirname(__file__), "resources", "style.qss"),
            ]
            for p in candidates:
                p = os.path.normpath(p)
                if os.path.exists(p):
                    with open(p, encoding="utf-8") as f:
                        self.setStyleSheet(f.read())
                    return
            # Minimal fallback
            self.setStyleSheet("""
                QWidget { background:#12121e; color:#dde3ee; font-size:12px; }
                QTableWidget { gridline-color:#1e3a5c; }
                QHeaderView::section { background:#162033; color:#4FC3F7; padding:4px; }
                QTextEdit { background:#0a0e1a; color:#c0ccd8; }
                QPushButton { background:#162033; border:1px solid #2e5f8a;
                              color:#b0c4de; border-radius:4px; padding:5px 12px; }
                QPushButton:hover { background:#1e3a5c; color:#4FC3F7; }
            """)

        # ── Build UI ──────────────────────────────────────────────────────────

        def _setup_ui(self):
            cw = QWidget()
            self.setCentralWidget(cw)
            root = QVBoxLayout(cw)
            root.setContentsMargins(8, 8, 8, 6)
            root.setSpacing(6)

            splitter = QSplitter(Qt.Horizontal)

            # ── Left: mode tabs + param table ──────────────────────────────
            left = QWidget()
            lv   = QVBoxLayout(left)
            lv.setContentsMargins(0, 0, 0, 0)
            lv.setSpacing(4)

            tab_row = QHBoxLayout()
            self._tabs = QTabBar()
            self._tabs.setExpanding(False)
            for m in MODE_NAMES:
                self._tabs.addTab(f"  {m}  ")
            self._tabs.currentChanged.connect(self._on_mode_tab)
            tab_row.addWidget(self._tabs)
            tab_row.addStretch()
            lv.addLayout(tab_row)

            self._table = QTableWidget(NUM_PARAMS, 2)
            self._table.setHorizontalHeaderLabels(["Parameter", "Value"])
            self._table.verticalHeader().setVisible(False)
            self._table.setEditTriggers(QTableWidget.NoEditTriggers)
            self._table.setSelectionBehavior(QTableWidget.SelectRows)
            self._table.horizontalHeader().setStretchLastSection(True)
            self._table.setColumnWidth(0, 220)
            self._table.setAlternatingRowColors(True)
            for i, name in enumerate(PARAM_NAMES):
                item = QTableWidgetItem(name)
                item.setForeground(QColor("#7090b0"))
                self._table.setItem(i, 0, item)
                self._table.setItem(i, 1, QTableWidgetItem(""))
            lv.addWidget(self._table)

            btn_row = QHBoxLayout()
            for label, slot in [
                ("Reset Defaults", self._on_reset),
                ("Load .bin",      self._on_load),
                ("Save .bin",      self._on_save),
            ]:
                b = QPushButton(label)
                b.setObjectName("fileBtn")
                b.clicked.connect(slot)
                btn_row.addWidget(b)
            lv.addLayout(btn_row)

            # ── Right: command log ──────────────────────────────────────────
            right = QWidget()
            rv    = QVBoxLayout(right)
            rv.setContentsMargins(0, 0, 0, 0)
            rv.setSpacing(4)

            hdr = QLabel("Command Log")
            hdr.setStyleSheet("color:#4FC3F7; font-weight:bold; font-size:12px;")
            rv.addWidget(hdr)

            self._log = QTextEdit()
            self._log.setReadOnly(True)
            self._log.setFont(QFont("Consolas", 10))
            self._log.setStyleSheet(
                "background:#080c18; color:#b8c8d8; "
                "border:1px solid #1e3a5c; border-radius:4px;"
            )
            rv.addWidget(self._log)

            clear = QPushButton("Clear Log")
            clear.setObjectName("fileBtn")
            clear.clicked.connect(self._log.clear)
            rv.addWidget(clear)

            splitter.addWidget(left)
            splitter.addWidget(right)
            splitter.setStretchFactor(0, 5)
            splitter.setStretchFactor(1, 6)
            root.addWidget(splitter, 1)

            # ── Status bar ──────────────────────────────────────────────────
            bar = QHBoxLayout()
            self._dot = QLabel("●")
            self._dot.setStyleSheet("color:#444466; font-size:18px;")
            self._dot.setFixedWidth(20)
            self._status_lbl = QLabel("Initialising…")
            self._status_lbl.setStyleSheet("color:#7090b0; font-size:11px;")
            bar.addWidget(self._dot)
            bar.addWidget(self._status_lbl, 1)
            root.addLayout(bar)

        # ── Slots ─────────────────────────────────────────────────────────────

        def _on_mode_tab(self, idx: int):
            self._cur_mode = idx
            self._refresh_table()

        def _refresh_table(self):
            snap = state.snapshot()[self._cur_mode]
            for i, v in enumerate(snap):
                item = QTableWidgetItem(_display(i, v))
                item.setForeground(QColor("#4FC3F7"))
                self._table.setItem(i, 1, item)

        def _on_cmd(self, entry: tuple):
            ts, direction, msg, err = entry
            color = "#6fc4f7" if direction == "→" else ("#e94560" if err else "#4CAF50")
            self._log.append(
                f'<span style="color:#445566">[{ts}]</span> '
                f'<span style="color:{color}">{direction} {msg}</span>'
            )
            sb = self._log.verticalScrollBar()
            sb.setValue(sb.maximum())

        def _on_client(self, addr: tuple, connected: bool, count: int):
            if connected:
                self._dot.setStyleSheet("color:#4CAF50; font-size:18px;")
                self._status_lbl.setText(
                    f"Connected: {addr[0]}:{addr[1]}  |  Clients: {count}"
                )
                self._status_lbl.setStyleSheet(
                    "color:#4CAF50; font-size:11px; font-weight:bold;"
                )
                self._log.append(
                    f'<span style="color:#4CAF50">● CONNECT  {addr[0]}:{addr[1]}</span>'
                )
            else:
                n = server.client_count()
                if n == 0:
                    self._dot.setStyleSheet("color:#444466; font-size:18px;")
                    self._status_lbl.setText(
                        f"Listening on {host}:{port}  |  Clients: 0"
                    )
                    self._status_lbl.setStyleSheet("color:#7090b0; font-size:11px;")
                else:
                    self._status_lbl.setText(
                        f"Disconnected: {addr[0]}  |  Clients: {n}"
                    )
                self._log.append(
                    f'<span style="color:#f0ad4e">○ DISCONNECT  {addr[0]}:{addr[1]}</span>'
                )

        def _on_reset(self):
            state.reset()
            self._refresh_table()
            self._log.append(
                '<span style="color:#f0c040">⟳ Parameters reset to firmware defaults</span>'
            )

        def _on_load(self):
            path, _ = QFileDialog.getOpenFileName(
                self, "Load .bin", "", "Binary Files (*.bin);;All Files (*.*)"
            )
            if not path:
                return
            try:
                state.load_from_file(path)
                self._refresh_table()
                self._log.append(
                    f'<span style="color:#4CAF50">↑ Loaded: {os.path.basename(path)}</span>'
                )
            except Exception as e:
                self._log.append(f'<span style="color:#e94560">✗ Load error: {e}</span>')

        def _on_save(self):
            path, _ = QFileDialog.getSaveFileName(
                self, "Save .bin", "emulator_state.bin",
                "Binary Files (*.bin);;All Files (*.*)"
            )
            if not path:
                return
            try:
                state.save_to_file(path)
                self._log.append(
                    f'<span style="color:#4CAF50">↓ Saved: {os.path.basename(path)}</span>'
                )
            except Exception as e:
                self._log.append(f'<span style="color:#e94560">✗ Save error: {e}</span>')

        def closeEvent(self, event):
            server.stop()
            event.accept()

    app = QApplication(sys.argv)
    app.setApplicationName("MPC866 DSP Emulator")
    win = EmulatorWindow()
    win.show()
    sys.exit(app.exec())


# ─── Entry point ──────────────────────────────────────────────────────────────

def main():
    p = argparse.ArgumentParser(description="MPC866 Audio DSP Device Emulator")
    p.add_argument("--gui",  action="store_true",
                   help="Launch PyQt5 GUI (default: CLI)")
    p.add_argument("--host", default=DEFAULT_HOST,
                   help=f"Bind address (default: {DEFAULT_HOST})")
    p.add_argument("--port", type=int, default=DEFAULT_PORT,
                   help=f"TCP port (default: {DEFAULT_PORT})")
    args = p.parse_args()

    if args.gui:
        run_gui(args.host, args.port)
    else:
        run_cli(args.host, args.port)


if __name__ == "__main__":
    main()
