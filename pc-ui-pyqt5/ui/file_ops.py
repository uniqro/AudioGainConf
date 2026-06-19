"""
File Operations for Audio DSP Configuration UI.
Handles upload/download of parameter files and export/import of configs.
"""

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QPushButton,
    QLabel, QFileDialog, QGroupBox, QLineEdit, QSpinBox,
)
from PyQt5.QtCore import pyqtSignal, Qt


class FileOps(QWidget):
    """File operations widget for parameter file management."""

    upload_file_clicked = pyqtSignal(str)  # file path
    download_requested = pyqtSignal()
    export_clicked = pyqtSignal(str)  # file path
    import_clicked = pyqtSignal(str)  # file path

    def __init__(self, parent=None):
        super().__init__(parent)
        self._setup_ui()

    _GRP = """
        QGroupBox {
            border: 1px solid #2a3a5c;
            border-radius: 3px;
            margin-top: 4px;
            padding-top: 6px;
            font-weight: bold;
            font-size: 11px;
            color: #4FC3F7;
        }
        QGroupBox::title { subcontrol-origin: margin; left: 6px; padding: 0 4px; }
    """

    def _setup_ui(self):
        """Build the file operations UI (compact layout)."""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 3, 4, 3)
        layout.setSpacing(4)

        # ── File Info ──────────────────────────────────────────────
        info_group = QGroupBox("FILE INFO")
        info_group.setStyleSheet(self._GRP)
        info_layout = QVBoxLayout(info_group)
        info_layout.setContentsMargins(8, 2, 8, 8)
        info_layout.setSpacing(5)

        # Header ID row
        hdr_row = QHBoxLayout()
        hdr_row.setSpacing(6)
        hdr_lbl = QLabel("Header:")
        hdr_lbl.setStyleSheet("color:#7090b0; font-size:11px;")
        hdr_lbl.setFixedWidth(48)
        self.header_edit = QLineEdit("AUDIO_CFG_V1")
        self.header_edit.setMaxLength(39)
        self.header_edit.setPlaceholderText("AUDIO_CFG_V1")
        self.header_edit.setFixedHeight(24)
        self.header_edit.setStyleSheet(
            "background:#0e1828; border:1px solid #1e3a5c; border-radius:3px;"
            "padding:2px 6px; color:#dde3ee; font-size:11px;"
        )
        hdr_row.addWidget(hdr_lbl)
        hdr_row.addWidget(self.header_edit, 1)
        info_layout.addLayout(hdr_row)

        # Version spinboxes row
        ver_row = QHBoxLayout()
        ver_row.setSpacing(4)
        ver_lbl = QLabel("Version:")
        ver_lbl.setStyleSheet("color:#7090b0; font-size:11px;")
        ver_lbl.setFixedWidth(48)

        spin_style = (
            "QSpinBox { background:#0e1828; border:1px solid #1e3a5c; border-radius:3px;"
            "  padding:2px 4px; color:#dde3ee; font-size:12px; font-weight:bold; }"
            "QSpinBox:hover { border-color:#4FC3F7; }"
            "QSpinBox::up-button, QSpinBox::down-button { background:#162033; border:none; width:16px; }"
        )

        self.spin_major = QSpinBox()
        self.spin_major.setRange(0, 99)
        self.spin_major.setValue(1)
        self.spin_major.setFixedWidth(44)
        self.spin_major.setFixedHeight(24)
        self.spin_major.setStyleSheet(spin_style)

        self.spin_minor = QSpinBox()
        self.spin_minor.setRange(0, 99)
        self.spin_minor.setValue(0)
        self.spin_minor.setFixedWidth(44)
        self.spin_minor.setFixedHeight(24)
        self.spin_minor.setStyleSheet(spin_style)

        self.spin_fix = QSpinBox()
        self.spin_fix.setRange(0, 99)
        self.spin_fix.setValue(0)
        self.spin_fix.setFixedWidth(44)
        self.spin_fix.setFixedHeight(24)
        self.spin_fix.setStyleSheet(spin_style)

        dot1 = QLabel(".")
        dot2 = QLabel(".")
        dot1.setStyleSheet("color:#4FC3F7; font-weight:bold; font-size:14px;")
        dot2.setStyleSheet("color:#4FC3F7; font-weight:bold; font-size:14px;")

        ver_row.addWidget(ver_lbl)
        ver_row.addWidget(self.spin_major)
        ver_row.addWidget(dot1)
        ver_row.addWidget(self.spin_minor)
        ver_row.addWidget(dot2)
        ver_row.addWidget(self.spin_fix)
        ver_row.addStretch()
        info_layout.addLayout(ver_row)

        # Version preview label
        self.version_preview = QLabel("V01.00.00  ──.──.────  ──:──:──")
        self.version_preview.setStyleSheet(
            "color:#446688; font-size:10px; font-family:monospace; padding:2px 0;"
        )
        info_layout.addWidget(self.version_preview)

        # Connect spinboxes to update preview
        self.spin_major.valueChanged.connect(self._refresh_version_preview)
        self.spin_minor.valueChanged.connect(self._refresh_version_preview)
        self.spin_fix.valueChanged.connect(self._refresh_version_preview)

        # ── Device operations ──
        io_group = QGroupBox("DEVICE")
        io_group.setStyleSheet(self._GRP)
        io_layout = QVBoxLayout(io_group)
        io_layout.setContentsMargins(6, 6, 6, 6)
        io_layout.setSpacing(3)
        io_layout.setContentsMargins(0, 0, 0, 0)

        self.upload_btn = QPushButton("Upload to Radio")
        self.upload_btn.setObjectName("fileBtn")
        self.upload_btn.clicked.connect(self._on_upload)
        io_layout.addWidget(self.upload_btn)

        download_btn = QPushButton("Download from Radio")
        download_btn.setObjectName("fileBtn")
        download_btn.clicked.connect(self._on_download)
        io_layout.addWidget(download_btn)

        layout.addWidget(io_group)

        # ── Local operations ──
        export_group = QGroupBox("LOCAL")
        export_group.setStyleSheet(self._GRP)
        export_layout = QVBoxLayout(export_group)
        export_layout.setContentsMargins(6, 6, 6, 6)
        export_layout.setSpacing(3)
        export_layout.setContentsMargins(0, 0, 0, 0)

        self.export_btn = QPushButton("Export Params")
        self.export_btn.setObjectName("fileBtn")
        self.export_btn.clicked.connect(self._on_export)
        export_layout.addWidget(self.export_btn)

        self.import_btn = QPushButton("Import Params")
        self.import_btn.setObjectName("fileBtn")
        self.import_btn.clicked.connect(self._on_import)
        export_layout.addWidget(self.import_btn)

        layout.addWidget(export_group)

        layout.addWidget(info_group)

        # ── Status ──
        self.status_label = QLabel("")
        self.status_label.setAlignment(Qt.AlignCenter)
        self.status_label.setStyleSheet(
            "color: #cccccc; font-size: 11px; "
            "background: transparent; padding: 4px;"
        )
        layout.addWidget(self.status_label)

        layout.addStretch()

    # ── File info getters / setters ───────────────────────────────

    def get_header_id(self) -> str:
        text = self.header_edit.text().strip()
        return text if text else "AUDIO_CFG_V1"

    def get_version_parts(self):
        return self.spin_major.value(), self.spin_minor.value(), self.spin_fix.value()

    def set_header_id(self, s: str):
        self.header_edit.setText(s[:39])

    def set_version_parts(self, major: int, minor: int, fix: int):
        self.spin_major.blockSignals(True)
        self.spin_minor.blockSignals(True)
        self.spin_fix.blockSignals(True)
        self.spin_major.setValue(max(0, min(99, major)))
        self.spin_minor.setValue(max(0, min(99, minor)))
        self.spin_fix.setValue(max(0, min(99, fix)))
        self.spin_major.blockSignals(False)
        self.spin_minor.blockSignals(False)
        self.spin_fix.blockSignals(False)
        self._refresh_version_preview()

    def set_version_preview(self, text: str):
        self.version_preview.setText(text)
        self.version_preview.setStyleSheet(
            "color:#4FC3F7; font-size:10px; font-family:monospace; padding:2px 0;"
        )

    def _refresh_version_preview(self):
        ma, mi, fx = self.get_version_parts()
        self.version_preview.setText(
            f"V{ma:02d}.{mi:02d}.{fx:02d}  ──.──.────  ──:──:──"
        )
        self.version_preview.setStyleSheet(
            "color:#446688; font-size:10px; font-family:monospace; padding:2px 0;"
        )

    def _on_upload(self):
        """Handle upload button click."""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select Parameter File",
            "",
            "All Files (*.bin *.dat *.json *.csv);;Binary Files (*.bin *.dat)"
        )
        if file_path:
            self.set_status(f"Selected: {file_path}")
            self.upload_file_clicked.emit(file_path)

    def _on_download(self):
        """Handle download button click."""
        self.download_requested.emit()

    def _on_export(self):
        """Handle export button click."""
        self.export_clicked.emit("")

    def _on_import(self):
        """Handle import button click."""
        self.import_clicked.emit("")

    def set_status(self, message: str, success: bool = True):
        """Set file operation status message.
        
        Args:
            message: Status message to display
            success: True for green (success), False for red (error)
        """
        color = "#4CAF50" if success else "#e94560"
        self.status_label.setText(message)
        self.status_label.setStyleSheet(
            f"color: {color}; font-size: 12px; "
            "background: transparent; padding: 6px; font-weight: bold;"
        )
