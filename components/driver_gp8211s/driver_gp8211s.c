
#include "driver_gp8211s.h"

#include "driver/i2c_master.h"
#include <stdio.h>

i2c_master_bus_handle_t i2c_bus_handle;
i2c_master_dev_handle_t gp8211s_dev_handle;

// Datasheet: https://jlcpcb.com/partdetail/Guestgood-GP8211S_TC50EW/C3152008

#define DEVICE_ADDRESS 0x58 // Datasheet says 0x58
// but maybe 0x59 from some strange internet source.
// https://esp32.com/viewtopic.php?t=46042

// Following values are from the DFRobot Library, but datasheet seems to suggest
// different values. Needs proper testing
// https://github.com/DFRobot/DFRobot_GP8XXX/blob/master/DFRobot_GP8XXX.h
#define OUTPUT_RANGE_5V_DATA 0x00  // Datasheet says 0x55
#define OUTPUT_RANGE_10V_DATA 0x11 // Datasheet says 0x77

#define CONFIG_REGISTER 0x01       // Register address to set output range
#define OUTPUT_VALUE_REGISTER 0x02 // Register address to set output value
#define DEVICE_SCL_SPEED_HZ 100000 // Datasheet says max 400kHz

// Resolution is 15 bit. Bit 16 needs to be 0

#define I2C_MASTER_PORT I2C_NUM_0
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21

void gp8211s_init_i2c() {
  i2c_master_bus_config_t i2c_mst_config = {
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .i2c_port = I2C_MASTER_PORT,
      .scl_io_num = I2C_MASTER_SCL_IO,
      .sda_io_num = I2C_MASTER_SDA_IO,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };

  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle));
}

void gp8211s_init_device() {
  i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = DEVICE_ADDRESS,
      .scl_speed_hz = DEVICE_SCL_SPEED_HZ,
      .scl_wait_us = 0,
  };

  ESP_ERROR_CHECK(
      i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &gp8211s_dev_handle));
  // Configure device
  // Set to 10V output range
  const uint8_t config_buffer[2] = {CONFIG_REGISTER, OUTPUT_RANGE_10V_DATA};
  i2c_master_transmit(gp8211s_dev_handle, config_buffer, sizeof(config_buffer),
                      10000);
}

void gp8211s_set_output(uint16_t value) {
  if (value > 0x7FFF) {
    value = 0x7FFF; // Clamp to max 15-bit value
  }
  // Maybe shift value one bit left

  uint8_t send_buffer[3] = {OUTPUT_VALUE_REGISTER, (value & 0xff),
                            (value >> 8)};

  i2c_master_transmit(gp8211s_dev_handle, send_buffer, sizeof(send_buffer),
                      10000);
}
