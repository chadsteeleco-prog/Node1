// sensor_ov5642.h
// OV5642 register initialization sequences for ArduCAM MEGA 5MP modules.
//
// NOTE: These tables are adapted from common ArduCAM OV5642 example
// sequences and community-maintained snippets. Use them as a starting
// point. Depending on your module revision you may need to tweak registers.
//
// Attribution: ArduCAM example init sequences (see https://www.arducam.com/)
// Please verify license compatibility for redistribution in your project.

#ifndef SENSOR_OV5642_H
#define SENSOR_OV5642_H

#include <Arduino.h>
#include <Wire.h>

#define OV5642_ADDR (0x78 >> 1)

static void ov5642_write_reg_i2c(uint16_t reg, uint8_t val) {
  Wire.beginTransmission(OV5642_ADDR);
  Wire.write((reg >> 8) & 0xFF);
  Wire.write(reg & 0xFF);
  Wire.write(val);
  Wire.endTransmission();
  delay(2);
}

// Full initialization table (conservative but comprehensive)
static const struct { uint16_t reg; uint8_t val; } ov5642_init_regs[] = {
  {0x3008, 0x42},
  {0x3008, 0x80},
  {0x3103, 0x11},
  {0x3000, 0x00},
  {0x3002, 0x1c},
  {0x3017, 0xff}, {0x3018, 0xff},
  {0x3034, 0x1a}, {0x3035, 0x11},
  {0x3036, 0x54}, {0x3037, 0x12},
  {0x3108, 0x01},
  {0x3630, 0x2e}, {0x3632, 0xe2}, {0x3634, 0x20},
  {0x3620, 0x33}, {0x3622, 0x01},
  {0x3c01, 0x80}, {0x3c04, 0x28},
  {0x5025, 0x80},
  {0x3a00, 0x78}, {0x3a1a, 0x06}, {0x3a13, 0x30},
  {0x3503, 0x07}, {0x3501, 0x73}, {0x3502, 0x00},
  {0x3a08, 0x01}, {0x3a09, 0x27},
  {0x3a0e, 0x00}, {0x3a0d, 0x00},
  {0x3708, 0x64}, {0x3709, 0x52}, {0x370c, 0x00},
  {0x3800, 0x00}, {0x3801, 0x00}, {0x3802, 0x00}, {0x3803, 0x00},
  {0x3804, 0x0a}, {0x3805, 0x1f}, {0x3806, 0x07}, {0x3807, 0x9f},
  {0x3808, 0x0a}, {0x3809, 0x20}, {0x380a, 0x07}, {0x380b, 0x98},
  {0x3810, 0x00}, {0x3811, 0x10},
  {0x3814, 0x31}, {0x3815, 0x31},
  {0x3820, 0x41}, {0x3821, 0x07},
  {0x3a0f, 0x30}, {0x3a10, 0x28},
  {0x4001, 0x02}, {0x4004, 0x02},
  {0x4300, 0x30},
  {0x460b, 0x35}, {0x460c, 0x22},
  {0x4808, 0x25},
  {0x5000, 0x06}, {0x5001, 0x00},
  {0x5002, 0x30}, {0x5003, 0x08},
  {0x5020, 0x04},
  {0x3008, 0x02}
};

// Resolution-specific tables (detailed sequences)
static const struct { uint16_t reg; uint8_t val; } ov5642_2592x1944_regs[] = {
  {0x3035, 0x21}, {0x3036, 0x69},
  {0x3c07, 0x07}, {0x3c08, 0x08},
  {0x3c09, 0x10},
  {0x3818, 0xa8}, {0x3819, 0x00},
  {0x3820, 0x00}, {0x3821, 0x06},
  {0x4300, 0x30}
};

static const struct { uint16_t reg; uint8_t val; } ov5642_1600x1200_regs[] = {
  {0x3035, 0x11}, {0x3036, 0x49},
  {0x3c07, 0x07}, {0x3c08, 0x08},
  {0x3c09, 0x10},
  {0x3818, 0xa8}, {0x3819, 0x00},
  {0x3820, 0x00}, {0x3821, 0x06}
};

static const struct { uint16_t reg; uint8_t val; } ov5642_1280x720_regs[] = {
  {0x3808, 0x05}, {0x3809, 0x00}, {0x380a, 0x02}, {0x380b, 0xd0},
  {0x3800, 0x00}, {0x3801, 0x00},
  {0x3810, 0x00}
};

static const struct { uint16_t reg; uint8_t val; } ov5642_640x480_regs[] = {
  {0x3808, 0x02}, {0x3809, 0x80}, {0x380a, 0x01}, {0x380b, 0xe0},
  {0x3810, 0x00}
};

static void OV5642_apply_table(const struct { uint16_t reg; uint8_t val; } *tbl, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    ov5642_write_reg_i2c(tbl[i].reg, tbl[i].val);
  }
}

static void OV5642_full_init() {
  OV5642_apply_table(ov5642_init_regs, sizeof(ov5642_init_regs)/sizeof(ov5642_init_regs[0]));
}

static void OV5642_set_JPEG_size(const char *name) {
  if (strcmp(name, "2592x1944") == 0) {
    OV5642_apply_table(ov5642_2592x1944_regs, sizeof(ov5642_2592x1944_regs)/sizeof(ov5642_2592x1944_regs[0]));
  } else if (strcmp(name, "1600x1200") == 0) {
    OV5642_apply_table(ov5642_1600x1200_regs, sizeof(ov5642_1600x1200_regs)/sizeof(ov5642_1600x1200_regs[0]));
  } else if (strcmp(name, "1280x720") == 0) {
    OV5642_apply_table(ov5642_1280x720_regs, sizeof(ov5642_1280x720_regs)/sizeof(ov5642_1280x720_regs[0]));
  } else {
    OV5642_apply_table(ov5642_640x480_regs, sizeof(ov5642_640x480_regs)/sizeof(ov5642_640x480_regs[0]));
  }
}

static void OV5642_set_JPEG_quality(int q) {
  if (q < 0) q = 0;
  if (q > 100) q = 100;
  // Heuristic mapping: map quality [0,100] -> encoder scale bytes.
  // Different modules may respond differently; these are conservative
  // changes that adjust internal encoder parameters without drastic
  // sensor reconfiguration. Users with specific modules should tune
  // these register values for best results.
  uint8_t scaled = (uint8_t)((q * 255) / 100);

  // Example registers that influence JPEG/ISP compression behavior on
  // some OV5642 variants. These writes are conservative; if you enable
  // OV5642_CONFIG and call this on a real module, monitor results and
  // tune as needed.
  ov5642_write_reg_i2c(0x4407, scaled); // encoder scale heuristic
  ov5642_write_reg_i2c(0x4408, 0x00 + (scaled >> 2));
  ov5642_write_reg_i2c(0x4409, 0x00 + (scaled >> 4));
  delay(20);
}

#endif // SENSOR_OV5642_H
