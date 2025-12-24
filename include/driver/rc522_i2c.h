#pragma once

#include <driver/i2c_master.h>
#include <driver/gpio.h>
#include <stdbool.h>
#include "rc522_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    i2c_port_t port;
    gpio_num_t sda_io_num;
    gpio_num_t scl_io_num;
    i2c_clock_source_t clk_source;
    bool enable_internal_pullup;
    uint32_t scl_speed_hz;
    uint8_t device_address;
    uint32_t rw_timeout_ms;

    /**
     * GPIO number of the RC522 RST pin.
     * Set to -1 if the RST pin is not connected.
     */
    gpio_num_t rst_io_num;

    /**
     * Internal: I2C bus/device handles and ownership.
     */
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    bool bus_created;
} rc522_i2c_config_t;

esp_err_t rc522_i2c_create(const rc522_i2c_config_t *config, rc522_driver_handle_t *driver);

#ifdef __cplusplus
}
#endif
