import io
import os
import threading
import time

import pytest
import sys
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from pico4ml_controller import Pico4MLController


SMALL_JPEG = bytes([
    0xFF,0xD8,0xFF,0xDB,0x00,0x43,0x00,0x03,0x02,0x02,0x03,0x02,0x02,0x03,0x03,0x03,
    0x03,0x04,0x03,0x03,0x04,0x05,0x08,0x05,0x05,0x04,0x04,0x05,0x0A,0x07,0x07,0x06,
    0x08,0x0C,0x0A,0x0C,0x0C,0x0B,0x0A,0x0B,0x0B,0x0D,0x0E,0x12,0x10,0x0D,0x0E,0x11,
    0x0E,0x0B,0x0B,0x10,0x16,0x10,0x11,0x13,0x14,0x15,0x15,0x15,0x0C,0x0F,0x17,0x18,0x16,
    0x14,0x18,0x12,0x14,0x15,0x14,0xFF,0xC0,0x00,0x11,0x08,0x00,0x01,0x00,0x01,0x03,0x01,
    0x11,0x00,0x02,0x11,0x01,0x03,0x11,0x01,0xFF,0xC4,0x00,0x14,0x00,0x01,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xDA,0x00,0x0C,
    0x03,0x01,0x00,0x02,0x11,0x03,0x11,0x00,0x3F,0x00,0xFF,0xD9
])


class MockSerial:
    def __init__(self):
        self.in_buf = bytearray()
        self.timeout = 1
        self.is_open = True
        self.lock = threading.Lock()

    def write(self, data: bytes):
        cmd = data.decode('ascii', errors='ignore')
        cmd = cmd.strip()
        # record last command for tests
        self.last_cmd = cmd
        if cmd == 'CAPTURE':
            header = f'JPEG {len(SMALL_JPEG)}\n'.encode('ascii')
            with self.lock:
                self.in_buf += header + SMALL_JPEG
        elif cmd.startswith('CAPTURE:') or cmd.startswith('SNAP:'):
            # CAPTURE:<idx>
            # echo same JPEG for any camera index
            header = f'JPEG {len(SMALL_JPEG)}\n'.encode('ascii')
            with self.lock:
                self.in_buf += header + SMALL_JPEG
        elif cmd.startswith('STREAM:'):
            parts = cmd.split(':')
            if len(parts) == 2:
                n = int(parts[1])
                with self.lock:
                    for _ in range(n):
                        self.in_buf += f'JPEG {len(SMALL_JPEG)}\n'.encode('ascii') + SMALL_JPEG
            elif len(parts) == 3:
                # STREAM:<idx>:<n>
                n = int(parts[2])
                with self.lock:
                    for _ in range(n):
                        self.in_buf += f'JPEG {len(SMALL_JPEG)}\n'.encode('ascii') + SMALL_JPEG
        elif cmd == 'IMU':
            with self.lock:
                self.in_buf += b'0.10,0.20,0.98,0.01,0.02,0.03\n'
        elif cmd == 'MIC':
            with self.lock:
                self.in_buf += b'12\n'
        elif cmd == 'STATUS':
            with self.lock:
                self.in_buf += b'FW:pico4ml-fw-0.2\n'
                self.in_buf += b'CAMERA:STUB\n'
                self.in_buf += b'IMU:STUB\n'
                self.in_buf += b'LCD:STUB\n'
                self.in_buf += b'END\n'
        elif cmd.startswith('SETRES:'):
            with self.lock:
                self.in_buf += b'OK\n'
        elif cmd.startswith('SETQ:'):
            with self.lock:
                self.in_buf += b'OK\n'
        elif cmd == 'INFO':
            with self.lock:
                self.in_buf += b'FW:pico4ml-fw-0.2\n'
                self.in_buf += b'FEATURES:CAPTURE,IMU,MIC\n'
                self.in_buf += b'OK\n'
        elif cmd.startswith('DISPLAY:'):
            with self.lock:
                self.in_buf += b'OK\n'
        else:
            with self.lock:
                self.in_buf += b'ERR:UNKNOWN\n'

    def readline(self):
        start = time.time()
        while True:
            with self.lock:
                idx = self.in_buf.find(b'\n')
                if idx != -1:
                    line = self.in_buf[:idx+1]
                    del self.in_buf[:idx+1]
                    return bytes(line)
            if time.time() - start > self.timeout:
                return b''
            time.sleep(0.001)

    def read(self, n: int):
        start = time.time()
        while True:
            with self.lock:
                if len(self.in_buf) > 0:
                    take = min(n, len(self.in_buf))
                    out = self.in_buf[:take]
                    del self.in_buf[:take]
                    return bytes(out)
            if time.time() - start > self.timeout:
                return b''
            time.sleep(0.001)

    def close(self):
        self.is_open = False


def test_capture_image(tmp_path):
    mock = MockSerial()
    pico = Pico4MLController(serial_instance=mock)
    out = tmp_path / 'test.jpg'
    pico.capture_image(str(out))
    assert out.exists()
    assert out.stat().st_size > 0


def test_read_imu_and_mic_and_display():
    mock = MockSerial()
    pico = Pico4MLController(serial_instance=mock)
    imu = pico.read_imu()
    assert pytest.approx(imu['ax'], 0.01) == 0.10
    assert isinstance(pico.read_microphone(), int)
    assert pico.display_text('Hello') is True


def test_stream_frames(tmp_path):
    mock = MockSerial()
    pico = Pico4MLController(serial_instance=mock)
    saved = pico.stream_frames(3, str(tmp_path))
    assert len(saved) == 3
    for p in saved:
        assert os.path.exists(p)
        assert os.path.getsize(p) > 0


def test_status_and_setters():
    mock = MockSerial()
    pico = Pico4MLController(serial_instance=mock)
    status = pico.get_status()
    assert 'FW' in status
    assert pico.set_resolution('QVGA') is True
    assert pico.set_quality(50) is True
    assert mock.last_cmd == 'SETQ:50'
    info = pico.get_info()
    assert info.get('FW', '').startswith('pico4ml-fw')


def test_capture_from_camera(tmp_path):
    mock = MockSerial()
    pico = Pico4MLController(serial_instance=mock)
    out = tmp_path / 'cam1.jpg'
    pico.capture_image_from(str(out), camera_index=1)
    assert out.exists()
    assert out.stat().st_size > 0


def test_stream_from_camera(tmp_path):
    mock = MockSerial()
    pico = Pico4MLController(serial_instance=mock)
    saved = pico.stream_frames_from(2, 2, str(tmp_path))
    assert len(saved) == 2
    for p in saved:
        assert os.path.exists(p)
        assert os.path.getsize(p) > 0
