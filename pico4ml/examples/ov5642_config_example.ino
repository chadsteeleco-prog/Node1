// ov5642_config_example.ino
// Example showing how to enable OV5642 full init in the Pico4ML firmware.

// Uncomment to enable the ArduCAM sensor helper usage and set a default resolution
// #define USE_ARDUCAM
// #define OV5642_CONFIG
// #define OV5642_DEFAULT_RES "2592x1944"

/*
  Wiring notes (RP2040 example):
   - 3.3V -> Camera VCC
   - GND  -> Camera GND
   - GP18 -> SCK
   - GP19 -> MOSI
   - GP16 -> MISO
   - Set CS pins (e.g., GP5..GP8) and update `CAM_CS#` in the main sketch
*/

// After enabling `OV5642_CONFIG` and customizing the macros above, open
// `Pico4ML_USB_Controller.ino` and compile+upload.

void setup() {}
void loop() {}
