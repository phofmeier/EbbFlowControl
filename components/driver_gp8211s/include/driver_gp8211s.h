/**
 * @brief Driver for the GP8211S (I2C to 0V-10V analog driver)
 *
 * @return * void
 */

#ifndef COMPONENTS_DRIVER_GP8211S_INCLUDE_DRIVER_GP8211S
#define COMPONENTS_DRIVER_GP8211S_INCLUDE_DRIVER_GP8211S
#include <stdint.h>

/** @brief Initialize the master I2C bus.
 */
void gp8211s_init_i2c();

/** @brief Initialize the GP8211S device on the I2C bus.
 *
 * This function configures the GP8211S device with the necessary settings
 * to operate correctly. It should be called after initializing the I2C bus.
 */
void gp8211s_init_device();

/** @brief Set the output value of the GP8211S device.
 *
 * @param value The desired output value (15-bit).
 */
void gp8211s_set_output(uint16_t value);

#endif /* COMPONENTS_DRIVER_GP8211S_INCLUDE_DRIVER_GP8211S */
