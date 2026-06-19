"""
Audio DSP Configuration Protocol
- 4 command types: STATUS (0x01), SET_PARAM (0x02), SET_GAIN_BLOCK (0x03), VERIFY (0x04)
- 15 parameters across 6 mode sets (AM, FM, HQII, 9600, 9600A, SK2)
- Each mode set = 300 bytes of parameter data
"""

import struct
from enum import IntEnum
from dataclasses import dataclass, field
from typing import List, Tuple


# ─── Command Types ───────────────────────────────────────────────
CMD_STATUS: int = 0x01
CMD_SET_PARAM: int = 0x02
CMD_SET_GAIN_BLOCK: int = 0x03
CMD_VERIFY: int = 0x04

CMD_NAMES = {
    CMD_STATUS: "STATUS",
    CMD_SET_PARAM: "SET_PARAM",
    CMD_SET_GAIN_BLOCK: "SET_GAIN_BLOCK",
    CMD_VERIFY: "VERIFY",
}

# ─── Mode Sets ──────────────────────────────────────────────────
MODE_SETS = ["AM", "FM", "HQII", "9600", "9600A", "SK2"]
NUM_MODES = len(MODE_SETS)

# ─── Parameter Definitions ──────────────────────────────────────
# Each param: (name, min_raw, max_raw, min_display, max_display, display_scale, format)
# display_scale: multiplier to convert raw value to display value (e.g. 0.5 for 0.5dB steps)

