"""
Audio DSP Configuration UI - Main Application
PyQt5-based desktop application for configuring Audio DSP parameters
via TCP network connection.

Features:
- TCP connection to DSP device (port 5558)
- 15 parameters across 6 mode sets (AM, FM, HQII, 9600, 9600A, SK2)
- Read/Write parameters over TCP
- Upload/download parameter files
- Export/import local configuration files

Usage:
    python main.py
"""

import sys
import os
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QSplitter, QScrollArea, QLabel, QMessageBox
)
from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtGui import QFont

from tcp_client import TCPClient
from audio_protocol import (
    MODE_SETS, NUM_MODES, NUM_PARAMS, CMD_STATUS, CMD_SET_GAIN_BLOCK,
    CMD_SET_PARAM, CMD_VERIFY,
    build_status_request, build_set_param_request,
    build_set_gain_block_request, build_verify_request,
    build_upload_request, build_full_upload,
    parse_status_response, parse_all_modes_response,
    build_config_file, parse_config_file, parse_version_string,
)
from ui.connection_panel import ConnectionPanel
from ui.param_editor import ParamEditor
from ui.file_ops import FileOps


class AudioDSPConfigApp(QMainWindow):
    """Main window for Audio DSP Configuration application."""

    def __init__(self):
        super().__init__()
        self._packets_sent = 0
        self._packets_received = 0
        self._mode_params = [[0] * NUM_PARAMS for _ in range(NUM_MODES)]
        self._pending_response = None  # Track expected response type
        self._response_buffer = bytearray()
        self._setup_ui()
        self._setup_client()
        self._load_stylesheet()

    def _setup_ui(self):
        """Build the main window UI."""
        self.setWindowTitle("Audio DSP Configuration Utility")
        self.setMinimumSize(1200, 800)
        self.resize(1300, 900)

        # ── Central widget ─────────────────────────────────────
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QVBoxLayout(central)
        main_layout.setContentsMargins(8, 8, 8, 8)
        main_layout.setSpacing(8)

        # ── Layout: compact top row | param editor below ──
        splitter = QSplitter(Qt.Vertical)

        # ── Top bar: ConnectionPanel + FileOps (compact) ──
        top_splitter = QSplitter(Qt.Horizontal)
        top_splitter.setContentsMargins(0, 0, 0, 0)
        top_splitter.setStretchFactor(0, 1)  # ConnectionPanel
        top_splitter.setStretchFactor(1, 1)  # FileOps

        self.connection_panel = ConnectionPanel()
        self.file_ops = FileOps()
        top_splitter.addWidget(self.connection_panel)
        top_splitter.addWidget(self.file_ops)

        # ── Bottom: ParamEditor (fills most screen) ──
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QScrollArea.Shape.NoFrame)
        self.param_editor = ParamEditor()
        scroll.setWidget(self.param_editor)

        splitter.addWidget(top_splitter)
        splitter.addWidget(scroll)
        splitter.setStretchFactor(0, 1)   # Top bar
        splitter.setStretchFactor(1, 4)   # ParamEditor
        
        # Top bar smaller, Audio Settings larger
        splitter.setSizes([110, 690])

        main_layout.addWidget(splitter)

        # ── Status bar ─────────────────────────────────────────
        status = QHBoxLayout()
        status.setSpacing(20)
        status.setContentsMargins(8, 2, 8, 2)

        self.status_label = QLabel("Ready")
        self.status_label.setStyleSheet("color: #888888; font-size: 12px;")
        
        self.connection_status = QLabel("Status: Disconnected")
        self.connection_status.setStyleSheet("color: #888888; font-size: 12px; font-weight: bold;")
        
        self.version_label = QLabel("Audio DSP Config v1.0")
        self.version_label.setAlignment(Qt.AlignRight)
        self.version_label.setStyleSheet("color: #666680; font-size: 10px;")

        status.addWidget(self.status_label)
        status.addStretch()
        status.addWidget(self.connection_status)
        status.addStretch()
        status.addWidget(self.version_label)

        main_layout.addLayout(status)

        # Connect UI signals
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

    def _setup_client(self):
        """Initialize TCP client and wire signals."""
        self.client = TCPClient(self)
        self.client.connected.connect(self._on_client_connected)
        self.client.disconnected.connect(self._on_client_disconnected)
        self.client.data_received.connect(self._on_data_received)
        self.client.error.connect(self._on_error)

    def _resource_path(self, relative_path):
        """Get absolute path to resource, works for dev and PyInstaller."""
        try:
            base_path = sys._MEIPASS
        except Exception:
            base_path = os.path.abspath(".")
        return os.path.join(base_path, relative_path)

    def _load_stylesheet(self):
        """Load dark theme stylesheet."""
        qss_path = self._resource_path(
            os.path.join("resources", "style.qss")
        )
        if os.path.exists(qss_path):
            with open(qss_path, "r", encoding="utf-8") as f:
                self.setStyleSheet(f.read())

    # ── Connection handlers ───────────────────────────────────────

    def _on_connect(self, ip: str, port: int):
        """Handle connect from ConnectionPanel."""
        success = self.client.connect(ip, port)
        if not success:
            self.connection_panel.set_connection_state(False)

    def _on_client_connected(self):
        """Handle successful TCP connection."""
        self.connection_panel.set_connection_state(True)
        self.connection_status.setText("Status: Connected")
        self.connection_status.setStyleSheet("color: #4CAF50; font-size: 12px; font-weight: bold;")
        self.status_label.setText("Connected to device")
        self.status_label.setStyleSheet("color: #4CAF50; font-size: 12px; font-weight: bold;")
        self._send_status_request()

    def _on_client_disconnected(self):
        """Handle TCP disconnect."""
        self.connection_panel.set_connection_state(False)
        self.connection_status.setText("Status: Disconnected")
        self.connection_status.setStyleSheet("color: #888888; font-size: 12px; font-weight: bold;")
        self.status_label.setText("Disconnected")
        self.status_label.setStyleSheet("color: #888888; font-size: 12px;")

    def _on_disconnect(self):
        """Handle disconnect from ConnectionPanel."""
        self.client.disconnect()

    def _on_error(self, msg: str):
        """Handle error from TCP client."""
        QMessageBox.warning(self, "Error", msg)
        self.connection_panel.set_connection_state(False)
        self._pending_response = None

    # ── Protocol communication ────────────────────────────────────

    def _send_status_request(self):
        """Send STATUS command to request current parameters."""
        self.client.send_command(build_status_request())
        self._packets_sent += 1
        self._pending_response = "STATUS"
        self.connection_panel.update_packet_counts(
            self._packets_sent, self._packets_received
        )

    def _send_set_param(self, param_idx: int, value: int):
        """Send SET_PARAM command for a single parameter."""
        mode = self.param_editor.get_current_mode_index()
        data = build_set_param_request(mode, param_idx, value)
        self.client.send(data)
        self._packets_sent += 1
        self._pending_response = "SET_PARAM"

    def _send_set_gain_block(self, mode: int):
        """Send SET_GAIN_BLOCK command for a mode set."""
        values = self.param_editor.get_all_params()
        data = build_set_gain_block_request(mode, values)
        self.client.send(data)
        self._packets_sent += 1
        self._pending_response = "SET_GAIN_BLOCK"

    def _send_verify(self, mode: int):
        """Send VERIFY command."""
        data = build_verify_request(mode)
        self.client.send(data)
        self._packets_sent += 1
        self._pending_response = "VERIFY"

    def _send_upload(self, file_path: str):
        """Send upload command with file contents."""
        try:
            with open(file_path, "rb") as f:
                file_data = f.read()

            # If file is 90 bytes (6 modes × 15 params), parse it
            if len(file_data) == NUM_MODES * NUM_PARAMS:
                data = build_upload_request(file_data)
            elif len(file_data) >= NUM_MODES * NUM_PARAMS:
                # Extract mode data from file
                mode_data = file_data[:NUM_MODES * NUM_PARAMS]
                data = build_upload_request(mode_data)
            else:
                self.file_ops.set_status(
                    f"File too small: {len(file_data)} bytes (expected {NUM_MODES * NUM_PARAMS})",
                    success=False
                )
                return

            self.client.send(data)
            self._packets_sent += 1
            self.file_ops.set_status(f"Uploaded: {os.path.basename(file_path)}")
        except Exception as e:
            self.file_ops.set_status(f"Upload failed: {e}", success=False)

    def _send_download(self):
        """Request full download of all parameters."""
        self._send_status_request()

    # ── Data processing ──────────────────────────────────────────

    def _on_data_received(self, data: bytes):
        """Handle incoming data from TCP connection."""
        self._packets_received += 1
        self.connection_panel.update_packet_counts(
            self._packets_sent, self._packets_received
        )

        # Accumulate buffer for multi-packet handling
        self._response_buffer.extend(data)

        pending = self._pending_response

        if pending == "STATUS":
            # STATUS response: [0x81][status][error_code][90 bytes] = 93 bytes
            if len(self._response_buffer) >= 3 + NUM_MODES * NUM_PARAMS:
                self._process_status_response(bytes(self._response_buffer))
                self._response_buffer.clear()
                self._pending_response = None

        elif pending in ("SET_PARAM", "SET_GAIN_BLOCK"):
            # Response: [cmd|0x80][status][error_code] = 3 bytes (+ 128 byte padding)
            if len(self._response_buffer) >= 3:
                buf = bytes(self._response_buffer)
                status = buf[1]
                if status != 0:
                    error_code = buf[2]
                    self.status_label.setText(
                        f"{pending} failed: error_code={error_code}"
                    )
                    self.status_label.setStyleSheet("color: #e94560; font-size: 12px;")
                self._response_buffer.clear()
                self._pending_response = None

        elif pending == "VERIFY":
            # Response: [0x84][status][error_code][crc_stored:4][crc_computed:4][match:1]
            if len(self._response_buffer) >= 9:
                buf = bytes(self._response_buffer)
                status = buf[1]
                match = buf[8] if len(buf) > 8 else 0
                if status == 0 and match:
                    self.status_label.setText("VERIFY: CRC32 OK")
                    self.status_label.setStyleSheet("color: #4CAF50; font-size: 12px;")
                else:
                    self.status_label.setText("VERIFY: CRC32 MISMATCH")
                    self.status_label.setStyleSheet("color: #e94560; font-size: 12px;")
                self._response_buffer.clear()
                self._pending_response = None

        else:
            # Unknown/unsolicited data — discard to prevent buffer growth
            self._response_buffer.clear()

    def _process_status_response(self, data: bytes):
        """Process STATUS response ([0x81][status][error][90 bytes]) and update UI."""
        self.param_editor.set_status("Reading parameters...")

        try:
            modes = parse_all_modes_response(data)
            self._mode_params = modes
            mode_idx = self.param_editor.get_current_mode_index()
            self.param_editor.set_all_params(modes[mode_idx])
            self.param_editor.set_status(
                f"Received all {NUM_MODES} mode sets"
            )
            self.status_label.setText(
                f"Received {NUM_MODES} modes ({len(data)} bytes)"
            )
            self.status_label.setStyleSheet("color: #4FC3F7; font-size: 12px;")
        except Exception as e:
            self.status_label.setText(f"Parse error: {e}")
            self.status_label.setStyleSheet("color: #e94560; font-size: 12px;")

    # ── UI action handlers ──────────────────────────────────────────

    def _on_read(self):
        """Triggered when Read button is clicked (also handled in param_editor)."""
        self._send_status_request()

    def _on_apply(self):
        """Triggered when Apply button is clicked."""
        from datetime import datetime as _dt
        mode = self.param_editor.get_current_mode_index()
        self._send_set_gain_block(mode)

        # Update version preview with current timestamp (UI only, no file write)
        ma, mi, fx = self.file_ops.get_version_parts()
        ts = _dt.now()
        ver_preview = (f"V{ma:02d}.{mi:02d}.{fx:02d} "
                       f"{ts.day:02d}.{ts.month:02d}.{ts.year:04d} "
                       f"{ts.hour:02d}:{ts.minute:02d}:{ts.second:02d}")
        self.file_ops.set_version_preview(ver_preview)

        self.param_editor.set_status(f"Applied to {MODE_SETS[mode]}")
        self.status_label.setText(f"Applied {MODE_SETS[mode]}  •  {ver_preview}")
        self.status_label.setStyleSheet("color: #4CAF50; font-size: 12px;")

        QTimer.singleShot(100, lambda: self._send_verify(mode))

    def _on_upload_file(self, file_path: str):
        """Handle file upload from FileOps."""
        self.status_label.setText(f"Uploading: {os.path.basename(file_path)}")
        self._send_upload(file_path)

    def _on_download(self):
        """Handle download from FileOps."""
        self.status_label.setText("Downloading all parameters...")
        self._send_download()

    def _on_export(self, file_path: str = ""):
        """Handle export to local file."""
        try:
            from PyQt5.QtWidgets import QFileDialog
            
            # Get file path - try QFileDialog, fallback to default
            if not file_path or file_path == "":
                try:
                    file_path, _ = QFileDialog.getSaveFileName(
                        self,
                        "Export Parameters",
                        "audio_parameters.bin",
                        "Binary Files (*.bin);;All Files (*.*)"
                    )
                except Exception:
                    # QFileDialog failed (common in PyInstaller), use default path
                    file_path = "audio_parameters.bin"
                    self.status_label.setText("Using default path (QFileDialog unavailable)")

            if file_path:
                all_modes = self.param_editor.get_all_mode_params()
                header_id = self.file_ops.get_header_id()
                major, minor, fix = self.file_ops.get_version_parts()

                data = build_config_file(all_modes, header_id, major, minor, fix)

                with open(file_path, "wb") as f:
                    f.write(data)

                # Show the timestamp that was embedded
                from datetime import datetime as _dt
                ts = _dt.now()
                ver_preview = (f"V{major:02d}.{minor:02d}.{fix:02d} "
                               f"{ts.day:02d}.{ts.month:02d}.{ts.year:04d} "
                               f"{ts.hour:02d}:{ts.minute:02d}:{ts.second:02d}")
                self.file_ops.set_version_preview(ver_preview)

                self.file_ops.set_status(f"Exported: {os.path.basename(file_path)}")
                self.status_label.setText(f"Exported 300 B  •  {ver_preview}")
        except Exception as e:
            self.file_ops.set_status(f"Export failed: {e}", success=False)
            import sys
            print(f"Export error: {e}", file=sys.stderr)

    def _on_import(self):
        """Handle import from local file."""
        try:
            from PyQt5.QtWidgets import QFileDialog
            file_path, _ = QFileDialog.getOpenFileName(
                self,
                "Import Parameters",
                "",
                "Binary Files (*.bin);;All Files (*.*)"
            )
        except Exception:
            self.status_label.setText("QFileDialog unavailable - import manual path required")
            return

        if not file_path:
            return

        try:
            with open(file_path, "rb") as f:
                data = f.read()

            parsed_modes, version_str, header_id_str = parse_config_file(data)

            self._mode_params = parsed_modes
            self.param_editor.set_all_mode_params(parsed_modes)

            self.file_ops.set_header_id(header_id_str)
            _, major, minor, fix = parse_version_string(version_str.encode('ascii'))
            self.file_ops.set_version_parts(major, minor, fix)
            self.file_ops.set_version_preview(version_str)

            self.param_editor.set_status(f"Imported {NUM_MODES} mode sets")
            self.file_ops.set_status(f"Imported: {os.path.basename(file_path)}")
            self.status_label.setText(f"Imported 300 B  •  {version_str}")
            self.status_label.setStyleSheet("color: #4CAF50; font-size: 12px;")

        except Exception as e:
            self.file_ops.set_status(f"Import failed: {e}", success=False)

    # ── Parameter change handler ──────────────────────────────────

    def on_param_changed(self, mode: int, param_idx: int, value: int):
        """Handle real-time parameter changes from slider/combobox."""
        # Only update local store; apply to device on explicit button press
        if 0 <= mode < NUM_MODES and 0 <= param_idx < NUM_PARAMS:
            self._mode_params[mode][param_idx] = value

    # ── Application lifecycle ─────────────────────────────────────

    def closeEvent(self, event):
        """Handle window close event."""
        self.client.disconnect()
        event.accept()


def main():
    """Application entry point."""
    app = QApplication(sys.argv)
    app.setApplicationName("Audio DSP Configuration")
    app.setOrganizationName("DSPConfig")

    window = AudioDSPConfigApp()
    window.show()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
