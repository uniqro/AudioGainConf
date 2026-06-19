"""
Connection Panel — compact single-row toolbar.
"""

from PyQt5.QtWidgets import (
    QWidget, QHBoxLayout, QVBoxLayout,
    QLineEdit, QPushButton, QLabel,
)
from PyQt5.QtCore import pyqtSignal, Qt


class ConnectionPanel(QWidget):
    connected = pyqtSignal(str, int)
    disconnected = pyqtSignal()
    connection_error = pyqtSignal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._setup_ui()

    def _setup_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(8, 6, 8, 6)
        root.setSpacing(3)

        # ── Row 1: controls ───────────────────────────────────
        row = QHBoxLayout()
        row.setSpacing(8)

        self.status_dot = QLabel("●")
        self.status_dot.setStyleSheet("color: #444466; font-size: 20px;")
        self.status_dot.setFixedWidth(22)

        lbl_ip = QLabel("IP:")
        lbl_ip.setStyleSheet("color: #7090b0; font-size: 12px;")
        self.ip_input = QLineEdit("192.168.1.100")
        self.ip_input.setFixedHeight(28)
        self.ip_input.setMinimumWidth(130)
        self.ip_input.setMaximumWidth(160)

        lbl_port = QLabel("Port:")
        lbl_port.setStyleSheet("color: #7090b0; font-size: 12px;")
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
        row.addWidget(self.ip_input)
        row.addWidget(lbl_port)
        row.addWidget(self.port_input)
        row.addWidget(self.connect_btn)
        row.addWidget(self.disconnect_btn)
        row.addStretch()
        row.addWidget(self.packet_label)
        root.addLayout(row)

        # ── Row 2: status text ────────────────────────────────
        self.status_label = QLabel("Disconnected")
        self.status_label.setStyleSheet(
            "color: #444466; font-size: 11px; padding-left: 30px;"
        )
        root.addWidget(self.status_label)

    # ── Handlers ──────────────────────────────────────────────

    def _on_connect(self):
        ip = self.ip_input.text().strip()
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
        self.ip_input.setEnabled(False)
        self.port_input.setEnabled(False)
        self.status_dot.setStyleSheet("color: #f0ad4e; font-size: 20px;")
        self.status_label.setText("Connecting…")
        self.status_label.setStyleSheet(
            "color: #f0ad4e; font-size: 11px; padding-left: 30px;"
        )
        self.connected.emit(ip, port)

    def _on_disconnect(self):
        self.disconnect_btn.setEnabled(False)
        self.status_dot.setStyleSheet("color: #f0ad4e; font-size: 20px;")
        self.status_label.setText("Disconnecting…")
        self.disconnected.emit()

    def set_connection_state(self, connected: bool):
        self.connect_btn.setEnabled(not connected)
        self.disconnect_btn.setEnabled(connected)
        self.ip_input.setEnabled(not connected)
        self.port_input.setEnabled(not connected)

        if connected:
            self.status_dot.setStyleSheet("color: #4CAF50; font-size: 20px;")
            self.status_label.setText(
                f"Connected  —  {self.ip_input.text()}:{self.port_input.text()}"
            )
            self.status_label.setStyleSheet(
                "color: #4CAF50; font-size: 11px; font-weight: bold; padding-left: 30px;"
            )
        else:
            self.status_dot.setStyleSheet("color: #444466; font-size: 20px;")
            self.status_label.setText("Disconnected")
            self.status_label.setStyleSheet(
                "color: #444466; font-size: 11px; padding-left: 30px;"
            )

    def update_packet_counts(self, sent: int, received: int):
        self.packet_label.setText(f"Tx: {sent}   Rx: {received}")

    def set_active_mode(self, mode: str):
        pass  # shown in main status bar