@dataclass
class ParamDef:
    name: str
    label: str
    group: str          # INPUT_SECTION, OUTPUT_SECTION, SOFT_AGC, HARD_AGC
    min_raw: int
    max_raw: int
    display_scale: float  # raw * scale = display value (e.g. 0.5 for 0.5dB steps)
    display_unit: str
    format: str           # struct format for raw storage

    def display_value(self, raw: int) -> float:
        """Convert raw value to human-readable display value."""
        return raw * self.display_scale

    def display_string(self, raw: int) -> str:
        """Convert raw value to display string with proper formatting."""
        val = self.display_value(raw)
        if self.name == "INPUT_LEVEL_CONTROL_GAIN":
            # 9 levels: 0, -1.5, -3.0, -4.5, -6.0, -7.5, -9.0, -10.5, -12.0 dB
            if raw == 0:
                return "0.0 dB"
            return "-%.1f dB" % (raw * 1.5)
        if self.name == "HARD_AGC_TARGET_GAIN":
            target_map = {
                0x00: "-5.5 dB", 0x01: "-8 dB", 0x02: "-10 dB", 0x03: "-12 dB",
                0x04: "-14 dB", 0x05: "-17 dB", 0x06: "-20 dB", 0x07: "-24 dB"
            }
            return target_map.get(raw, f"{raw} dB")
        elif self.name == "HARD_AGC_ATTACK_TIME":
            attack_map = {0x00: "8", 0x01: "11", 0x02: "16", 0x03: "20"}
            return f"{attack_map.get(raw, raw)}-msec"
        elif self.name == "HARD_AGC_DECAY_TIME":
            decay_map = {0x00: "100", 0x01: "200", 0x02: "400", 0x03: "500"}
            return f"{decay_map.get(raw, raw)}-msec"
        elif self.name == "HARD_AGC_NOISE_HYSTERESIS_LEVEL":
            if raw == 0:
                return "Disabled"
            hysteresis_map = {0x01: "1", 0x02: "2", 0x03: "3"}
            return f"{hysteresis_map.get(raw, raw)}-dB"
        elif self.name == "HARD_AGC_NOISE_THRESHOLD":
            if raw == 0:
                return "Disabled"
            # Threshold: -30 dB to -90 dB, each step = 2 dB (matches C enum)
            # raw=1 → -30 dB, raw=2 → -32 dB, ..., raw=31 → -90 dB
            db_val = -30 - (raw - 1) * 2
            return f"{db_val} dB"
        elif self.name == "SOFT_AGC_MAX_GAIN":
            return f"{val:.0f} dB"
        elif self.name in ("SIDETONE_ATTENUATION_GAIN", "SPEAKER_ATTENUATION_GAIN"):
            # Attenuation values show as negative
            if val == 0.0:
                return "0.0 dB"
            return f"-{val:.1f} dB"
        elif self.name.startswith("SOFT_AGC_TARGET_MOD_INDEX"):
            return f"{raw}%"
        elif self.name == "SOFT_AGC_ACTIVE" or self.name == "HARD_AGC_ACTIVE":
            active_map = {0: "PASSIVE", 1: "ACTIVE"}
            return active_map.get(raw, str(raw))
        else:
            if val < 0:
                return f"{val:.1f} dB"
            else:
                return f"{val:.1f} dB"

    def raw_from_display(self, display_text: str) -> int:
        """Convert display text back to raw value."""
        if self.display_scale == 0.5:
            try:
                return int(float(display_text.strip(" dB")) / self.display_scale)
            except ValueError:
                return self.min_raw
        elif self.name == "HARD_AGC_TARGET_GAIN":
            gain_map = {
                "-5.5 dB": 0x00, "-8 dB": 0x01, "-10 dB": 0x02, "-12 dB": 0x03,
                "-14 dB": 0x04, "-17 dB": 0x05, "-20 dB": 0x06, "-24 dB": 0x07
            }
            return gain_map.get(display_text, 0)
        elif self.name == "HARD_AGC_ATTACK_TIME":
            attack_map = {"8": 0x00, "11": 0x01, "16": 0x02, "20": 0x03}
            return attack_map.get(display_text.strip("-msec"), 0)
        elif self.name == "HARD_AGC_DECAY_TIME":
            decay_map = {"100": 0x00, "200": 0x01, "400": 0x02, "500": 0x03}
            return decay_map.get(display_text.strip("-msec"), 0)
        elif self.name == "HARD_AGC_NOISE_HYSTERESIS_LEVEL":
            if "Disabled" in display_text:
                return 0
            hysteresis_map = {"1": 0x01, "2": 0x02, "3": 0x03}
            return hysteresis_map.get(display_text.strip("-dB"), 0)
        elif self.name == "HARD_AGC_NOISE_THRESHOLD":
            if "Disabled" in display_text:
                return 0
            try:
                db_val = int(display_text.strip(" dB"))
                return 1 + (-30 - db_val) // 2
            except ValueError:
                return 0
        elif self.name == "SOFT_AGC_ACTIVE" or self.name == "HARD_AGC_ACTIVE":
            active_map = {"PASSIVE": 0, "ACTIVE": 1}
            return active_map.get(display_text, 0)
        elif self.name == "SOFT_AGC_TARGET_MOD_INDEX":
            try:
                return int(display_text.strip("%"))
            except ValueError:
                return 0
        elif self.name in ("SIDETONE_ATTENUATION_GAIN", "SPEAKER_ATTENUATION_GAIN"):
            try:
                # Handle negative display like "-15.0 dB" -> raw value
                abs_val = float(display_text.strip("- dB").strip())
                return int(abs_val / self.display_scale)
            except ValueError:
                return self.min_raw
        elif self.name == "SOFT_AGC_MAX_GAIN":
            try:
                return int(display_text.strip(" dB").strip())
            except ValueError:
                return self.min_raw
        else:
            return self.min_raw


