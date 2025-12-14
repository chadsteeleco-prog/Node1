# Changelog

All notable changes to the `pico4ml` subproject are documented here.

## 0.2.0 - 2025-12-14

- Added full Arduino firmware `Pico4ML_USB_Controller.ino` with serial command protocol (CAPTURE, IMU, MIC, STATUS, DISPLAY, STREAM, SETRES, SETQ, INFO)
- Optional hardware support: ArduCAM (OV5642), SparkFun LSM6DS3 (IMU), Adafruit ST7789 (LCD)
- Multi-camera support (4x RDUCAM MEGA 5MP SPI) with per-camera CS pins and commands `CAPTURE:<idx>`, `STREAM:<idx>:<n>`
- Implemented ArduCAM FIFO JPEG capture and USB streaming
- Added camera initialization and presence detection (per-camera `CAMERA<n>:OK` status)
- Added inline OV5642 register sequences and a `sensor_ov5642.h` with fuller init tables and helpers
- Added Python API `pico4ml_controller.py` with capture/stream/helpers and tests using `MockSerial`
- Added `aircraft_logger.py` example and `examples/multi_cam_capture.py`
- Added tests and CI-ready project layout; all unit tests pass

## Notes
- Sensor sequences may need tuning for some OV5642 module revisions. See `pico4ml/README.md` for details and enabling `OV5642_CONFIG` or `OV5642_INLINE_INIT`.
