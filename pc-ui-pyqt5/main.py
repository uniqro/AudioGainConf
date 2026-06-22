"""
Audio DSP Configuration UI — Main Application
PyQt5 desktop app for configuring Audio DSP parameters via TCP.

Key architecture:
 - _cmd_queue (deque): FIFO of pending response types; prevents command collisions
 - _watchdog (QTimer, 5 s): cancels a stalled command and re-enables UI
 - Response buffer capped at 1 KB to guard against runaway data
 - Atomic file export via tempfile + os.replace()
 - QSettings for last-used export/import directories
 - Keyboard shortcuts: Ctrl+R/A/E/I + Ctrl+1-6
"""

import sys
import os
import tempfile
from collections import deque
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QSplitter, QScrollArea, QLabel, QMessageBox, QShortcut, QFileDialog,
)
from PyQt5.QtCore import Qt, QTimer, QSettings
from PyQt5.QtGui import QKeySequence, QIcon

from tcp_client import TCPClient
from audio_protocol import (
    MODE_SETS, NUM_MODES, NUM_PARAMS,
    CMD_STATUS, CMD_SET_GAIN_BLOCK, CMD_SET_PARAM, CMD_VERIFY, CMD_SET_META, CMD_GET_META,
    build_status_request, build_set_param_request,
    build_set_gain_block_request, build_verify_request,
    build_set_meta_request, parse_meta_from_status_response,
    build_get_meta_request, parse_get_meta_response,
    parse_status_response, parse_all_modes_response,
    build_config_file, parse_config_file, parse_version_string,
    validate_params,
)
from ui.connection_panel import ConnectionPanel
from ui.param_editor import ParamEditor
from ui.file_ops import FileOps

_SETTINGS_ORG    = "DSPConfig"
_SETTINGS_APP    = "AudioDSP"
_WATCHDOG_SEC    = 5
_MAX_BUFFER_BYTES = 1024
_RSP_SIZE        = 131   # sizeof(AudioResponse): 1 cmd_type + 1 status + 1 error_code + 128 data