PARAM_DEFS: List[ParamDef] = [
    # 0 - INPUT SECTION
    ParamDef("INPUT_LEVEL_CONTROL_GAIN", "Input Level Control", "INPUT_SECTION",
             0, 8, -1.5, "", "B"),        # 9 levels (0-8): 0=-0dB, 1=-1.5dB, ..., 8=-12dB

    # 1 - INPUT SECTION
    ParamDef("ADC_PGA_GAIN", "ADC PGA Gain", "INPUT_SECTION",
             0, 119, 0.5, " dB", "B"),    # 0 to 59.5 dB step 0.5

    # 2 - INPUT SECTION
    ParamDef("SIDETONE_ATTENUATION_GAIN", "Sidetone Attenuation", "INPUT_SECTION",
             0, 120, 0.5, " dB", "B"),    # 0 to 60 dB step 0.5 (negative)

    # 3 - OUTPUT SECTION
    ParamDef("OUTPUT_LEVEL_CONTROL_GAIN", "Output Level Control", "OUTPUT_SECTION",
             0, 9, 1.0, " dB", "B"),       # 0-9 dB

    # 4 - OUTPUT SECTION
    ParamDef("SPEAKER_ATTENUATION_GAIN", "Speaker Attenuation", "OUTPUT_SECTION",
             0, 127, 0.5, " dB", "B"),    # 0 to 63.5 dB step 0.5 (negative)

    # 5 - SOFT AGC
    ParamDef("SOFT_AGC_ACTIVE", "Soft AGC Active", "SOFT_AGC",
             0, 1, 1.0, "", "B"),          # 0=PASSIVE, 1=ACTIVE

    # 6 - SOFT AGC
    ParamDef("SOFT_AGC_TARGET_MOD_INDEX", "Soft AGC Target Mod Index", "SOFT_AGC",
             0, 100, 1.0, "%", "B"),      # 0-100% step 1

    # 7 - SOFT AGC
    ParamDef("SOFT_AGC_MAX_GAIN", "Soft AGC Max Gain", "SOFT_AGC",
             0, 30, 1.0, " dB", "B"),     # 0-30 dB step 1

    # 8 - HARD AGC
    ParamDef("HARD_AGC_ACTIVE", "Hard AGC Active", "HARD_AGC",
             0, 1, 1.0, "", "B"),          # 0=PASSIVE, 1=ACTIVE

    # 9 - HARD AGC
    ParamDef("HARD_AGC_TARGET_GAIN", "Hard AGC Target Gain", "HARD_AGC",
             0, 7, 1.0, " dB", "B"),      # 3-bit: -5.5, -8, -10, -12, -14, -17, -20, -24

    # 10 - HARD AGC
    ParamDef("HARD_AGC_ATTACK_TIME", "Hard AGC Attack Time", "HARD_AGC",
             0, 3, 1.0, "-msec", "B"),   # 2-bit: 8, 11, 16, 20 msec

    # 11 - HARD AGC
    ParamDef("HARD_AGC_DECAY_TIME", "Hard AGC Decay Time", "HARD_AGC",
             0, 3, 1.0, "-msec", "B"),   # 2-bit: 100, 200, 400, 500 msec

    # 12 - HARD AGC
    ParamDef("HARD_AGC_MAX_GAIN", "Hard AGC Max Gain", "HARD_AGC",
             0, 119, 0.5, " dB", "B"),   # 0-59.5 dB step 0.5

    # 13 - HARD AGC
    ParamDef("HARD_AGC_NOISE_HYSTERESIS_LEVEL", "Hard AGC Noise Hysteresis", "HARD_AGC",
             0, 3, 1.0, " dB", "B"),     # 2-bit: disabled, 1, 2, 3 dB

    # 14 - HARD AGC
    ParamDef("HARD_AGC_NOISE_THRESHOLD", "Hard AGC Noise Threshold", "HARD_AGC",
             0, 0x1F, 1.0, " dB", "B"), # 5-bit: disabled or -30...-90 dB (stored as 1 byte)
]

# Corrected param list: only the 15 actual parameters + 1 reserved = 16 bytes per mode set
# Mode struct: 16 bytes × 6 modes = 96 bytes body
# But task says 300 bytes per mode... Let me keep it flexible

NUM_PARAMS = 15  # Actual user-configurable params only

# ─── Mode Configuration Structure ────────────────────────────────
# Per-mode: 15 params × 1 byte each = 15 bytes
# Plus optional calibration/version/padding fields

BYTES_PER_MODE = 15 * 1  # 15 params, each 1 byte

# Total packet body for 6 modes: 6 × 15 = 90 bytes
# Packet frame: header + cmd + mode_data

HEADER_SIZE = 4  # Magic + version
MODE_SET_OFFSET = HEADER_SIZE  # Start of mode data in packet


def get_param_group_names() -> List[str]:
    """Return unique group names (sections)."""
    seen = set()
    groups = []
    for p in PARAM_DEFS:
        if p.group not in seen and p.group != "HARD_AGC" or NUM_PARAMS <= 14:
            pass
        if p.group not in seen:
            seen.add(p.group)
            groups.append(p.group)
    return groups


