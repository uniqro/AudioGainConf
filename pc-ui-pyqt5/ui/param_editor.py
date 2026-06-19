"""
Parameter Editor — tab-based mode selector, toggle switches, full-width Total Gains.
"""

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QComboBox,
    QPushButton, QLabel, QGroupBox, QSlider,
    QFormLayout, QSpinBox, QScrollArea, QTabBar,
)
from PyQt5.QtCore import pyqtSignal, Qt

from audio_protocol import (
    MODE_SETS, NUM_MODES, NUM_PARAMS, PARAM_DEFS, ParamDef,
)


# ─── Toggle Switch ────────────────────────────────────────────────────────────

class ToggleSwitch(QPushButton):
    """Pill-shaped PASSIVE / ACTIVE toggle replacing QCheckBox."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setCheckable(True)
        self.setChecked(False)
        self.setObjectName("toggleSwitch")
        self._refresh()
        self.toggled.connect(lambda _: self._refresh())

    def _refresh(self):
        self.setText("■  ACTIVE" if self.isChecked() else "□  PASSIVE")


# ─── ParamEditor ─────────────────────────────────────────────────────────────

class ParamEditor(QWidget):
    mode_changed   = pyqtSignal(int)
    param_changed  = pyqtSignal(int, int, int)   # mode, param_idx, raw_value
    read_requested = pyqtSignal()
    apply_requested = pyqtSignal()

    # Shared group-box style (cyan header, consistent border)
    _GRP = """
        QGroupBox {
            border: 1px solid #1e3a5c;
            border-radius: 5px;
            margin-top: 6px;
            padding-top: 10px;
            font-weight: bold;
            font-size: 11px;
            color: #4FC3F7;
        }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }
    """

    def __init__(self, parent=None):
        super().__init__(parent)
        self._current_mode = 0
        self._param_controls: dict = {}
        self._param_labels:   dict = {}
        self._mode_params = [[0] * NUM_PARAMS for _ in range(NUM_MODES)]
        self._setup_ui()
        self._load_mode_ui(0)
        self._update_total_gains()

    # ── Build UI ──────────────────────────────────────────────────────────────

    def _setup_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(6, 6, 6, 6)
        root.setSpacing(6)

        # ── Mode tab bar ───────────────────────────────────────────
        tab_row = QHBoxLayout()
        tab_row.setSpacing(0)
        tab_row.setContentsMargins(0, 0, 0, 0)

        self.mode_tabs = QTabBar()
        self.mode_tabs.setObjectName("modeTabs")
        self.mode_tabs.setExpanding(False)
        for m in MODE_SETS:
            self.mode_tabs.addTab(f"  {m}  ")
        self.mode_tabs.currentChanged.connect(self._on_mode_changed)

        tab_row.addWidget(self.mode_tabs)
        tab_row.addStretch()
        root.addLayout(tab_row)

        # ── Scrollable parameter area ──────────────────────────────
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)

        content = QWidget()
        cl = QVBoxLayout(content)
        cl.setContentsMargins(0, 2, 4, 0)
        cl.setSpacing(8)

        # Two columns
        cols = QHBoxLayout()
        cols.setSpacing(10)

        left = QVBoxLayout()
        left.setSpacing(8)
        self._build_microphone_block(left)
        self._build_sidetone_block(left)
        self._build_speaker_block(left)
        left.addStretch()

        right = QVBoxLayout()
        right.setSpacing(8)
        self._build_soft_agc_block(right)
        self._build_hard_agc_block(right)
        right.addStretch()

        cols.addLayout(left, 1)
        cols.addLayout(right, 1)
        cl.addLayout(cols)

        # Full-width Total Gains at bottom
        self._build_total_gains_section(cl)
        cl.addStretch()

        scroll.setWidget(content)
        root.addWidget(scroll, 1)

        # ── Action buttons (full width, above status bar) ──────────
        btn_row = QHBoxLayout()
        btn_row.setSpacing(6)
        btn_row.setContentsMargins(0, 0, 0, 0)

        self.read_btn = QPushButton("▼   READ FROM RADIO")
        self.read_btn.setObjectName("actionBtn")
        self.read_btn.setMinimumHeight(32)
        self.read_btn.clicked.connect(self._on_read)

        self.apply_btn = QPushButton("▲   APPLY TO RADIO")
        self.apply_btn.setObjectName("applyBtn")
        self.apply_btn.setMinimumHeight(32)
        self.apply_btn.clicked.connect(self._on_apply)

        btn_row.addWidget(self.read_btn, 1)
        btn_row.addWidget(self.apply_btn, 1)
        root.addLayout(btn_row)

        # ── Status label ───────────────────────────────────────────
        self.status_label = QLabel("Ready")
        self.status_label.setStyleSheet(
            "color: #446688; font-size: 11px; background: transparent;"
        )
        root.addWidget(self.status_label)

    # ── Section builders ──────────────────────────────────────────────────────

    def _make_group(self, title: str):
        g = QGroupBox(title)
        g.setStyleSheet(self._GRP)
        f = QFormLayout()
        f.setSpacing(7)
        f.setLabelAlignment(Qt.AlignRight)
        f.setFormAlignment(Qt.AlignLeft | Qt.AlignTop)
        f.setContentsMargins(10, 2, 10, 10)
        return g, f

    def _add_params(self, form, pairs):
        for idx, label in pairs:
            ctrl = self._create_param_control(PARAM_DEFS[idx], idx)
            form.addRow(label, ctrl["widget"])
            self._param_controls[idx] = ctrl
            self._param_labels[idx] = ctrl.get("label")

    def _build_microphone_block(self, parent):
        g, f = self._make_group("MICROPHONE")
        self._add_params(f, [(0, "Input Level"), (1, "ADC PGA")])
        g.setLayout(f)
        parent.addWidget(g)

    def _build_sidetone_block(self, parent):
        g, f = self._make_group("SIDETONE")
        self._add_params(f, [(2, "Attenuation")])
        g.setLayout(f)
        parent.addWidget(g)

    def _build_speaker_block(self, parent):
        g, f = self._make_group("SPEAKER")
        self._add_params(f, [(3, "Output"), (4, "Attenuation")])
        g.setLayout(f)
        parent.addWidget(g)

    def _build_soft_agc_block(self, parent):
        g, f = self._make_group("SOFT AGC")
        self._add_params(f, [(5, "Active"), (6, "Target"), (7, "Max Gain")])
        g.setLayout(f)
        parent.addWidget(g)

    def _build_hard_agc_block(self, parent):
        g, f = self._make_group("HARD AGC")
        self._add_params(f, [
            (8,  "Active"),
            (9,  "Target Gain"),
            (10, "Attack"),
            (11, "Decay"),
            (12, "Max Gain"),
            (13, "Noise Hyst."),
            (14, "Noise Thr."),
        ])
        g.setLayout(f)
        parent.addWidget(g)

    def _build_total_gains_section(self, parent_layout):
        g = QGroupBox("TOTAL GAINS")
        g.setStyleSheet("""
            QGroupBox {
                border: 1px solid #1e5c2a;
                border-radius: 5px;
                margin-top: 6px;
                padding-top: 10px;
                font-weight: bold;
                font-size: 11px;
                color: #4CAF50;
            }
            QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }
        """)
        row = QHBoxLayout()
        row.setContentsMargins(14, 2, 14, 10)
        row.setSpacing(50)

        self._lbl_mic      = QLabel("Mic Gain:  +0.0 dB")
        self._lbl_speaker  = QLabel("Speaker:  +0.0 dB")
        self._lbl_sidetone = QLabel("Sidetone:  +0.0 dB")

        style = "color: #80c880; font-size: 12px; font-weight: bold;"
        for lbl in (self._lbl_mic, self._lbl_speaker, self._lbl_sidetone):
            lbl.setStyleSheet(style)
            row.addWidget(lbl)

        row.addStretch()
        g.setLayout(row)
        parent_layout.addWidget(g)

    # ── Total Gains ───────────────────────────────────────────────────────────

    def _update_total_gains(self):
        def db(idx):
            return PARAM_DEFS[idx].display_value(self.get_param_value(idx))

        mic      = db(0) + db(1)
        speaker  = db(3) - db(4)
        sidetone = db(0) + db(1) - db(2) + db(3)

        self._lbl_mic.setText(     f"Mic Gain:  {mic:+.1f} dB")
        self._lbl_speaker.setText( f"Speaker:  {speaker:+.1f} dB")
        self._lbl_sidetone.setText(f"Sidetone:  {sidetone:+.1f} dB")

    # ── Control factory ───────────────────────────────────────────────────────

    def _create_param_control(self, param_def: ParamDef, index: int) -> dict:
        name = param_def.name

        # Toggle switch for AGC active flags
        if name in ("SOFT_AGC_ACTIVE", "HARD_AGC_ACTIVE"):
            t = ToggleSwitch()
            t.toggled.connect(
                lambda state, idx=index: self._on_param_changed(idx, int(state), PARAM_DEFS[idx])
            )
            return {"widget": t, "label": None}

        # Combo-box parameters
        if name == "INPUT_LEVEL_CONTROL_GAIN":
            return self._make_combo(param_def, index,
                [(param_def.display_string(v), v) for v in range(9)])

        if name == "HARD_AGC_TARGET_GAIN":
            return self._make_combo(param_def, index,
                [(param_def.display_string(v), v) for v in range(8)])

        if name == "HARD_AGC_ATTACK_TIME":
            return self._make_combo(param_def, index,
                [(param_def.display_string(v), v) for v in range(4)])

        if name == "HARD_AGC_DECAY_TIME":
            return self._make_combo(param_def, index,
                [(param_def.display_string(v), v) for v in range(4)])

        if name == "HARD_AGC_NOISE_HYSTERESIS_LEVEL":
            return self._make_combo(param_def, index,
                [("Disabled", 0), ("1 dB", 1), ("2 dB", 2), ("3 dB", 3)])

        if name == "HARD_AGC_NOISE_THRESHOLD":
            items = [("Disabled", 0)]
            for v in range(1, 32):
                items.append((f"{-30 - (v-1)*2} dB", v))
            return self._make_combo(param_def, index, items)

        # 0.5 dB-step sliders
        if param_def.display_scale == 0.5:
            return self._make_half_db_slider(param_def, index)

        # Integer sliders
        return self._make_slider(param_def, index)

    # ── Widget helpers ────────────────────────────────────────────────────────

    def _make_combo(self, param_def, index, items):
        cb = QComboBox()
        cb.setMinimumWidth(105)
        for text, raw in items:
            cb.addItem(text, raw)
        cb.currentIndexChanged.connect(
            lambda _, b=cb, idx=index: self._on_param_changed(
                idx,
                b.currentData() if b.currentData() is not None else 0,
                PARAM_DEFS[idx],
            )
        )
        return {"widget": cb, "label": None, "_combo": cb}

    def _make_slider(self, param_def, index):
        lo, hi = param_def.min_raw, param_def.max_raw
        sl = QSlider(Qt.Horizontal)
        sl.setRange(lo, hi)
        sl.setValue(lo)
        sl.setMinimumHeight(20)

        lbl = QLabel(param_def.display_string(lo))
        lbl.setStyleSheet("color: #4FC3F7; font-size: 11px; min-width: 68px;")
        bm, bp = self._pm_buttons()

        idx = index

        def on_v(v):
            lbl.setText(param_def.display_string(v))
            self._on_param_changed(idx, v, PARAM_DEFS[idx])

        sl.valueChanged.connect(on_v)
        bm.clicked.connect(lambda: sl.setValue(max(lo, sl.value() - 1)))
        bp.clicked.connect(lambda: sl.setValue(min(hi, sl.value() + 1)))

        return {"widget": self._slider_row(sl, lbl, bm, bp), "label": lbl, "_slider": sl}

    def _make_half_db_slider(self, param_def, index):
        hi = param_def.max_raw
        sl = QSlider(Qt.Horizontal)
        sl.setRange(0, hi)
        sl.setValue(0)
        sl.setMinimumHeight(20)

        lbl = QLabel(param_def.display_string(0))
        lbl.setStyleSheet("color: #4FC3F7; font-size: 11px; min-width: 68px;")
        bm, bp = self._pm_buttons()

        idx = index

        def on_v(v):
            lbl.setText(param_def.display_string(v))
            self._on_param_changed(idx, v, PARAM_DEFS[idx])

        sl.valueChanged.connect(on_v)
        bm.clicked.connect(lambda: sl.setValue(max(0,  sl.value() - 2)))
        bp.clicked.connect(lambda: sl.setValue(min(hi, sl.value() + 2)))

        return {"widget": self._slider_row(sl, lbl, bm, bp), "label": lbl, "_slider": sl}

    def _pm_buttons(self):
        s = ("QPushButton { background:#162033; border:1px solid #2e5f8a; "
             "color:#7090b0; font-weight:bold; font-size:15px; padding:0; border-radius:3px; }"
             "QPushButton:hover { background:#4FC3F7; color:#000; border-color:#4FC3F7; }")
        bm, bp = QPushButton("−"), QPushButton("+")
        bm.setFixedSize(24, 24)
        bp.setFixedSize(24, 24)
        bm.setStyleSheet(s)
        bp.setStyleSheet(s)
        return bm, bp

    def _slider_row(self, sl, lbl, bm, bp):
        outer = QWidget()
        row = QHBoxLayout(outer)
        row.setContentsMargins(0, 2, 0, 2)
        row.setSpacing(6)
        row.addWidget(sl, 1)
        row.addWidget(lbl)
        row.addWidget(bm)
        row.addWidget(bp)
        return outer

    # ── Mode / param persistence ──────────────────────────────────────────────

    def _save_mode_params(self, mode_idx: int):
        for i in range(NUM_PARAMS):
            self._mode_params[mode_idx][i] = self.get_param_value(i)

    def _load_mode_ui(self, mode_idx: int):
        for i in range(NUM_PARAMS):
            self.set_param_value(i, self._mode_params[mode_idx][i])

    # ── Public getters / setters ──────────────────────────────────────────────

    def get_param_value(self, param_index: int) -> int:
        if param_index >= len(PARAM_DEFS):
            return 0
        ctrl = self._param_controls.get(param_index)
        if ctrl is None:
            return 0
        w = ctrl["widget"]
        if isinstance(w, ToggleSwitch):
            return 1 if w.isChecked() else 0
        if isinstance(w, QComboBox):
            d = w.currentData()
            return d if d is not None else 0
        if isinstance(w, QSpinBox):
            return w.value()
        if isinstance(w, QWidget) and "_slider" in ctrl:
            return ctrl["_slider"].value()
        return 0

    def set_param_value(self, param_index: int, raw_value: int):
        if param_index >= len(PARAM_DEFS):
            return
        ctrl = self._param_controls.get(param_index)
        if ctrl is None:
            return
        w = ctrl["widget"]
        p = PARAM_DEFS[param_index]

        if isinstance(w, ToggleSwitch):
            w.blockSignals(True)
            w.setChecked(raw_value == 1)
            w.blockSignals(False)
        elif isinstance(w, QComboBox):
            for i in range(w.count()):
                if w.itemData(i) == raw_value:
                    w.setCurrentIndex(i)
                    break
        elif isinstance(w, QSpinBox):
            w.setValue(raw_value)
        elif isinstance(w, QWidget) and "_slider" in ctrl:
            s = ctrl["_slider"]
            s.blockSignals(True)
            s.setValue(raw_value)
            s.blockSignals(False)
            lbl = ctrl.get("label")
            if lbl:
                lbl.setText(p.display_string(raw_value))

        self._mode_params[self._current_mode][param_index] = raw_value
        self._update_total_gains()

    def set_all_params(self, raw_values: list):
        for i in range(min(len(raw_values), NUM_PARAMS)):
            self.set_param_value(i, raw_values[i])

    def get_all_params(self) -> list:
        return [self.get_param_value(i) for i in range(NUM_PARAMS)]

    def get_current_mode_index(self) -> int:
        return self._current_mode

    def get_all_mode_params(self) -> list:
        """Return all 6×15 raw values (always current, including active mode)."""
        self._save_mode_params(self._current_mode)
        return [list(m) for m in self._mode_params]

    def set_all_mode_params(self, modes: list):
        """Load all 6×15 raw values and refresh the visible mode."""
        for i in range(min(len(modes), NUM_MODES)):
            self._mode_params[i] = list(modes[i])[:NUM_PARAMS]
        self._load_mode_ui(self._current_mode)
        self._update_total_gains()

    def set_status(self, message: str):
        self.status_label.setText(message)

    # ── Event handlers ────────────────────────────────────────────────────────

    def _on_param_changed(self, param_index: int, raw_value: int, param_def: ParamDef):
        self._mode_params[self._current_mode][param_index] = raw_value
        self.param_changed.emit(self._current_mode, param_index, raw_value)
        self._update_total_gains()

    def _on_mode_changed(self, index: int):
        self._save_mode_params(self._current_mode)
        self._current_mode = index
        self.mode_changed.emit(index)
        self._load_mode_ui(index)
        self.status_label.setText(f"Mode: {MODE_SETS[index]}")

    def _on_read(self):
        self.read_requested.emit()

    def _on_apply(self):
        self.apply_requested.emit()
        self.status_label.setText(
            f"Applying {NUM_PARAMS} params → {MODE_SETS[self._current_mode]}"
        )