class AudioDSPConfigApp(QMainWindow):

    def __init__(self):
        super().__init__()
        self._packets_sent     = 0
        self._packets_received = 0
        self._mode_params      = [[0] * NUM_PARAMS for _ in range(NUM_MODES)]
        self._cmd_queue: deque = deque()          # FIFO of pending response types
        self._pending_cmds: deque = deque()       # commands waiting to be sent sequentially
        self._response_buffer  = bytearray()
        self._export_after_download = False       # set by _on_download, cleared after export
        self._setup_ui()
        self._setup_client()
        self._setup_watchdog()
        self._setup_shortcuts()
        self._load_stylesheet()

    # ── UI setup ──────────────────────────────────────────────────

    def _setup_ui(self):
        self.setWindowTitle("Audio DSP Configuration Utility")
        self.setMinimumSize(1200, 800)
        self.resize(1300, 900)

        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QVBoxLayout(central)
        main_layout.setContentsMargins(8, 8, 8, 8)
        main_layout.setSpacing(8)

        splitter = QSplitter(Qt.Vertical)

        top_splitter = QSplitter(Qt.Horizontal)
        top_splitter.setContentsMargins(0, 0, 0, 0)
        top_splitter.setStretchFactor(0, 1)
        top_splitter.setStretchFactor(1, 1)

        self.connection_panel = ConnectionPanel()
        self.file_ops = FileOps()
        top_splitter.addWidget(self.connection_panel)
        top_splitter.addWidget(self.file_ops)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QScrollArea.Shape.NoFrame)
        self.param_editor = ParamEditor()
        scroll.setWidget(self.param_editor)

        splitter.addWidget(top_splitter)
        splitter.addWidget(scroll)
        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 4)
        splitter.setSizes([110, 690])

        main_layout.addWidget(splitter)

        # ── Three-section status bar ──────────────────────────────
        status_bar = QHBoxLayout()
        status_bar.setSpacing(20)
        status_bar.setContentsMargins(8, 2, 8, 2)

        self._lbl_left = QLabel("Ready")
        self._lbl_left.setStyleSheet("color: #888888; font-size: 12px;")

        self._lbl_center = QLabel("Tx: 0   Rx: 0")
        self._lbl_center.setAlignment(Qt.AlignCenter)
        self._lbl_center.setStyleSheet("color: #446688; font-size: 11px;")

        self._lbl_right = QLabel("Audio DSP Config v1.0")
        self._lbl_right.setAlignment(Qt.AlignRight)
        self._lbl_right.setStyleSheet("color: #666680; font-size: 10px;")

        status_bar.addWidget(self._lbl_left, 3)
        status_bar.addWidget(self._lbl_center, 2)
        status_bar.addWidget(self._lbl_right, 1)
        main_layout.addLayout(status_bar)

        # Wire UI signals
        self.connection_panel.connected.connect(
            lambda ip, port: self._on_connect(ip, port)
        )
        self.connection_panel.disconnected.connect(self._on_disconnect)
        self.connection_panel.connection_error.connect(self._on_error)

        self.file_ops.upload_file_clicked.connect(self._on_upload_file)
        self.file_ops.download_requested.connect(self._on_download)
        self.file_ops.export_clicked.connect(self._on_export)
        self.file_ops.import_clicked.connect(self._on_import)

        self.param_editor.read_requested.connect(self._on_read)
        self.param_editor.apply_requested.connect(self._on_apply)
        self.param_editor.apply_all_requested.connect(self._on_apply_all)

    # ── TCP client ────────────────────────────────────────────────

    def _setup_client(self):
        self.client = TCPClient(self)
        self.client.connected.connect(self._on_client_connected)
        self.client.disconnected.connect(self._on_client_disconnected)
        self.client.data_received.connect(self._on_data_received)
        self.client.error.connect(self._on_error)
        self.client.reconnecting.connect(self._on_reconnecting)

    # ── Watchdog timer ────────────────────────────────────────────

    def _setup_watchdog(self):
        self._watchdog = QTimer(self)
        self._watchdog.setSingleShot(True)
        self._watchdog.timeout.connect(self._on_watchdog_timeout)

    def _start_watchdog(self):
        self._watchdog.start(_WATCHDOG_SEC * 1000)
        self.param_editor.set_busy(True)

    def _cancel_watchdog(self):
        self._watchdog.stop()
        if not self._cmd_queue and not self._pending_cmds:
            self.param_editor.set_busy(False)

    def _on_watchdog_timeout(self):
        self._cmd_queue.clear()
        self._pending_cmds.clear()
        self._response_buffer.clear()
        self.param_editor.set_busy(False)
        self._set_status("Timeout: no response from device", error=True)

    # ── Keyboard shortcuts ────────────────────────────────────────

    def _setup_shortcuts(self):
        QShortcut(QKeySequence("Ctrl+R"), self).activated.connect(self._on_read)
        QShortcut(QKeySequence("Ctrl+A"), self).activated.connect(self._on_apply)
        QShortcut(QKeySequence("Ctrl+E"), self).activated.connect(
            lambda: self._on_export()
        )
        QShortcut(QKeySequence("Ctrl+I"), self).activated.connect(self._on_import)
        for i, mode in enumerate(MODE_SETS, start=1):
            QShortcut(QKeySequence(f"Ctrl+{i}"), self).activated.connect(
                lambda idx=i - 1: self.param_editor.mode_tabs.setCurrentIndex(idx)
            )

    # ── Status helpers ────────────────────────────────────────────

    def _set_status(self, msg: str, error: bool = False, ok: bool = False):
        color = "#e94560" if error else ("#4CAF50" if ok else "#4FC3F7")
        self._lbl_left.setText(msg)
        self._lbl_left.setStyleSheet(f"color: {color}; font-size: 12px;")

    def _update_packet_ui(self):
        self._lbl_center.setText(
            f"Tx: {self._packets_sent}   Rx: {self._packets_received}"
        )
        self.connection_panel.update_packet_counts(
            self._packets_sent, self._packets_received
        )

    # ── Resource path ─────────────────────────────────────────────

    def _resource_path(self, relative_path):
        try:
            base_path = sys._MEIPASS
        except Exception:
            base_path = os.path.abspath(".")
        return os.path.join(base_path, relative_path)

    def _load_stylesheet(self):
        qss_path = self._resource_path(os.path.join("resources", "style.qss"))
        if os.path.exists(qss_path):
            with open(qss_path, "r", encoding="utf-8") as f:
                self.setStyleSheet(f.read())

    # ── Connection handlers ───────────────────────────────────────

    def _on_connect(self, ip: str, port: int):
        success = self.client.connect(ip, port)
        if not success:
            self.connection_panel.set_connection_state(False)

    def _on_client_connected(self):
        self.connection_panel.set_connection_state(True)
        self._set_status("Connected — reading parameters…", ok=True)
        self._enqueue_sequential(build_get_meta_request(), "GET_META")
        self._enqueue_sequential(build_status_request(), "STATUS")

    def _on_client_disconnected(self):
        self.connection_panel.set_connection_state(False)
        self._cancel_watchdog()
        self._cmd_queue.clear()
        self._pending_cmds.clear()
        self._response_buffer.clear()
        self._set_status("Disconnected")

    def _on_disconnect(self):
        self.client.disconnect()

    def _on_reconnecting(self, attempt: int, delay_sec: int):
        self.connection_panel.set_reconnecting(attempt, delay_sec)
        self._set_status(
            f"Reconnecting… attempt {attempt}/3, next in {delay_sec} s"
        )

    def _on_error(self, msg: str):
        self._cancel_watchdog()
        self._cmd_queue.clear()
        self._pending_cmds.clear()
        self.connection_panel.set_connection_state(False)
        self._set_status(f"Error: {msg}", error=True)

    # ── Protocol: send ────────────────────────────────────────────

    def _enqueue_and_send(self, cmd_bytes: bytes, response_type: str):
        """Send a command and enqueue the expected response type."""
        if not self.client.send_command(cmd_bytes):
            return
        self._packets_sent += 1
        self._update_packet_ui()
        was_empty = len(self._cmd_queue) == 0
        self._cmd_queue.append(response_type)
        if was_empty:
            self._start_watchdog()

    def _enqueue_sequential(self, cmd_bytes: bytes, response_type: str):
        """Queue a command to be sent only after all prior responses are received.

        Prevents the server from receiving multiple commands in one recv() call,
        which would cause it to discard all but the first."""
        self._pending_cmds.append((cmd_bytes, response_type))
        if not self._cmd_queue:
            self._fire_next_pending()

    def _fire_next_pending(self):
        """Send the next queued command if the pipeline is idle."""
        if self._pending_cmds and not self._cmd_queue:
            cmd_bytes, rtype = self._pending_cmds.popleft()
            self._enqueue_and_send(cmd_bytes, rtype)

    def _send_status_request(self):
        self._enqueue_and_send(build_status_request(), "STATUS")

    def _send_set_param(self, param_idx: int, value: int):
        mode = self.param_editor.get_current_mode_index()
        self._enqueue_and_send(
            build_set_param_request(mode, param_idx, value), "SET_PARAM"
        )

    def _send_set_gain_block(self, mode: int):
        values = self.param_editor.get_all_params()
        self._enqueue_and_send(
            build_set_gain_block_request(mode, values), "SET_GAIN_BLOCK"
        )

    def _send_verify(self):
        self._enqueue_and_send(build_verify_request(), "VERIFY")

    def _send_upload(self, file_path: str):
        try:
            with open(file_path, "rb") as f:
                file_data = f.read()

            try:
                mode_params, version_str, header_id_str = parse_config_file(file_data)
            except ValueError as e:
                self.file_ops.set_status(f"File error: {e}", success=False)
                self._set_status(f"Upload error: {e}", error=True)
                return

            # Sync UI header/version from file so export after download stays consistent
            self.file_ops.set_header_id(header_id_str)
            _, major, minor, fix = parse_version_string(version_str.encode("ascii"))
            self.file_ops.set_version_parts(major, minor, fix)
            self.file_ops.set_version_preview(version_str)

            for mode, values in enumerate(mode_params):
                self._enqueue_sequential(
                    build_set_gain_block_request(mode, values), "SET_GAIN_BLOCK"
                )
            self._enqueue_sequential(
                build_set_meta_request(header_id_str, version_str), "SET_META"
            )
            self._enqueue_sequential(build_verify_request(), "VERIFY")

            self.file_ops.set_status(f"Uploading: {os.path.basename(file_path)}…")
            self._set_status(f"Uploading {os.path.basename(file_path)}…")
        except Exception as e:
            self.file_ops.set_status(f"Upload failed: {e}", success=False)
            self._set_status(f"Upload error: {e}", error=True)

    # ── Protocol: receive ─────────────────────────────────────────

    def _on_data_received(self, data: bytes):
        self._packets_received += 1
        self._update_packet_ui()
        # Guard against runaway buffer
        if len(self._response_buffer) + len(data) > _MAX_BUFFER_BYTES:
            self._response_buffer.clear()
            self._set_status("Buffer overflow — discarded", error=True)
            return

        self._response_buffer.extend(data)

        if not self._cmd_queue:
            self._response_buffer.clear()
            return

        pending = self._cmd_queue[0]

        if pending == "GET_META":
            if len(self._response_buffer) >= _RSP_SIZE:
                buf = bytes(self._response_buffer[:_RSP_SIZE])
                self._process_get_meta_response(buf)
                del self._response_buffer[:_RSP_SIZE]
                self._cmd_queue.popleft()
                self._cancel_watchdog()
                if self._cmd_queue:
                    self._start_watchdog()
                self._fire_next_pending()

        elif pending == "STATUS":
            if len(self._response_buffer) >= _RSP_SIZE:
                self._process_status_response(bytes(self._response_buffer[:_RSP_SIZE]))
                del self._response_buffer[:_RSP_SIZE]
                self._cmd_queue.popleft()
                self._cancel_watchdog()
                if self._cmd_queue:
                    self._start_watchdog()
                self._fire_next_pending()

        elif pending in ("SET_PARAM", "SET_GAIN_BLOCK", "SET_META"):
            if len(self._response_buffer) >= _RSP_SIZE:
                buf = bytes(self._response_buffer[:_RSP_SIZE])
                if buf[1] != 0:
                    self._set_status(
                        f"{pending} error: code={buf[2]}", error=True
                    )
                del self._response_buffer[:_RSP_SIZE]
                self._cmd_queue.popleft()
                self._cancel_watchdog()
                if self._cmd_queue:
                    self._start_watchdog()
                self._fire_next_pending()

        elif pending == "VERIFY":
            if len(self._response_buffer) >= _RSP_SIZE:
                buf = bytes(self._response_buffer[:_RSP_SIZE])
                # data[8] = buf[11]: crc_match byte from AudioCmdHandleVerify
                match = buf[11]
                if buf[1] == 0 and match:
                    self._set_status("VERIFY: CRC32 OK ✓", ok=True)
                else:
                    self._set_status("VERIFY: CRC32 MISMATCH", error=True)
                del self._response_buffer[:_RSP_SIZE]
                self._cmd_queue.popleft()
                self._cancel_watchdog()
                if self._cmd_queue:
                    self._start_watchdog()
                self._fire_next_pending()
        else:
            self._response_buffer.clear()

    def _process_get_meta_response(self, data: bytes):
        header_id_str, version_str = parse_get_meta_response(data)
        if header_id_str:
            self.file_ops.set_header_id(header_id_str)
        if version_str and version_str.startswith('V'):
            try:
                _, major, minor, fix = parse_version_string(version_str.encode('ascii'))
                self.file_ops.set_version_parts(major, minor, fix)
            except Exception:
                pass
            self.file_ops.set_version_preview(version_str)

    def _process_status_response(self, data: bytes):
        try:
            modes = parse_all_modes_response(data)
            self._mode_params = modes
            self.param_editor.set_all_mode_params(modes)   # clears dirty too
            self.param_editor.set_status(f"Received all {NUM_MODES} mode sets")
            self._set_status(
                f"Read OK — {NUM_MODES} modes ({len(data)} B)", ok=True
            )
            # Restore version/header metadata from flash if device has it.
            # set_version_parts must come BEFORE set_version_preview because
            # set_version_parts triggers _refresh_version_preview which resets
            # the preview to the placeholder "──" string.
            version_str = parse_meta_from_status_response(data)
            if version_str and version_str.startswith('V'):
                try:
                    _, major, minor, fix = parse_version_string(version_str.encode('ascii'))
                    self.file_ops.set_version_parts(major, minor, fix)
                except Exception:
                    pass
                self.file_ops.set_version_preview(version_str)
            if self._export_after_download:
                self._export_after_download = False
                QTimer.singleShot(0, self._on_export)
        except Exception as e:
            self._export_after_download = False
            self._set_status(f"Parse error: {e}", error=True)

    # ── UI action handlers ────────────────────────────────────────

    def _on_read(self):
        if not self.client.is_connected:
            self._set_status("Not connected", error=True)
            return
        self._enqueue_sequential(build_get_meta_request(), "GET_META")
        self._enqueue_sequential(build_status_request(), "STATUS")

    def _on_apply(self):
        if not self.client.is_connected:
            self._set_status("Not connected", error=True)
            return
        from datetime import datetime as _dt
        mode = self.param_editor.get_current_mode_index()
        values = self.param_editor.get_all_params()

        ma, mi, fx = self.file_ops.get_version_parts()
        ts = _dt.now()
        ver_preview = (f"V{ma:02d}.{mi:02d}.{fx:02d} "
                       f"{ts.day:02d}.{ts.month:02d}.{ts.year:04d} "
                       f"{ts.hour:02d}:{ts.minute:02d}:{ts.second:02d}")
        header_id = self.file_ops.get_header_id()

        self._enqueue_sequential(build_set_gain_block_request(mode, values), "SET_GAIN_BLOCK")
        self._enqueue_sequential(build_set_meta_request(header_id, ver_preview), "SET_META")
        self._enqueue_sequential(build_verify_request(), "VERIFY")

        self.file_ops.set_version_preview(ver_preview)
        self._set_status(f"Applying {MODE_SETS[mode]}…  {ver_preview}")

    def _on_apply_all(self):
        """Apply all 6 mode parameter blocks to the device sequentially."""
        if not self.client.is_connected:
            self._set_status("Not connected", error=True)
            return
        from datetime import datetime as _dt
        # get_all_mode_params() saves the currently visible tab before returning
        all_modes = self.param_editor.get_all_mode_params()
        for mode, values in enumerate(all_modes):
            self._enqueue_sequential(build_set_gain_block_request(mode, values), "SET_GAIN_BLOCK")
        ma, mi, fx = self.file_ops.get_version_parts()
        ts = _dt.now()
        ver_preview = (f"V{ma:02d}.{mi:02d}.{fx:02d} "
                       f"{ts.day:02d}.{ts.month:02d}.{ts.year:04d} "
                       f"{ts.hour:02d}:{ts.minute:02d}:{ts.second:02d}")
        header_id = self.file_ops.get_header_id()
        self._enqueue_sequential(build_set_meta_request(header_id, ver_preview), "SET_META")
        self._enqueue_sequential(build_verify_request(), "VERIFY")
        self.file_ops.set_version_preview(ver_preview)
        self.param_editor.clear_dirty()
        self._set_status(f"Applying all {NUM_MODES} modes…")

    def _save_all_modes_to_internal(self):
        """Flush current UI mode into _mode_params before apply-all."""
        cur = self.param_editor.get_current_mode_index()
        vals = self.param_editor.get_all_params()
        for i, v in enumerate(vals):
            self._mode_params[cur][i] = v

    def _on_upload_file(self, file_path: str):
        self._set_status(f"Uploading: {os.path.basename(file_path)}")
        self._send_upload(file_path)

    def _on_download(self):
        if not self.client.is_connected:
            self._set_status("Not connected", error=True)
            return
        self._export_after_download = True
        self._set_status("Downloading parameters from device…")
        self._enqueue_sequential(build_get_meta_request(), "GET_META")
        self._enqueue_sequential(build_status_request(), "STATUS")

    # ── File export ───────────────────────────────────────────────

    def _on_export(self, _ignored: str = ""):
        settings = QSettings(_SETTINGS_ORG, _SETTINGS_APP)
        last_dir = settings.value("last_export_dir", "")
        default_path = os.path.join(last_dir, "audio_parameters.bin") if last_dir else "audio_parameters.bin"

        try:
            file_path, _ = QFileDialog.getSaveFileName(
                self, "Export Parameters", default_path,
                "Binary Files (*.bin);;All Files (*.*)"
            )
        except Exception:
            file_path = default_path

        if not file_path:
            return

        try:
            all_modes = self.param_editor.get_all_mode_params()
            header_id = self.file_ops.get_header_id()
            major, minor, fix = self.file_ops.get_version_parts()

            # Preserve timestamp from version_preview if it was set by import/upload.
            # version_preview contains "──" placeholders only when no file was loaded.
            from datetime import datetime as _dt
            ts = _dt.now()
            preview_text = self.file_ops.get_version_preview()
            if preview_text and '──' not in preview_text:
                try:
                    parts = preview_text.strip().split(' ')
                    d = parts[1].split('.')   # DD.MM.YYYY
                    t = parts[2].split(':')   # HH:MM:SS
                    ts = _dt(int(d[2]), int(d[1]), int(d[0]),
                             int(t[0]), int(t[1]), int(t[2]))
                except (IndexError, ValueError):
                    pass

            data = build_config_file(all_modes, header_id, major, minor, fix,
                                     timestamp=ts)

            # Atomic write: temp file → os.replace()
            dir_name = os.path.dirname(file_path) or "."
            with tempfile.NamedTemporaryFile(
                dir=dir_name, delete=False, suffix=".tmp"
            ) as tmp:
                tmp.write(data)
                tmp_path = tmp.name
            os.replace(tmp_path, file_path)

            settings.setValue("last_export_dir", os.path.dirname(file_path))

            ver_preview = (f"V{major:02d}.{minor:02d}.{fix:02d} "
                           f"{ts.day:02d}.{ts.month:02d}.{ts.year:04d} "
                           f"{ts.hour:02d}:{ts.minute:02d}:{ts.second:02d}")
            self.file_ops.set_version_preview(ver_preview)
            self.file_ops.set_status(f"Exported: {os.path.basename(file_path)}")
            self._set_status(f"Exported 300 B  •  {ver_preview}", ok=True)

        except Exception as e:
            self.file_ops.set_status(f"Export failed: {e}", success=False)
            self._set_status(f"Export error: {e}", error=True)

    # ── File import ───────────────────────────────────────────────

    def _on_import(self, _ignored: str = ""):
        settings = QSettings(_SETTINGS_ORG, _SETTINGS_APP)
        last_dir = settings.value("last_import_dir", "")

        try:
            file_path, _ = QFileDialog.getOpenFileName(
                self, "Import Parameters", last_dir,
                "Binary Files (*.bin);;All Files (*.*)"
            )
        except Exception:
            self._set_status("File dialog unavailable", error=True)
            return

        if not file_path:
            return

        try:
            with open(file_path, "rb") as f:
                raw = f.read()

            parsed_modes, version_str, header_id_str = parse_config_file(raw)
            validate_params(parsed_modes)   # raises ValueError if out of range

            settings.setValue("last_import_dir", os.path.dirname(file_path))

            self._mode_params = parsed_modes
            self.param_editor.set_all_mode_params(parsed_modes)

            self.file_ops.set_header_id(header_id_str)
            _, major, minor, fix = parse_version_string(version_str.encode("ascii"))
            self.file_ops.set_version_parts(major, minor, fix)
            self.file_ops.set_version_preview(version_str)

            self.param_editor.set_status(f"Imported {NUM_MODES} mode sets")
            self.file_ops.set_status(f"Imported: {os.path.basename(file_path)}")
            self._set_status(f"Imported 300 B  •  {version_str}", ok=True)

        except ValueError as e:
            self.file_ops.set_status(f"Import failed: {e}", success=False)
            self._set_status(f"Import error: {e}", error=True)
        except Exception as e:
            self.file_ops.set_status(f"Import failed: {e}", success=False)
            self._set_status(f"Import error: {e}", error=True)

    # ── Parameter change handler ──────────────────────────────────

    def on_param_changed(self, mode: int, param_idx: int, value: int):
        if 0 <= mode < NUM_MODES and 0 <= param_idx < NUM_PARAMS:
            self._mode_params[mode][param_idx] = value

    # ── Lifecycle ─────────────────────────────────────────────────

    def closeEvent(self, event):
        self.client.disconnect()
        event.accept()


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("Audio DSP Configuration")
    app.setOrganizationName(_SETTINGS_ORG)

    _icon_path = os.path.join(
        getattr(sys, "_MEIPASS", os.path.dirname(os.path.abspath(__file__))),
        "resources", "plane.png"
    )
    if os.path.exists(_icon_path):
        app.setWindowIcon(QIcon(_icon_path))

    window = AudioDSPConfigApp()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()