# ─── Build/Parse Functions ──────────────────────────────────────

def build_status_request() -> bytes:
    """Build STATUS command request (0x01)."""
    return struct.pack("<B", CMD_STATUS)


def build_set_param_request(mode_index: int, param_index: int, value: int) -> bytes:
    """Build SET_PARAM command (0x02).
    
    Args:
        mode_index: Mode set index (0-5 = AM, FM, HQII, 9600, 9600A, SK2)
        param_index: Parameter index (0-14)
        value: Raw value to set
    """
    header = struct.pack("<BBB", 0x02, mode_index, param_index)
    body = struct.pack("B", value & 0xFF)
    return header + body


def build_set_gain_block_request(mode_index: int, values: List[int]) -> bytes:
    """Build SET_GAIN_BLOCK command (0x03) to send all param values for a mode.
    
    Args:
        mode_index: Mode set index (0-5)
        values: List of 15 raw values
    """
    header = struct.pack("<BB", 0x03, mode_index)
    body = bytes(values[:15])  # Ensure exactly 15 bytes
    return header + body


def build_verify_request(mode_index: int = 0) -> bytes:
    """Build VERIFY command (0x04).

    Embedded side expects no payload — verifies all stored config.
    mode_index parameter kept for call-site compatibility but not sent.
    """
    return struct.pack("<B", CMD_VERIFY)


def build_read_all_request(mode_index: int) -> bytes:
    """Build request to read all parameters for a mode set."""
    return struct.pack("<B", CMD_STATUS)


def build_upload_request(mode_data: bytes) -> bytes:
    """Build upload request with mode parameter data.
    
    Args:
        mode_data: Raw mode data bytes (15 bytes × N modes)
    """
    # Format: CMD (1B) + mode_count (1B) + mode_data (N × 15 bytes)
    mode_count = len(mode_data) // 15
    header = struct.pack("<BB", CMD_SET_GAIN_BLOCK, mode_count)
    checksum = sum(mode_data) & 0xFF
    return header + mode_data + struct.pack("B", checksum)


def parse_status_response(data: bytes, mode_index: int = 0) -> List[int]:
    """Parse STATUS response and extract parameters for a mode set.
    
    Args:
        data: Raw response bytes
        mode_index: Which mode set to extract (0-5)
    
    Returns:
        List of 15 raw parameter values
    """
    if len(data) < 3:
        return [0] * NUM_PARAMS

    cmd = data[0]
    status = data[1]
    # Embedded side sends: [cmd|0x80][status][error_code][data×90]
    if cmd == (CMD_STATUS | 0x80):
        if status != 0:
            return [0] * NUM_PARAMS
        # data payload starts at byte 3, each mode is NUM_PARAMS bytes
        start = 3 + mode_index * NUM_PARAMS
        if len(data) >= start + NUM_PARAMS:
            return list(data[start:start + NUM_PARAMS])
        return [0] * NUM_PARAMS

    return [0] * NUM_PARAMS


def parse_all_modes_response(data: bytes) -> List[List[int]]:
    """Parse STATUS response containing all 6 mode sets.

    Embedded response format: [cmd|0x80][status][error_code][mode0×15]...[mode5×15]
    Total: 3 + 6*15 = 93 bytes minimum.

    Args:
        data: Raw response bytes

    Returns:
        List of 6 lists, each containing 15 raw parameter values
    """
    empty = [[0] * NUM_PARAMS for _ in range(NUM_MODES)]

    if len(data) < 3:
        return empty

    cmd = data[0]
    status = data[1]

    if cmd != (CMD_STATUS | 0x80) or status != 0:
        return empty

    # Payload starts at byte 3
    payload = data[3:]
    results = []
    for i in range(NUM_MODES):
        start = i * NUM_PARAMS
        end = start + NUM_PARAMS
        if end <= len(payload):
            results.append(list(payload[start:end]))
        else:
            results.append([0] * NUM_PARAMS)

    while len(results) < NUM_MODES:
        results.append([0] * NUM_PARAMS)

    return results[:NUM_MODES]


