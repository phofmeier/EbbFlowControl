
#include "driver_gp8211s.h"

#include "driver/i2c_master.h"
#include <stdio.h>

// https://esp32.com/viewtopic.php?t=46042

// https://github.com/DFRobot/DFRobot_GP8XXX/blob/master/DFRobot_GP8XXX.h

i2c_master_bus_handle_t i2c_bus_handle;
i2c_master_dev_handle_t gp8211s_dev_handle;

#define DEVICE_ADDRESS 0x58 // or 0x59 Needs to be checked
#define OUTPUT_RANGE_5V_DATA 0x00
#define OUTPUT_RANGE_10V_DATA 0x11 // or 0x77

#define RESOLUTION_15_BIT 0x7FFF
#define CONFIG_REGISTER 0x01
#define OUTPUT_VALUE_REGISTER 0x02

void gp8211s_init_i2c() {
  // TODO: Implement I2C initialization for GP8211S

  // Create I2C master handle
  //   i2c_master_bus_config_t i2c_master_config = {
  //       .i2c_port = 1,
  //       .sda_io_num = (gpio_num_t)PIN_I2C_SDA,
  //       .scl_io_num = (gpio_num_t)PIN_I2C_SCL,
  //       .clk_source = I2C_CLK_SRC_DEFAULT,
  //       .glitch_ignore_cnt = 7,
  //       .intr_priority = 1,
  //       .trans_queue_depth = 10,
  //       .flags = {.enable_internal_pullup = 1, .allow_pd = 0}};
  //   ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_master_config,
  //   &i2c_master_handle)); if (i2c_master_handle == NULL) {
  //     ESP_LOGE(TAG, "Error creating I2C master handle");
  //   }

#define I2C_MASTER_PORT I2C_NUM_0
#define I2C_MASTER_SCL_IO 4
#define I2C_MASTER_SDA_IO 5

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
      .scl_speed_hz = 100000,
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
