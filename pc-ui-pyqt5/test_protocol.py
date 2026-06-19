#!/usr/bin/env python
"""Test script for pc-ui-pyqt5 - basic imports and protocol."""
import sys

# Test protocol only (no GUI needed)
from audio_protocol import (
    NUM_PARAMS, NUM_MODES, PARAM_DEFS,
    build_full_upload, parse_full_download,
    build_status_request, build_set_param_request,
    build_set_gain_block_request, build_verify_request,
    build_upload_request,
    parse_status_response, parse_all_modes_response,
)

print('Protocol imports OK', flush=True)

# Verify 15 params
assert len(PARAM_DEFS) == 15, 'Expected 15 params, got %d' % len(PARAM_DEFS)
print('PARAM_DEFS count: 15', flush=True)
print('NUM_PARAMS:', NUM_PARAMS, flush=True)
print('NUM_MODES:', NUM_MODES, flush=True)

# Test protocol build functions
status = build_status_request()
assert status == bytes([0x01]), 'STATUS request failed'
print('build_status_request OK', flush=True)

set_param = build_set_param_request(0, 3, 60)
assert set_param == bytes([0x02, 0x00, 0x03, 0x3C]), 'SET_PARAM failed'
print('build_set_param_request OK', flush=True)

set_block = build_set_gain_block_request(0, list(range(15)))
assert len(set_block) == 3 + 15, 'SET_GAIN_BLOCK size wrong'
print('build_set_gain_block_request OK', flush=True)

verify = build_verify_request(1)
assert verify == bytes([0x04, 0x01]), 'VERIFY failed'
print('build_verify_request OK', flush=True)

# Test full upload/download round-trip
test_modes = [[i % 256 for i in range(15)] for _ in range(6)]
pkt = build_full_upload(test_modes)
assert len(pkt) == 93, 'Upload packet size wrong: %d' % len(pkt)
parsed, version = parse_full_download(pkt)
assert parsed == test_modes, 'Parsed modes mismatch'
assert version == 1, 'Version mismatch'
print('build_full_upload / parse_full_download OK', flush=True)

# Test status response parsing
status_data = bytes([0x01, 0x00, 0x0F] + list(test_modes[2]))
params = parse_status_response(status_data, 0)
assert params == test_modes[2], 'Status parse mismatch'
print('parse_status_response OK', flush=True)

# Test upload request
mode_data = bytes([i % 256 for i in range(90)])
upload = build_upload_request(mode_data)
assert len(upload) == 2 + 90 + 1, 'Upload request size wrong'
print('build_upload_request OK', flush=True)

# Test param display strings
gains = PARAM_DEFS[0]
for v in range(9):
    s = gains.display_string(v)
    if v == 0:
        assert s == '0.0 dB', 'Input gain 0 wrong: %s' % s
    else:
        assert s == '-%.1f dB' % (v * 1.5), 'Input gain %d wrong: %s' % (v, s)
print('Input gain display strings OK', flush=True)

agc_gain = PARAM_DEFS[9]
for v in range(8):
    s = agc_gain.display_string(v)
assert s == '-24', 'HARD_AGC_TARGET_GAIN wrong: %s' % s
print('HARD_AGC_TARGET_GAIN display strings OK', flush=True)

target_map = PARAM_DEFS[9]
assert target_map.display_string(0) == '-5.5', 'Failed'
assert target_map.display_string(7) == '-24', 'Failed'
print('Hard AGC Target Gain map OK', flush=True)

# Test active/passive params
active = PARAM_DEFS[5]
assert active.display_string(0) == 'PASSIVE', 'PASSIVE wrong'
assert active.display_string(1) == 'ACTIVE', 'ACTIVE wrong'
print('Active/Passive params OK', flush=True)

# Noise hysteresis
nh = PARAM_DEFS[13]
assert nh.display_string(0) == 'Disabled', 'Hysteresis 0 wrong'
assert nh.display_string(3) == '3-dB', 'Hysteresis 3 wrong'
print('Noise hysteresis OK', flush=True)

# Noise threshold
nt = PARAM_DEFS[14]
assert nt.display_string(0) == 'Disabled', 'Threshold 0 wrong'
assert nt.display_string(1) == '-30-dB', 'Threshold 1 wrong: %s' % nt.display_string(1)
assert nt.display_string(6) == '-35-dB', 'Threshold 6 wrong: %s' % nt.display_string(6)
print('Noise threshold OK', flush=True)

print(flush=True)
print('*** ALL PROTOCOL TESTS PASSED ***', flush=True)
