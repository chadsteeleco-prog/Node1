/*
  Pico4ML_USB_Controller.ino
  Firmware exposing a serial command protocol over USB CDC with optional
  support for camera (Arducam), IMU (LSM6DS3), and LCD (ST77xx).

  To enable hardware features, define the following in the Arduino IDE's
  build flags or uncomment the corresponding lines below (and install the
  required libraries):

    #define USE_ARDUCAM    // requires Arducam_Mini module
    #define USE_LSM6DS3    // requires SparkFun LSM6DS3 library
    #define USE_ST7789     // requires Adafruit ST7789 library

  Commands (newline-terminated):
    CAPTURE         -> sends "JPEG <len>\n" then <len> bytes of JPEG
    SNAP            -> alias for CAPTURE
    IMU             -> sends CSV floats: ax,ay,az,gx,gy,gz\n
    MIC             -> sends integer amplitude\n
    STATUS          -> sends multi-line status ending with "END\n"
    DISPLAY:text    -> show text on LCD (OK)\n
    STREAM:n        -> sends n JPEGs
    SETRES:name     -> set resolution (VGA,QVGA,QQVGA) -> OK\n
    SETQ:n          -> set JPEG quality (0-100) -> OK\n
    INFO            -> send firmware info -> OK\n
  This sketch will fall back to built-in stub behavior when hardware
  libraries are not enabled, allowing easy testing with the provided
  Python test harness.
*/

#include <Arduino.h>
#include <Wire.h>

#if 0
// Uncomment to enable real hardware support and ensure libraries are
// installed through the Arduino Library Manager.
// #define USE_ARDUCAM
// #define USE_LSM6DS3
// #define USE_ST7789
#endif

#ifdef USE_ARDUCAM
#include <SPI.h>
#include <ArduCAM.h>
// This example assumes RDUCAM MEGA 5MP (OV5642 family) on SPI.
// Define CS pins for up to 4 cameras. Adjust pins for your wiring.
#define CAM_CS0 5
#define CAM_CS1 6
#define CAM_CS2 7
#define CAM_CS3 8

// Number of cameras supported in this build
#define CAMERA_COUNT 4

ArduCAM *cams[CAMERA_COUNT];
bool cam_present[CAMERA_COUNT];
#ifdef OV5642_CONFIG
// If you have the OV5642 helper headers from ArduCAM examples, define
// `OV5642_CONFIG` (e.g., in build flags) and ensure the header providing
// `OV5642_set_JPEG_size()` and related helpers is available in the include
// path. This allows the firmware to call the sensor-specific helper functions
// to set resolution and other registers.
#include "sensor_ov5642.h"
#ifndef OV5642_DEFAULT_RES
#define OV5642_DEFAULT_RES "2592x1944"
#endif
#endif
#endif

#ifdef USE_LSM6DS3
#include <SparkFunLSM6DS3.h>
SparkFunLSM6DS3 imu;
#endif

#ifdef USE_ST7789
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#define TFT_CS 10
#define TFT_DC 8
#define TFT_RST 9
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
#endif

#include <Arduino.h>

// A tiny valid JPEG image stored in PROGMEM (fallback when no camera)
const uint8_t small_jpeg[] PROGMEM = {
  0xFF,0xD8,0xFF,0xDB,0x00,0x43,0x00,0x03,0x02,0x02,0x03,0x02,0x02,0x03,0x03,0x03,
  0x03,0x04,0x03,0x03,0x04,0x05,0x08,0x05,0x05,0x04,0x04,0x05,0x0A,0x07,0x07,0x06,
  0x08,0x0C,0x0A,0x0C,0x0C,0x0B,0x0A,0x0B,0x0B,0x0D,0x0E,0x12,0x10,0x0D,0x0E,0x11,
  0x0E,0x0B,0x0B,0x10,0x16,0x10,0x11,0x13,0x14,0x15,0x15,0x15,0x0C,0x0F,0x17,0x18,0x16,
  0x14,0x18,0x12,0x14,0x15,0x14,0xFF,0xC0,0x00,0x11,0x08,0x00,0x01,0x00,0x01,0x03,0x01,
  0x11,0x00,0x02,0x11,0x01,0x03,0x11,0x01,0xFF,0xC4,0x00,0x14,0x00,0x01,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xDA,0x00,0x0C,
  0x03,0x01,0x00,0x02,0x11,0x03,0x11,0x00,0x3F,0x00,0xFF,0xD9
};

static const char *FW_VERSION = "pico4ml-fw-0.2";
static const char *FEATURES = "CAPTURE,IMU,MIC,LCD,STREAM,SETRES,SETQ,INFO";

// runtime settings
enum ImageRes { RES_VGA=0, RES_QVGA=1, RES_QQVGA=2 };
static ImageRes currentRes = RES_QVGA;
static int jpegQuality = 80;

