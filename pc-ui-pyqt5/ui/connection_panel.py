"""
Connection Panel — IP history, elapsed-time display, reconnecting state.
"""

import time
from PyQt5.QtWidgets import (
    QWidget, QHBoxLayout, QVBoxLayout,
    QComboBox, QPushButton, QLabel,
)
from PyQt5.QtCore import pyqtSignal, Qt, QTimer, QSettings

_SETTINGS_ORG  = "DSPConfig"
_SETTINGS_APP  = "AudioDSP"
_MAX_HISTORY   = 5


class ConnectionPanel(QWidget):
    connected        = pyqtSignal(str, int)
    disconnected     = pyqtSignal()
    connection_error = pyqtSignal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._connected_since: float = 0.0
        self._setup_ui()
        self._setup_elapsed_timer()
        self._load_history()

    # ── Build UI ──────────────────────────────────────────────────

    def _setup_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(8, 6, 8, 6)
        root.setSpacing(3)

        row = QHBoxLayout()
        row.setSpacing(8)

        self.status_dot = QLabel("●")
        self.status_dot.setStyleSheet("color: #444466; font-size: 20px;")
        self.status_dot.setFixedWidth(22)

        lbl_ip = QLabel("IP:")
        lbl_ip.setStyleSheet("color: #7090b0; font-size: 12px;")

        # Editable combo with IP history
        self.ip_combo = QComboBox()
        self.ip_combo.setEditable(True)
        self.ip_combo.setInsertPolicy(QComboBox.NoInsert)
        self.ip_combo.setFixedHeight(28)
        self.ip_combo.setMinimumWidth(150)
        self.ip_combo.setMaximumWidth(180)
        self.ip_combo.lineEdit().setPlaceholderText("192.168.1.100")

        lbl_port = QLabel("Port:")
        lbl_port.setStyleSheet("color: #7090b0; font-size: 12px;")

        from PyQt5.QtWidgets import QLineEdit
        self.port_input = QLineEdit("5558")
        self.port_input.setFixedHeight(28)
        self.port_input.setFixedWidth(58)

        self.connect_btn = QPushButton("CONNECT")
        self.connect_btn.setObjectName("connectBtn")
        self.connect_btn.setFixedHeight(28)
        self.connect_btn.clicked.connect(self._on_connect)

        self.disconnect_btn = QPushButton("DISCONNECT")
        self.disconnect_btn.setObjectName("disconnectBtn")
        self.disconnect_btn.setFixedHeight(28)
        self.disconnect_btn.setEnabled(False)
        self.disconnect_btn.clicked.connect(self._on_disconnect)

        self.packet_label = QLabel("Tx: 0   Rx: 0")
        self.packet_label.setStyleSheet(
            "color: #446688; font-size: 11px; min-width: 95px;"
        )
        self.packet_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)

        row.addWidget(self.status_dot)
        row.addWidget(lbl_ip)
        row.addWidget(self.ip_combo)
        row.addWidget(lbl_port)
        row.addWidget(self.port_input)
        row.addWidget(self.connect_btn)
        row.addWidget(self.disconnect_btn)
        row.addStretch()
        row.addWidget(self.packet_label)
        root.addLayout(row)

        self.status_label = QLabel("Disconnected")
        self.status_label.setStyleSheet(
            "color: #444466; font-size: 11px; padding-left: 30px;"
        )
        root.addWidget(self.status_label)

    def _setup_elapsed_timer(self):
        self._elapsed_timer = QTimer(self)
        self._elapsed_timer.setInterval(1000)
        self._elapsed_timer.timeout.connect(self._update_elapsed)

    # ── QSettings: IP history ─────────────────────────────────────

    def _load_history(self):
        s = QSettings(_SETTINGS_ORG, _SETTINGS_APP)
        history = s.value("ip_history", ["192.168.1.100"])
        if isinstance(history, str):
            history = [history]
        self.ip_combo.blockSignals(True)
        self.ip_combo.clear()
        for ip in history[:_MAX_HISTORY]:
            self.ip_combo.addItem(ip)
        if self.ip_combo.count() == 0:
            self.ip_combo.addItem("192.168.1.100")
        self.ip_combo.setCurrentIndex(0)
        self.ip_combo.blockSignals(False)

        port = s.value("last_port", "5558")
        self.port_input.setText(str(port))

    def _save_to_history(self, ip: str, port: str):
        s = QSettings(_SETTINGS_ORG, _SETTINGS_APP)
        history = s.value("ip_history", [])
        if isinstance(history, str):
            history = [history]
        if ip in history:
            history.remove(ip)
        history.insert(0, ip)
        history = history[:_MAX_HISTORY]
        s.setValue("ip_history", history)
        s.setValue("last_port", port)

        self.ip_combo.blockSignals(True)
        self.ip_combo.clear()
        for h in history:
            self.ip_combo.addItem(h)
        self.ip_combo.setCurrentIndex(0)
        self.ip_combo.blockSignals(False)

    # ── Handlers ─────────────────────────────────────────────────

    def _on_connect(self):
        ip = self.ip_combo.currentText().strip()
        if not ip:
            self.connection_error.emit("Please enter an IP address")
            return
        try:
            port = int(self.port_input.text().strip())
            if not (1 <= port <= 65535):
                raise ValueError
        except ValueError:
            self.connection_error.emit("Invalid port number")
            return

        self.connect_btn.setEnabled(False)
        self.ip_combo.setEnabled(False)
        self.port_input.setEnabled(False)
        self.status_dot.setStyleSheet("color: #f0ad4e; font-size: 20px;")
        self.status_label.setText("Connecting…")
        self.status_label.setStyleSheet(
            "color: #f0ad4e; font-size: 11px; padding-left: 30px;"
        )
        self.connected.emit(ip, port)

    def _on_disconnect(self):
        self.disconnect_btn.setEnabled(False)
        self._elapsed_timer.stop()
        self.status_dot.setStyleSheet("color: #f0ad4e; font-size: 20px;")
        self.status_label.setText("Disconnecting…")
        self.disconnected.emit()

    # ── Public API ────────────────────────────────────────────────

    def set_connection_state(self, is_connected: bool):
        self.connect_btn.setEnabled(not is_connected)
        self.disconnect_btn.setEnabled(is_connected)
        self.ip_combo.setEnabled(not is_connected)
        self.port_input.setEnabled(not is_connected)

        if is_connected:
            ip   = self.ip_combo.currentText().strip()
            port = self.port_input.text().strip()
            self._save_to_history(ip, port)
            self._connected_since = time.monotonic()
            self._elapsed_timer.start()
            self.status_dot.setStyleSheet("color: #4CAF50; font-size: 20px;")
            self.status_label.setText(f"Connected  —  {ip}:{port}  |  00:00:00")
            self.status_label.setStyleSheet(
                "color: #4CAF50; font-size: 11px; font-weight: bold; padding-left: 30px;"
            )
        else:
            self._elapsed_timer.stop()
            self.status_dot.setStyleSheet("color: #444466; font-size: 20px;")
            self.status_label.setText("Disconnected")
            self.status_label.setStyleSheet(
                "color: #444466; font-size: 11px; padding-left: 30px;"
            )

    def set_reconnecting(self, attempt: int, delay_sec: int):
        """Show auto-reconnect state (called by TCPClient reconnecting signal)."""
        self.status_dot.setStyleSheet("color: #f0ad4e; font-size: 20px;")
        self.status_label.setText(
            f"Reconnecting… (attempt {attempt}, next in {delay_sec} s)"
        )
        self.status_label.setStyleSheet(
            "color: #f0ad4e; font-size: 11px; padding-left: 30px;"
        )

    def update_packet_counts(self, sent: int, received: int):
        self.packet_label.setText(f"Tx: {sent}   Rx: {received}")

    def set_active_mode(self, mode: str):
        pass

    # ── Elapsed timer ─────────────────────────────────────────────

    def _update_elapsed(self):
        elapsed = int(time.monotonic() - self._connected_since)
        h, rem  = divmod(elapsed, 3600)
        m, s    = divmod(rem, 60)
        ip   = self.ip_combo.currentText().strip()
        port = self.port_input.text().strip()
        self.status_label.setText(
            f"Connected  —  {ip}:{port}  |  {h:02d}:{m:02d}:{s:02d}"
        )
