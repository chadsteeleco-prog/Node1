# Pico4ML Pro Raspberry Pi Integration

Custom firmware and Python controller for full Pico4ML Pro functionality over USB.

This subproject contains:

- `Pico4ML_USB_Controller.ino` — Arduino firmware exposing a serial command protocol over USB CDC.
- `pico4ml_controller.py` — Python API for interacting with the device.
- `aircraft_logger.py` — Example PiAware integration and motion-triggered capture.
- `examples/` — small examples, including `multi_cam_capture.py` and `ov5642_config_example.ino`.
- `tests/` — unit tests for the Python controller using a `MockSerial`.
- `requirements.txt` — Python runtime requirements.

- CI: `/.github/workflows/ci.yml` — runs Python tests; optional Arduino compile job available via `workflow_dispatch` (manual run).

![CI](https://github.com/chadsteeleco-prog/Node1/actions/workflows/ci.yml/badge.svg)

Hardware library support

The firmware provides optional support for these libraries (install via Arduino Library Manager):

- Arducam (camera capture) — supports RDUCAM MEGA 3MP SPI modules
- SparkFun LSM6DS3 (IMU)
- Adafruit GFX + Adafruit ST7789 (LCD)

If the libraries are not available the firmware falls back to stub behavior for easy testing.

Multi-camera (4x RDUCAM MEGA 3MP SPI) wiring and notes

- The RDUCAM MEGA 5MP is a SPI camera module (OV5642 family). Multiple cameras can share SCK/MOSI/MISO lines but need unique CS (chip-select) pins.
- Default CS pins in the sketch: `CAM_CS0=5`, `CAM_CS1=6`, `CAM_CS2=7`, `CAM_CS3=8`. Change these to match your wiring.
- Ensure the camera power and SPI voltage levels match the Pico4ML board (use level shifters if necessary).
- To capture from camera 2 (index 1), send `CAPTURE:1` or `SNAP:1`.
- To stream 4 frames from camera 3 (index 2), send `STREAM:2:4`.

ArduCAM setup notes

- Install the ArduCAM library via Arduino Library Manager (search "ArduCAM").
- Power: connect camera VCC to 3.3V and GND to GND. Do NOT power camera with 5V unless it is 5V tolerant.
- SPI pins (example on RP2040 Pico): `SCK`->GP18, `MOSI`->GP19, `MISO`->GP16 (adjust per board/pinout). CS pins are defined as `CAM_CS#` in the sketch.
- If using multiple cameras, connect SCK/MOSI/MISO to all cameras and use separate CS lines.
- If you enable `USE_ARDUCAM` in the sketch, the firmware will attempt to use the ArduCAM APIs to capture JPEG frames; otherwise it falls back to a small embedded JPEG for testing.
- If you want the firmware to perform sensor-specific initialization (recommended for reliable captures with MEGA 5MP), either define `OV5642_CONFIG` and provide the ArduCAM OV5642 sensor helper (e.g., `sensor_ov5642.h`) in the Arduino include path, or enable `OV5642_INLINE_INIT` to use a small, inline register sequence embedded in the sketch. The `OV5642_INLINE_INIT` sequence is a conservative, minimal set of register writes and may need tuning for some modules.

OV5642 full register set

- The project includes an optional `sensor_ov5642.h` which contains a fuller OV5642 init sequence and resolution tables (enable by defining `OV5642_CONFIG`).
- The `OV5642_full_init()` function applies a comprehensive register set suitable for many MEGA 5MP modules.
- You can still use `SETRES:` at runtime to change resolution when `OV5642_INLINE_INIT` is enabled; when `OV5642_CONFIG` is enabled `OV5642_full_init()` and `OV5642_set_JPEG_size()` will be used instead.

OV5642 inline resolutions

- The inline init provides a small set of resolution sequences you can choose between at runtime with `SETRES:` or compile-time by modifying the sketch.
- Supported inline targets (names accepted by `SETRES:`): `2592x1944`, `1600x1200`, `1280x720`, `640x480`.
- Example: `SETRES:1600x1200` will apply the 1600x1200 register table (guarded by `OV5642_INLINE_INIT`).

- Reference: ArduCAM Mega getting started: https://www.arducam.com/docs/arducam-mega/arducam-mega-getting-started/

Example wiring (RP2040 pins shown):

- `3.3V` -> Camera VCC
- `GND` -> Camera GND
- `GP18` -> SCK
- `GP19` -> MOSI
- `GP16` -> MISO
- `CAM_CS0` -> Camera0 CS (e.g., GP5)
- `CAM_CS1` -> Camera1 CS (e.g., GP6)
- `CAM_CS2` -> Camera2 CS (e.g., GP7)
- `CAM_CS3` -> Camera3 CS (e.g., GP8)

Command examples

- `CAPTURE:0` : capture from camera 0
- `SNAP:2` : capture from camera 2
- `STREAM:1:3` : stream 3 frames from camera 1


Command protocol

- `CAPTURE` / `SNAP` : Capture single frame, firmware responds: `JPEG <len>` then <len> bytes.
- `STREAM:n` : Capture and send `n` frames.
- `IMU` : Returns `ax,ay,az,gx,gy,gz` as CSV.
- `MIC` : Returns integer microphone amplitude (simple analog fallback by default).
- `STATUS` : Multi-line status: `FW:..., CAMERA:..., IMU:..., LCD:..., END`.
- `DISPLAY:text` : Show `text` on LCD, responds `OK`.
- `SETRES:name` : Set resolution (`VGA`, `QVGA`, `QQVGA`) — responds `OK`.
- `SETQ:n` : Set JPEG quality (0-100) — responds `OK`.
- `INFO` : Returns firmware info lines then `OK`.

Python API additions

- `Pico4MLController.get_status()` -> returns dict from `STATUS`.
- `Pico4MLController.set_resolution(name)` -> sets resolution.
- `Pico4MLController.set_quality(q)` -> sets JPEG quality.
- `Pico4MLController.get_info()` -> returns dict of info from `INFO`.

Testing

- Run tests with `pytest` (they do not require hardware; the tests use `MockSerial`).

Example

```python
from pico4ml_controller import Pico4MLController
pico = Pico4MLController()
print(pico.get_info())
print(pico.get_status())
pico.set_resolution('QVGA')
pico.set_quality(60)
pico.capture_image('/tmp/test.jpg')
```