void sendJPEG(const uint8_t *data, size_t len) {
  // Header: JPEG <len>\n
  Serial.print("JPEG ");
  Serial.print(len);
  Serial.print('\n');
  // send binary
  for (size_t i = 0; i < len; ++i) {
    uint8_t b = pgm_read_byte_near(data + i);
    Serial.write(b);
  }
}

// If Arducam available, provide a wrapper that captures to a buffer and
// sends the JPEG bytes via Serial. Otherwise use the small_jpeg fallback.
void sendJPEGBuffer(const uint8_t *data, size_t len) {
  // header then raw bytes
  Serial.print("JPEG ");
  Serial.print(len);
  Serial.print('\n');
  for (size_t i = 0; i < len; ++i) Serial.write(data[i]);
}

void captureAndSend(int camIndex = 0) {
#ifdef USE_ARDUCAM
  if (camIndex < 0 || camIndex >= CAMERA_COUNT) camIndex = 0;
  ArduCAM *c = cams[camIndex];
  if (c == NULL) {
    sendJPEG(small_jpeg, sizeof(small_jpeg));
    return;
  }
  // Attempt a real ArduCAM JPEG capture using common ArduCAM APIs.
  c->clear_fifo_flag();
  c->start_capture();
  // wait for capture complete
  unsigned long start = millis();
  while (!(c->get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK))) {
    if (millis() - start > 2000) break; // timeout
    delay(5);
  }
  uint32_t length = c->read_fifo_length();
  if (length == 0 || length > 2UL * 1024UL * 1024UL) {
    // fallback if capture fails
    sendJPEG(small_jpeg, sizeof(small_jpeg));
    return;
  }

  // Read FIFO in burst mode and stream via Serial
  c->CS_LOW();
  c->set_fifo_burst();
  // Send header
  Serial.print("JPEG ");
  Serial.print(length);
  Serial.print('\n');
  const uint32_t chunk = 256;
  uint8_t temp = 0;
  for (uint32_t i = 0; i < length; ++i) {
    temp = SPI.transfer(0x00);
    Serial.write(temp);
  }
  c->CS_HIGH();
#else
  (void)camIndex;
  sendJPEG(small_jpeg, sizeof(small_jpeg));
#endif
}

void handleCapture() {
  captureAndSend();
}

void handleIMU() {
#ifdef USE_LSM6DS3
  if (imu.begin() == 0) {
    float ax = imu.readFloatAccelX();
    float ay = imu.readFloatAccelY();
    float az = imu.readFloatAccelZ();
    float gx = imu.readFloatGyroX();
    float gy = imu.readFloatGyroY();
    float gz = imu.readFloatGyroZ();
    Serial.printf("%f,%f,%f,%f,%f,%f\n", ax, ay, az, gx, gy, gz);
    return;
  }
#endif
  // Fallback fake values
  Serial.println("0.01,0.02,0.98,0.10,0.20,0.30");
}

void handleMIC() {
  // Simple analog microphone on A0 as fallback
#ifdef MIC_PIN
  int v = analogRead(MIC_PIN);
  Serial.println(v);
#else
  Serial.println(42);
#endif
}

void handleSTATUS() {
  Serial.printf("FW:%s\n", FW_VERSION);
#ifdef USE_ARDUCAM
  Serial.println("CAMERA:ENABLED");
  // Per-camera presence
  for (int i = 0; i < CAMERA_COUNT; ++i) {
    Serial.print("CAMERA"); Serial.print(i); Serial.print(":");
    Serial.println(cam_present[i] ? "OK" : "ERR");
  }
#else
  Serial.println("CAMERA:STUB");
#endif
#ifdef USE_LSM6DS3
  Serial.println("IMU:ENABLED");
#else
  Serial.println("IMU:STUB");
#endif
#ifdef MIC_PIN
  Serial.println("MIC:ANALOG");
#else
  Serial.println("MIC:STUB");
#endif
#ifdef USE_ST7789
  Serial.println("LCD:ENABLED");
#else
  Serial.println("LCD:STUB");
#endif
  Serial.printf("RES:%d\n", (int)currentRes);
  Serial.printf("Q:%d\n", jpegQuality);
  Serial.printf("FEATURES:%s\n", FEATURES);
  Serial.println("END");
}

void handleDISPLAY(const String &payload) {
#ifdef USE_ST7789
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.println(payload);
  Serial.println("OK");
#else
  (void)payload;
  Serial.println("OK");
#endif
}

void handleSTREAM(int n) {
  for (int i = 0; i < n; ++i) {
    captureAndSend();
    delay(200); // slight pause between frames
  }
}