# ─── AudioConfigFileData binary format (300 bytes) ──────────────
#
# Offset   0 : header_id[40]  "AUDIO_CFG_V1\0..." (null-padded)
# Offset  40 : version[10]    "1.0.0\0..."
# Offset  50 : set_count(1)   = 6
# Offset  51 : sets[6×15]     90 bytes of gain params
# Offset 141 : crc32(4)       CRC32 over bytes 0-140, big-endian (MPC866 BE)
# Offset 145 : unused_pad[155]
# Total      : 300 bytes
#
# CRC poly: 0xEDB88320 (standard reflected) — same as Python binascii.crc32

CFG_MAGIC         = b"AUDIO_CFG_V1"
CFG_HEADER_ID_LEN = 40
CFG_VERSION_LEN   = 30   # "VMM.mm.ff DD.MM.YYYY HH:MM:SS\0" + pad  (was 10)
CFG_FILE_SIZE     = 300
CFG_CRC_OFFSET    = 161  # crc32 at byte 161  (was 141)
CFG_CRC_COVERED   = 161  # CRC covers bytes 0-160  (was 141)
CFG_PARAMS_OFFSET = 71   # sets[] start at byte 71  (was 51)


def build_version_string(major: int, minor: int, fix: int, dt=None) -> bytes:
    """Build a 30-byte version field: 'VMM.mm.ff DD.MM.YYYY HH:MM:SS' null-padded.

    Args:
        major/minor/fix: Version numbers (0-99).
        dt: datetime object; uses datetime.now() if None.
    """
    from datetime import datetime as _dt
    if dt is None:
        dt = _dt.now()
    s = f"V{major:02d}.{minor:02d}.{fix:02d} {dt.day:02d}.{dt.month:02d}.{dt.year:04d} {dt.hour:02d}:{dt.minute:02d}:{dt.second:02d}"
    encoded = s.encode('ascii')[:CFG_VERSION_LEN - 1]      # leave room for \0
    return encoded.ljust(CFG_VERSION_LEN, b'\x00')


def parse_version_string(raw: bytes) -> Tuple[str, int, int, int]:
    """Parse a 30-byte version field.

    Returns (full_string, major, minor, fix).
    """
    full = raw.rstrip(b'\x00').decode('ascii', errors='replace')
    major = minor = fix = 0
    try:
        # Expected: "VMM.mm.ff ..."
        parts = full[1:].split('.')   # strip leading 'V', split by '.'
        major = int(parts[0])
        minor = int(parts[1])
        fix_str = parts[2].split(' ')[0]
        fix = int(fix_str)
    except (IndexError, ValueError):
        pass
    return full, major, minor, fix


def build_config_file(mode_params: List[List[int]],
                      header_id: str = "AUDIO_CFG_V1",
                      major: int = 1, minor: int = 0, fix: int = 0,
                      timestamp=None) -> bytes:
    """Serialize all 6 mode sets into a 300-byte AudioConfigFileData binary.

    MPC866 is big-endian; crc32 field is packed '>I'.
    timestamp: datetime object for version string; uses datetime.now() if None.
    """
    import binascii as _crc

    hdr = header_id.encode('ascii')[:CFG_HEADER_ID_LEN]
    hdr = hdr.ljust(CFG_HEADER_ID_LEN, b'\x00')

    ver = build_version_string(major, minor, fix, timestamp)
    set_count = bytes([NUM_MODES])

    params_bytes = b''
    for i in range(NUM_MODES):
        m = mode_params[i] if i < len(mode_params) else [0] * NUM_PARAMS
        params_bytes += bytes(v & 0xFF for v in m[:NUM_PARAMS])

    covered   = hdr + ver + set_count + params_bytes          # 161 bytes
    crc_val   = _crc.crc32(covered) & 0xFFFFFFFF
    crc_bytes = struct.pack('>I', crc_val)                    # big-endian (MPC866)
    padding   = bytes(CFG_FILE_SIZE - CFG_CRC_OFFSET - 4)    # 135 bytes

    result = covered + crc_bytes + padding
    assert len(result) == CFG_FILE_SIZE
    return result


