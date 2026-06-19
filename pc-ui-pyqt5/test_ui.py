#!/usr/bin/env python
"""Test script for pc-ui-pyqt5 integration."""
import sys
sys.argv = ['test', '-platform', 'offscreen']
from PyQt5.QtWidgets import QApplication

app = QApplication(sys.argv)

from audio_protocol import NUM_PARAMS, PARAM_DEFS
from tcp_client import TCPClient
from ui.connection_panel import ConnectionPanel
from ui.param_editor import ParamEditor
from ui.file_ops import FileOps

# Create widgets
cp = ConnectionPanel()
pe = ParamEditor()
fo = FileOps()

print('Widgets created OK', flush=True)
print('NUM_PARAMS:', NUM_PARAMS)
print('PARAM_DEFS:', len(PARAM_DEFS))
print('Param controls:', len(pe._param_controls))

# Test get/set roundtrip
test_values = [4, 30, 15, 5, 40, 1, 50, 15, 1, 3, 2, 1, 50, 1, 10]
pe.set_all_params(test_values)
result = pe.get_all_params()
if result == test_values:
    print('Get/set roundtrip OK')
else:
    print('Roundtrip mismatch: %s != %s' % (result, test_values))
    sys.exit(1)

# Test protocol
from audio_protocol import build_full_upload, parse_full_download
modes = [[i % 256 for i in range(15)] for _ in range(6)]
pkt = build_full_upload(modes)
parsed, version = parse_full_download(pkt)
if parsed == modes and version == 1:
    print('Protocol roundtrip OK')
else:
    print('Protocol mismatch')
    sys.exit(1)

app.quit()
print('ALL TESTS PASSED')
