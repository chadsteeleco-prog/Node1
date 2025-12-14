"""Pico4MLController

Simple Python wrapper for communicating with the Pico4ML firmware over
USB CDC (serial). Supports capture, streaming, IMU, microphone and LCD.
"""

import io
import os
import time
from typing import Optional

try:
    import serial
    import serial.tools.list_ports
except Exception:  # pragma: no cover - environment may not have pyserial installed
    serial = None



class Pico4MLError(Exception):
    pass


class Pico4MLController:
    def __init__(self, port: Optional[str] = None, baud: int = 115200,
                 timeout: float = 10.0, serial_instance: Optional['serial.Serial'] = None):
        """Open serial connection. If `serial_instance` is provided, it will
        be used instead of opening a port (useful for tests).
        """
        self._owns_serial = False
        if serial_instance:
            self.ser = serial_instance
        else:
            if serial is None:
                raise Pico4MLError("pyserial required to open real hardware port; install pyserial or pass a serial_instance for testing")
            if port is None:
                port = self._auto_find_port()
                if port is None:
                    raise Pico4MLError("Could not find Pico4ML serial port")
            self.ser = serial.Serial(port, baud, timeout=timeout)
            self._owns_serial = True

    def _auto_find_port(self) -> Optional[str]:
        ports = list(serial.tools.list_ports.comports())
        for p in ports:
            if 'ACM' in p.device or 'USB' in p.device:
                return p.device
            # Try friendly name
            if 'Pico' in (p.description or '') or 'pico' in (p.description or ''):
                return p.device
        return None

    def close(self):
        if self._owns_serial and self.ser and self.ser.is_open:
            self.ser.close()

    def send_command(self, cmd: str):
        if not cmd.endswith('\n'):
            cmd = cmd + '\n'
        self.ser.write(cmd.encode('ascii', errors='ignore'))

    def _readline(self, timeout: Optional[float] = None) -> str:
        old_to = self.ser.timeout
        if timeout is not None:
            self.ser.timeout = timeout
        try:
            line = self.ser.readline()
            return line.decode('utf-8', errors='ignore').rstrip('\r\n')
        finally:
            if timeout is not None:
                self.ser.timeout = old_to

    def _read_exact(self, n: int) -> bytes:
        data = b''
        while len(data) < n:
            chunk = self.ser.read(n - len(data))
            if not chunk:
                raise Pico4MLError(f"Timeout while reading {n} bytes (got {len(data)})")
            data += chunk
        return data

    def capture_image(self, out_path: str):
        """Capture a single image and save it to `out_path`.
        The firmware sends a header: "JPEG <len>\n" followed by raw bytes.
        """
        self.send_command('CAPTURE')
        header = self._readline(timeout=5.0)
        if not header.startswith('JPEG '):
            raise Pico4MLError(f"Unexpected response: {header}")
        length = int(header.split(' ', 1)[1])
        data = self._read_exact(length)
        with open(out_path, 'wb') as f:
            f.write(data)
        return out_path

    def capture_image_from(self, out_path: str, camera_index: int = 0):
        """Capture a single image from `camera_index` and save it."""
        self.send_command(f'CAPTURE:{int(camera_index)}')
        header = self._readline(timeout=8.0)
        if not header.startswith('JPEG '):
            raise Pico4MLError(f"Unexpected response: {header}")
        length = int(header.split(' ', 1)[1])
        data = self._read_exact(length)
        with open(out_path, 'wb') as f:
            f.write(data)
        return out_path

    def stream_frames(self, n: int, save_dir: str):
        os.makedirs(save_dir, exist_ok=True)
        self.send_command(f'STREAM:{int(n)}')
        saved = []
        for i in range(n):
            header = self._readline(timeout=10.0)
            if not header.startswith('JPEG '):
                raise Pico4MLError(f"Unexpected response during stream: {header}")
            length = int(header.split(' ', 1)[1])
            data = self._read_exact(length)
            path = os.path.join(save_dir, f'frame_{i:04d}.jpg')
            with open(path, 'wb') as f:
                f.write(data)
            saved.append(path)
        return saved

    def stream_frames_from(self, camera_index: int, n: int, save_dir: str):
        os.makedirs(save_dir, exist_ok=True)
        self.send_command(f'STREAM:{int(camera_index)}:{int(n)}')
        saved = []
        for i in range(n):
            header = self._readline(timeout=15.0)
            if not header.startswith('JPEG '):
                raise Pico4MLError(f"Unexpected response during stream: {header}")
            length = int(header.split(' ', 1)[1])
            data = self._read_exact(length)
            path = os.path.join(save_dir, f'cam{camera_index}_frame_{i:04d}.jpg')
            with open(path, 'wb') as f:
                f.write(data)
            saved.append(path)
        return saved

    def read_imu(self):
        self.send_command('IMU')
        line = self._readline(timeout=2.0)
        parts = [float(x) for x in line.split(',')]
        return dict(ax=parts[0], ay=parts[1], az=parts[2], gx=parts[3], gy=parts[4], gz=parts[5])

    def read_microphone(self) -> int:
        self.send_command('MIC')
        line = self._readline(timeout=2.0)
        return int(line.strip())

    def display_text(self, text: str) -> bool:
        # Firmware responds with OK
        self.send_command(f'DISPLAY:{text}')
        resp = self._readline(timeout=2.0)
        return resp.strip() == 'OK'

    def get_status(self, timeout: float = 2.0) -> dict:
        """Request STATUS and return a dict of reported fields."""
        self.send_command('STATUS')
        info = {}
        while True:
            line = self._readline(timeout=timeout)
            if not line:
                break
            if line == 'END':
                break
            if ':' in line:
                k, v = line.split(':', 1)
                info[k] = v
            else:
                # fallback key/value
                parts = line.split('=')
                if len(parts) == 2:
                    info[parts[0]] = parts[1]
        return info

    def set_resolution(self, name: str) -> bool:
        self.send_command(f'SETRES:{name}')
        resp = self._readline(timeout=2.0)
        return resp.strip() == 'OK'

    def set_quality(self, q: int) -> bool:
        self.send_command(f'SETQ:{int(q)}')
        resp = self._readline(timeout=2.0)
        return resp.strip() == 'OK'

    def get_info(self, timeout: float = 2.0) -> dict:
        self.send_command('INFO')
        info = {}
        while True:
            line = self._readline(timeout=timeout)
            if not line:
                break
            if line == 'OK':
                break
            if ':' in line:
                k, v = line.split(':', 1)
                info[k] = v
        return info


if __name__ == '__main__':
    # Quick demonstration when running directly
    import tempfile
    try:
        pico = Pico4MLController()
    except Exception as e:
        print('Could not open Pico4ML serial:', e)
        raise SystemExit(1)
    out = os.path.join(tempfile.gettempdir(), 'pico_capture.jpg')
    print('Capturing to', out)
    pico.capture_image(out)
    print('Saved:', out)
    pico.close()
