#include <string.h>
#include "rc522_helpers_internal.h"
#include "rc522_types_internal.h"
#include "rc522_driver_internal.h"
#include "driver/rc522_i2c.h"

RC522_LOG_DEFINE_BASE();

static esp_err_t rc522_i2c_install(const rc522_driver_handle_t driver)
{
    RC522_CHECK(driver == NULL);
    RC522_CHECK(driver->config == NULL);

    rc522_i2c_config_t *conf = (rc522_i2c_config_t *)(driver->config);

    if (conf->bus_handle == NULL) {
        esp_err_t err = i2c_master_get_bus_handle(conf->port, &conf->bus_handle);
        if (err != ESP_OK) {
            if (conf->sda_io_num < 0 || conf->scl_io_num < 0) {
                return ESP_ERR_INVALID_ARG;
            }

            i2c_master_bus_config_t bus_cfg = {
                .clk_source = conf->clk_source ? conf->clk_source : I2C_CLK_SRC_DEFAULT,
                .i2c_port = conf->port,
                .sda_io_num = conf->sda_io_num,
                .scl_io_num = conf->scl_io_num,
                .glitch_ignore_cnt = 7,
                .flags.enable_internal_pullup = conf->enable_internal_pullup,
            };

            RC522_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &conf->bus_handle));
            conf->bus_created = true;
        }
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = conf->device_address,
        .scl_speed_hz = conf->scl_speed_hz ? conf->scl_speed_hz : 100000,
    };

    esp_err_t add_err = i2c_master_bus_add_device(conf->bus_handle, &dev_cfg, &conf->dev_handle);
    if (add_err != ESP_OK) {
        if (conf->bus_created && conf->bus_handle) {
            i2c_del_master_bus(conf->bus_handle);
            conf->bus_handle = NULL;
            conf->bus_created = false;
        }
        return add_err;
    }

    if (conf->rst_io_num > GPIO_NUM_NC) {
        RC522_RETURN_ON_ERROR(rc522_driver_init_rst_pin(conf->rst_io_num));
    }

    return ESP_OK;
}

static esp_err_t rc522_i2c_send(const rc522_driver_handle_t driver, uint8_t address, const rc522_bytes_t *bytes)
{
    RC522_CHECK(driver == NULL);
    RC522_CHECK(driver->config == NULL);
    RC522_CHECK_BYTES(bytes);

    // FIXME: Find a way to send [address + buffer]
    //        without need for second buffer
    uint8_t buffer2[64];

    buffer2[0] = address;
    memcpy(buffer2 + 1, bytes->ptr, bytes->length);

    rc522_i2c_config_t *conf = (rc522_i2c_config_t *)(driver->config);
    if (conf->dev_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    RC522_RETURN_ON_ERROR(i2c_master_transmit(conf->dev_handle,
        buffer2,
        (bytes->length + 1),
        conf->rw_timeout_ms));

    return ESP_OK;
}

static esp_err_t rc522_i2c_receive(const rc522_driver_handle_t driver, uint8_t address, rc522_bytes_t *bytes)
{
    RC522_CHECK(driver == NULL);
    RC522_CHECK(driver->config == NULL);
    RC522_CHECK_BYTES(bytes);

    rc522_i2c_config_t *conf = (rc522_i2c_config_t *)(driver->config);
    if (conf->dev_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    RC522_RETURN_ON_ERROR(i2c_master_transmit_receive(conf->dev_handle,
        &address,
        1,
        bytes->ptr,
        bytes->length,
        conf->rw_timeout_ms));

    return ESP_OK;
}

static esp_err_t rc522_i2c_reset(const rc522_driver_handle_t driver)
{
    RC522_CHECK(driver == NULL);
    RC522_CHECK(driver->config == NULL);

    rc522_i2c_config_t *conf = (rc522_i2c_config_t *)(driver->config);

    if (conf->rst_io_num < 0) {
        return RC522_ERR_RST_PIN_UNUSED;
    }

    RC522_RETURN_ON_ERROR(gpio_set_level(conf->rst_io_num, RC522_DRIVER_HARD_RST_PIN_PWR_DOWN_LEVEL));
    rc522_delay_ms(RC522_DRIVER_HARD_RST_PULSE_DURATION_MS);
    RC522_RETURN_ON_ERROR(gpio_set_level(conf->rst_io_num, !RC522_DRIVER_HARD_RST_PIN_PWR_DOWN_LEVEL));
    rc522_delay_ms(RC522_DRIVER_HARD_RST_PULSE_DURATION_MS);

    return ESP_OK;
}

static esp_err_t rc522_i2c_uninstall(const rc522_driver_handle_t driver)
{
    RC522_CHECK(driver == NULL);
    RC522_CHECK(driver->config == NULL);

    rc522_i2c_config_t *conf = (rc522_i2c_config_t *)(driver->config);

    if (conf->dev_handle) {
        RC522_RETURN_ON_ERROR(i2c_master_bus_rm_device(conf->dev_handle));
        conf->dev_handle = NULL;
    }

    if (conf->bus_created && conf->bus_handle) {
        RC522_RETURN_ON_ERROR(i2c_del_master_bus(conf->bus_handle));
        conf->bus_handle = NULL;
        conf->bus_created = false;
    }

    return ESP_OK;
}

esp_err_t rc522_i2c_create(const rc522_i2c_config_t *config, rc522_driver_handle_t *driver)
{
    RC522_CHECK(config == NULL);
    RC522_CHECK(driver == NULL);

    RC522_RETURN_ON_ERROR(rc522_driver_create(config, sizeof(rc522_i2c_config_t), driver));

    (*driver)->install = rc522_i2c_install;
    (*driver)->send = rc522_i2c_send;
    (*driver)->receive = rc522_i2c_receive;
    (*driver)->reset = rc522_i2c_reset;
    (*driver)->uninstall = rc522_i2c_uninstall;

    return ESP_OK;
}
