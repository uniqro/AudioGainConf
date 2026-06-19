"""
TCP Client for Audio DSP Configuration.
Handles non-blocking socket communication with the DSP device.
Uses QTimer for async reads since PyQt sockets are not natively async.
"""

import socket
from PyQt5.QtCore import QObject, pyqtSignal, QTimer, QIODevice


class TCPClient(QObject):
    """Non-blocking TCP client for the Audio DSP protocol."""

    connected = pyqtSignal()
    disconnected = pyqtSignal()
    data_received = pyqtSignal(bytes)
    error = pyqtSignal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.socket = None
        self.timer = QTimer(self)
        self.timer.setSingleShot(False)
        self.timer.timeout.connect(self._on_timer)
        self.buffer = bytearray()
        # Default connection parameters
        self._host = "192.168.1.100"
        self._port = 5558
        self._is_connected = False

    @property
    def is_connected(self) -> bool:
        return self._is_connected

    @property
    def host(self) -> str:
        return self._host

    @host.setter
    def host(self, value: str):
        self._host = value

    @property
    def port(self) -> int:
        return self._port

    @port.setter
    def port(self, value: int):
        self._port = value

    def connect(self, host: str = None, port: int = None) -> bool:
        """Connect to the DSP device.
        
        Args:
            host: IP address (defaults to _host property)
            port: Port number (defaults to _port property, 5558)
        
        Returns:
            True if connection started successfully
        """
        host = host or self._host
        port = port or self._port

        # Close existing connection if any
        if self.socket:
            self._disconnect_internal()

        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.setblocking(False)
            self.socket.connect_ex((host, port))
            
            self._is_connected = True
            self._host = host
            self._port = port
            
            # Start polling for data
            self.timer.start(50)  # Check every 50ms
            
            self.connected.emit()
            return True
        except Exception as e:
            self._is_connected = False
            if self.socket:
                self.socket.close()
                self.socket = None
            self.error.emit(f"Connection failed: {e}")
            return False

    def disconnect(self) -> bool:
        """Disconnect from the DSP device."""
        self._disconnect_internal()
        return True

    def send(self, data: bytes) -> bool:
        """Send data to the DSP device.
        
        Args:
            data: Bytes to send
        
        Returns:
            True if data was sent successfully
        """
        if not self.socket or not self._is_connected:
            self.error.emit("Not connected")
            return False

        try:
            self.socket.sendall(data)
            return True
        except Exception as e:
            self._is_connected = False
            self.error.emit(f"Send failed: {e}")
            return False

    def send_command(self, cmd_bytes: bytes) -> bool:
        """Send a command byte to the DSP.
        
        Args:
            cmd_bytes: Command bytes (e.g., bytes([0x01]) for STATUS)
        
        Returns:
            True if command was sent
        """
        return self.send(cmd_bytes)

    def _on_timer(self):
        """Called periodically to check for incoming data."""
        if not self.socket or not self._is_connected:
            self.timer.stop()
            return

        try:
            data = self.socket.recv(4096)
            if data:
                self.buffer.extend(data)
                # Process complete packets
                self._process_buffer()
            else:
                # Connection closed by remote
                self._is_connected = False
                self.disconnected.emit()
                self.timer.stop()
                self._disconnect_internal()
        except ConnectionResetError:
            self._is_connected = False
            self.error.emit("Connection reset")
            self.disconnected.emit()
            self.timer.stop()
            self._disconnect_internal()
        except BlockingIOError:
            # No data available yet
            pass
        except Exception as e:
            self._is_connected = False
            self.error.emit(f"Receive error: {e}")
            self.timer.stop()
            self._disconnect_internal()

    def _process_buffer(self):
        """Process available buffer data and emit packets."""
        if len(self.buffer) > 0:
            data = bytes(self.buffer)
            self.buffer.clear()
            self.data_received.emit(data)

    def _disconnect_internal(self):
        """Internal disconnect without emitting signals."""
        self._is_connected = False
        self.timer.stop()
        self.buffer.clear()
        if self.socket:
            try:
                self.socket.close()
            except Exception:
                pass
            self.socket = None

    def cleanup(self):
        """Clean up resources."""
        self.disconnect()