def parse_config_file(data: bytes) -> Tuple[List[List[int]], str, str]:
    """Parse a 300-byte AudioConfigFileData binary.

    Returns (mode_params[6][15], version_str, header_id_str).
    Raises ValueError on bad magic, wrong size, or CRC mismatch.
    """
    import binascii as _crc

    if len(data) < CFG_FILE_SIZE:
        raise ValueError(f"File too small: {len(data)} B (expected {CFG_FILE_SIZE})")

    header_id_str = data[:CFG_HEADER_ID_LEN].rstrip(b'\x00').decode('ascii', errors='replace')

    ver_raw = data[CFG_HEADER_ID_LEN: CFG_HEADER_ID_LEN + CFG_VERSION_LEN]
    version_str, _, _, _ = parse_version_string(ver_raw)

    set_count = data[CFG_HEADER_ID_LEN + CFG_VERSION_LEN]
    if set_count != NUM_MODES:
        raise ValueError(f"Unexpected set_count: {set_count}")

    stored_crc   = struct.unpack('>I', data[CFG_CRC_OFFSET:CFG_CRC_OFFSET + 4])[0]
    computed_crc = _crc.crc32(data[:CFG_CRC_COVERED]) & 0xFFFFFFFF
    if stored_crc != computed_crc:
        raise ValueError(
            f"CRC32 mismatch — stored {stored_crc:#010x}, computed {computed_crc:#010x}"
        )

    modes = []
    for i in range(NUM_MODES):
        off = CFG_PARAMS_OFFSET + i * NUM_PARAMS
        modes.append(list(data[off: off + NUM_PARAMS]))

    return modes, version_str, header_id_str


# ─── File I/O helpers ───────────────────────────────────────────

def params_to_bytes(mode: int, param_values: List[int]) -> bytes:
    """Convert 15 param values for a mode to raw bytes.
    
    Args:
        mode: Mode index (0-5)
        param_values: List of 15 raw values
    
    Returns:
        Raw bytes (15 bytes per mode)
    """
    return bytes(v & 0xFF for v in param_values[:NUM_PARAMS])


def bytes_to_params(raw_bytes: bytes) -> List[int]:
    """Convert raw mode data bytes to 15 param values.
    
    Args:
        raw_bytes: Raw bytes (at least 15 bytes)
    
    Returns:
        List of 15 raw values
    """
    values = list(raw_bytes[:NUM_PARAMS])
    while len(values) < NUM_PARAMS:
        values.append(0)
    return values[:NUM_PARAMS]


def build_full_upload(mode_params: List[List[int]], version: int = 1) -> bytes:
    """Build complete upload packet for all modes.
    
    Args:
        mode_params: List of 6 lists, each with 15 raw parameter values
        version: Protocol version
    
    Returns:
        Full packet bytes ready to send over TCP
    """
    header = struct.pack("<BB", 0xA0, version)  # Magic + version
    mode_data = b""
    for params in mode_params[:NUM_MODES]:
        mode_data += bytes(v & 0xFF for v in params[:NUM_PARAMS])
    checksum = sum(mode_data) & 0xFF
    return header + mode_data + struct.pack("B", checksum)


def parse_full_download(data: bytes) -> Tuple[List[List[int]], int]:
    """Parse full download packet containing all modes.
    
    Args:
        data: Full packet bytes
    
    Returns:
        (mode_params, version) - list of mode params and version number
    """
    if len(data) < 3:
        return [[0] * NUM_PARAMS for _ in range(NUM_MODES)], 0
    
    version = data[1]
    mode_data = data[2:-1]  # Exclude header (2 bytes) and checksum (1 byte)
    
    results = []
    for i in range(NUM_MODES):
        start = i * NUM_PARAMS
        end = start + NUM_PARAMS
        if end <= len(mode_data):
            vals = list(mode_data[start:end])
            while len(vals) < NUM_PARAMS:
                vals.append(0)
            results.append(vals[:NUM_PARAMS])
        else:
            results.append([0] * NUM_PARAMS)
    
    while len(results) < NUM_MODES:
        results.append([0] * NUM_PARAMS)
    
    return results[:NUM_MODES], version