void handleSetRes(const String &arg) {
  String a = arg;
  a.toUpperCase();
  if (a == "VGA") currentRes = RES_VGA;
  else if (a == "QVGA") currentRes = RES_QVGA;
  else if (a == "QQVGA") currentRes = RES_QQVGA;
#ifdef OV5642_INLINE_INIT
  // Map common names to our inline OV5642 resolution set
  if (a == "2592x1944" || a == "FULL") ov5642_set_resolution(OV5642_RES_2592x1944);
  else if (a == "1600x1200" || a == "UXGA") ov5642_set_resolution(OV5642_RES_1600x1200);
  else if (a == "1280x720" || a == "HD") ov5642_set_resolution(OV5642_RES_1280x720);
  else if (a == "640x480" || a == "VGA") ov5642_set_resolution(OV5642_RES_640x480);
#endif
  Serial.println("OK");
}

void handleSetQ(int q) {
  if (q < 0) q = 0;
  if (q > 100) q = 100;
  jpegQuality = q;
  Serial.println("OK");
}

void handleINFO() {
  Serial.printf("FW:%s\n", FW_VERSION);
  Serial.printf("FEATURES:%s\n", FEATURES);
  Serial.println("OK");
}

String readLine() {
  String line;
  while (true) {
    if (Serial.available()) {
      int c = Serial.read();
      if (c == '\n') break;
      if (c == '\r') continue;
      line += (char)c;
    }
  }
  return line;
}

void setup() {
  Serial.begin(115200);
  // Wait for host
  while (!Serial) delay(10);
#ifdef USE_ARDUCAM
  SPI.begin();
  // Instantiate ArduCAM objects for each CS pin. Adjust model if needed.
  cams[0] = new ArduCAM(OV5642, CAM_CS0);
  cams[1] = new ArduCAM(OV5642, CAM_CS1);
  cams[2] = new ArduCAM(OV5642, CAM_CS2);
  cams[3] = new ArduCAM(OV5642, CAM_CS3);
  // Note: additional camera-specific init may be required depending on your board.
#if 1
  // Try basic initialization for each camera (MEGA 5MP / OV5642 family)
  for (int i = 0; i < CAMERA_COUNT; ++i) {
    if (cams[i]) {
      Serial.print("Init camera "); Serial.println(i);
      // Common ArduCAM init steps. Some ArduCAM variants expose helper
      // functions like OV5642_set_JPEG_size(). Adjust as needed for your model.
      Wire.begin();
      cams[i]->set_format(JPEG);
      cams[i]->InitCAM();
      delay(50);
#ifdef OV5642
  // Attempt to set a sensible default JPEG size for MEGA 5MP modules.
  // If you have sensor helper functions available (see `sensor_ov5642.h`),
  // enable `OV5642_CONFIG` to call them.
  #ifdef OV5642_CONFIG
    OV5642_full_init();
    OV5642_set_JPEG_size(OV5642_DEFAULT_RES);
    OV5642_set_JPEG_quality(jpegQuality);
  #endif
#endif
      delay(50);
      // Attempt a quick capture to verify camera is responding
      cam_present[i] = false;
      cams[i]->clear_fifo_flag();
      cams[i]->start_capture();
      unsigned long t0 = millis();
      while (!(cams[i]->get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK))) {
        if (millis() - t0 > 1000) break;
        delay(5);
      }
      uint32_t l = cams[i]->read_fifo_length();
      if (l > 16 && l < (2UL * 1024UL * 1024UL)) {
        cam_present[i] = true;
        Serial.print("Camera "); Serial.print(i); Serial.println(" OK");
      } else {
        cam_present[i] = false;
        Serial.print("Camera "); Serial.print(i); Serial.println(" ERROR");
      }
    }
  }
