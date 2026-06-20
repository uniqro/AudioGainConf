"""
TCP Client for Audio DSP Configuration.
Non-blocking socket with:
 - OS-level TCP keep-alive to detect silent drops
 - getpeername() verification before emitting connected()
 - Exponential-backoff auto-reconnect (user disconnect cancels retries)
"""

import socket
import select
from PyQt5.QtCore import QObject, pyqtSignal, QTimer


_INITIAL_RETRY_DELAY_MS = 1_000   # 1 s
_MAX_RETRY_DELAY_MS     = 30_000  # 30 s
_POLL_INTERVAL_MS       = 50      # 50 ms poll


class TCPClient(QObject):
    connected    = pyqtSignal()
    disconnected = pyqtSignal()
    data_received = pyqtSignal(bytes)
    error        = pyqtSignal(str)
    reconnecting = pyqtSignal(int, int)   # (attempt_number, delay_seconds)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.socket: socket.socket | None = None
        self.buffer = bytearray()

        self._host = "192.168.1.100"
        self._port = 5558
        self._is_connected  = False
        self._connecting    = False   # SYN_SENT phase
        self._user_stopped  = False   # True after explicit disconnect()
        self._retry_count   = 0
        self._retry_delay_ms = _INITIAL_RETRY_DELAY_MS

        # Poll timer: runs every 50 ms while socket exists
        self._poll = QTimer(self)
        self._poll.setSingleShot(False)
        self._poll.timeout.connect(self._on_timer)

        # Retry timer: fires once after backoff delay
        self._retry = QTimer(self)
        self._retry.setSingleShot(True)
        self._retry.timeout.connect(self._do_retry)

        # Backwards-compat alias used by legacy call-sites
        self.timer = self._poll

    # ── Public API ────────────────────────────────────────────────

    @property
    def is_connected(self) -> bool:
        return self._is_connected

    @property
    def host(self) -> str:
        return self._host

    @host.setter
    def host(self, v: str):
        self._host = v

    @property
    def port(self) -> int:
        return self._port

    @port.setter
    def port(self, v: int):
        self._port = v

    def connect(self, host: str = None, port: int = None) -> bool:
        """Start connection (user-initiated). Resets retry state."""
        if host:
            self._host = host
        if port:
            self._port = port
        self._user_stopped   = False
        self._retry_count    = 0
        self._retry_delay_ms = _INITIAL_RETRY_DELAY_MS
        self._retry.stop()
        return self._do_connect()

    def disconnect(self) -> bool:
        """User-initiated disconnect — cancels auto-reconnect."""
        self._user_stopped = True
        self._retry.stop()
        was_connected = self._is_connected or self._connecting
        self._disconnect_internal()
        if was_connected:
            self.disconnected.emit()
        return True

    def send(self, data: bytes) -> bool:
        if not self.socket or not self._is_connected:
            self.error.emit("Not connected")
            return False
        try:
            self.socket.sendall(data)
            return True
        except Exception as e:
            self.error.emit(f"Send failed: {e}")
            self._disconnect_internal()
            self.disconnected.emit()
            self._schedule_retry()
            return False

    def send_command(self, cmd: bytes) -> bool:
        return self.send(cmd)

    def cleanup(self):
        self.disconnect()

    # ── Internal ─────────────────────────────────────────────────

    def _do_connect(self) -> bool:
        self._disconnect_internal()
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.setblocking(False)
            # OS-level keep-alive so silent drops are detected
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
            err = sock.connect_ex((self._host, self._port))
            # WSAEWOULDBLOCK (10035 Win) or EINPROGRESS (115 Linux) → still connecting
            if err not in (0, 10035, 115):
                sock.close()
                raise OSError(err, f"connect_ex returned {err}")
            self.socket     = sock
            self._connecting = True
            self._is_connected = False
            self._poll.start(_POLL_INTERVAL_MS)
            return True
        except Exception as e:
            self._connecting = False
            self.error.emit(f"Connection failed: {e}")
            return False

    def _do_retry(self):
        if self._user_stopped:
            return
        self._retry_count += 1
        self.reconnecting.emit(self._retry_count, self._retry_delay_ms // 1000)
        self._do_connect()

    def _schedule_retry(self):
        if self._user_stopped:
            return
        self._retry.start(self._retry_delay_ms)
        self._retry_delay_ms = min(self._retry_delay_ms * 2, _MAX_RETRY_DELAY_MS)

    def _on_timer(self):
        if not self.socket:
            self._poll.stop()
            return

        # ── Phase 1: waiting for SYN-ACK ──
        if self._connecting:
            try:
                self.socket.getpeername()
                # Connected!
                self._connecting     = False
                self._is_connected   = True
                self._retry_count    = 0
                self._retry_delay_ms = _INITIAL_RETRY_DELAY_MS
                self.connected.emit()
            except OSError:
                # Check for definitive failure via select
                _, writable, exceptional = select.select(
                    [], [self.socket], [self.socket], 0
                )
                if exceptional:
                    self._connecting = False
                    err_code = self.socket.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
                    self._disconnect_internal()
                    self.error.emit(f"Connection refused (err {err_code})")
                    self._schedule_retry()
            return

        # ── Phase 2: normal receive ──
        if not self._is_connected:
            self._poll.stop()
            return

        try:
            data = self.socket.recv(4096)
            if data:
                self.buffer.extend(data)
                self._process_buffer()
            else:
                # Graceful close by remote
                self._is_connected = False
                self.disconnected.emit()
                self._poll.stop()
                self._disconnect_internal()
                self._schedule_retry()
        except ConnectionResetError:
            self._is_connected = False
            self.error.emit("Connection reset by device")
            self.disconnected.emit()
            self._poll.stop()
            self._disconnect_internal()
            self._schedule_retry()
        except BlockingIOError:
            pass  # no data yet
        except Exception as e:
            self._is_connected = False
            self.error.emit(f"Receive error: {e}")
            self._poll.stop()
            self._disconnect_internal()
            self._schedule_retry()

    def _process_buffer(self):
        if self.buffer:
            data = bytes(self.buffer)
            self.buffer.clear()
            self.data_received.emit(data)

    def _disconnect_internal(self):
        self._is_connected = False
        self._connecting   = False
        self._poll.stop()
        self.buffer.clear()
        if self.socket:
            try:
                self.socket.close()
            except Exception:
                pass
            self.socket = None