#endif
  #ifdef OV5642_INLINE_INIT
  // Minimal inline OV5642 initialization (simplified). This provides a small
  // set of register writes that help the sensor come up in JPEG mode. For full
  // register sequences use the ArduCAM provided `sensor_ov5642.h` or the
  // official ArduCAM example sequences.

  #define OV5642_I2C_ADDR (0x78 >> 1)

  static void ov5642_write_reg(uint16_t reg, uint8_t val) {
    Wire.beginTransmission(OV5642_I2C_ADDR);
    Wire.write((reg >> 8) & 0xFF);
    Wire.write(reg & 0xFF);
    Wire.write(val);
    Wire.endTransmission();
    delay(2);
  }

  static void ov5642_init_basic() {
    // A short, conservative sequence adapted for many OV5642 modules.
    const struct { uint16_t a; uint8_t v; } regs[] = {
      {0x3008, 0x82}, // system reset
      {0x3103, 0x03}, // system clock from PLL
      {0x3017, 0xff}, {0x3018, 0xff},
      {0x3621, 0xe0}, {0x3704, 0xa0}, {0x3703, 0x5a}, {0x370b, 0x1a},
      {0x3808, 0x05}, {0x3809, 0x00}, // set default output size (e.g., 1280x720)
      {0x380a, 0x02}, {0x380b, 0xd0},
      {0x3a0f, 0x30}, {0x3a10, 0x28}
    };
    for (unsigned int i = 0; i < sizeof(regs)/sizeof(regs[0]); ++i) {
      ov5642_write_reg(regs[i].a, regs[i].v);
    }
    delay(50);
  }

  // Extended resolution tables adapted from common OV5642 init snippets
  // (conservative subset). These are intentionally small to be safe for
  // different modules; they may need tuning for some boards.
  enum OV5642_RES {
    OV5642_RES_2592x1944 = 0,
    OV5642_RES_1600x1200 = 1,
    OV5642_RES_1280x720  = 2,
    OV5642_RES_640x480   = 3
  };

  static const struct { uint16_t a; uint8_t v; } regs_2592x1944[] = {
    {0x3808, 0x0A}, {0x3809, 0x20}, // 2592
    {0x380a, 0x07}, {0x380b, 0x98}, // 1944
    {0x3a0f, 0x30}, {0x3a10, 0x28}
  };

  static const struct { uint16_t a; uint8_t v; } regs_1600x1200[] = {
    {0x3808, 0x06}, {0x3809, 0x40}, // 1600
    {0x380a, 0x04}, {0x380b, 0xB0}, // 1200
    {0x3a0f, 0x30}
  };

  static const struct { uint16_t a; uint8_t v; } regs_1280x720[] = {
    {0x3808, 0x05}, {0x3809, 0x00}, // 1280
    {0x380a, 0x02}, {0x380b, 0xD0}, // 720
    {0x3a0f, 0x30}
  };

  static const struct { uint16_t a; uint8_t v; } regs_640x480[] = {
    {0x3808, 0x02}, {0x3809, 0x80}, // 640
    {0x380a, 0x01}, {0x380b, 0xE0}, // 480
    {0x3a0f, 0x30}
  };

  static void ov5642_apply_regs(const struct { uint16_t a; uint8_t v; } *tbl, size_t n) {
    for (size_t i = 0; i < n; ++i) {
      ov5642_write_reg(tbl[i].a, tbl[i].v);
    }
  }

  static void ov5642_set_resolution(int res) {
    switch (res) {
      case OV5642_RES_2592x1944:
        ov5642_apply_regs(regs_2592x1944, sizeof(regs_2592x1944)/sizeof(regs_2592x1944[0]));
        break;
      case OV5642_RES_1600x1200:
        ov5642_apply_regs(regs_1600x1200, sizeof(regs_1600x1200)/sizeof(regs_1600x1200[0]));
        break;
      case OV5642_RES_1280x720:
        ov5642_apply_regs(regs_1280x720, sizeof(regs_1280x720)/sizeof(regs_1280x720[0]));
        break;
      case OV5642_RES_640x480:
      default:
        ov5642_apply_regs(regs_640x480, sizeof(regs_640x480)/sizeof(regs_640x480[0]));
        break;
    }
    // small delay to allow settings to take effect
    delay(50);
  }
  #endif
  #endif
#endif
}

void loop() {
  if (Serial.available()) {
    String cmd = readLine();
    if (cmd == "CAPTURE") {
      handleCapture();
    } else if (cmd.startsWith("CAPTURE:")) {
      int idx = cmd.substring(8).toInt();
      captureAndSend(idx);
    } else if (cmd == "SNAP") {
      handleCapture();
    } else if (cmd.startsWith("SNAP:")) {
      int idx = cmd.substring(5).toInt();
      captureAndSend(idx);
    } else if (cmd == "IMU") {
      handleIMU();
    } else if (cmd == "MIC") {
      handleMIC();
    } else if (cmd == "STATUS") {
      handleSTATUS();
    } else if (cmd.startsWith("SETRES:")) {
      String payload = cmd.substring(7);
      handleSetRes(payload);
    } else if (cmd.startsWith("SETQ:")) {
      int q = cmd.substring(5).toInt();
      handleSetQ(q);
    } else if (cmd == "INFO") {
      handleINFO();
    } else if (cmd.startsWith("DISPLAY:")) {
      String payload = cmd.substring(8);
      handleDISPLAY(payload);
    } else if (cmd.startsWith("STREAM:")) {
      // STREAM:n  or STREAM:idx:n
      String args = cmd.substring(7);
      int idx = 0;
      int n = 1;
      int colon = args.indexOf(':');
      if (colon == -1) {
        n = args.toInt();
      } else {
        idx = args.substring(0, colon).toInt();
        n = args.substring(colon + 1).toInt();
      }
      if (n <= 0) n = 1;
      for (int i = 0; i < n; ++i) {
        captureAndSend(idx);
        delay(200);
      }
    } else {
      Serial.println("ERR:UNKNOWN");
    }
  }
}
