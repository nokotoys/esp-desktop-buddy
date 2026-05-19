/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "bsp/esp_vocat.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#include "battery.h"

#define BATTERY_TAG          "battery"
#define BQ27220_ADDR         0x55
#define BQ27220_BUS_HZ       100000
#define BQ27220_XFER_TIMEOUT 100  // ms; gauge responds in <5 ms typically

// Standard register addresses for the BQ27220 variant on the VoCat. All
// values are 16-bit little-endian; Current is signed (two's complement).
// SoC sits at 0x2C on this variant — confirmed by an in-circuit dump
// against RemainingCapacity / FullChargeCapacity at 0x10 / 0x12.
#define BQ27220_REG_VOLTAGE  0x08
#define BQ27220_REG_CURRENT  0x0C
#define BQ27220_REG_SOC      0x2C

static i2c_master_dev_handle_t s_dev;

esp_err_t battery_init(void)
{
    esp_err_t err = bsp_i2c_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(BATTERY_TAG, "bsp_i2c_init failed: %s", esp_err_to_name(err));
        return err;
    }

    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL) {
        ESP_LOGE(BATTERY_TAG, "bus handle null");
        return ESP_FAIL;
    }

    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = BQ27220_ADDR,
        .scl_speed_hz    = BQ27220_BUS_HZ,
    };
    err = i2c_master_bus_add_device(bus, &cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(BATTERY_TAG, "add_device 0x%02X failed: %s",
                 BQ27220_ADDR, esp_err_to_name(err));
        return err;
    }

    // One probe read so a missing/wrong-address gauge is loud at boot.
    battery_reading_t probe = {0};
    if (battery_read(&probe) == ESP_OK && probe.valid) {
        ESP_LOGI(BATTERY_TAG, "BQ27220 ready: %d%% %dmV %dmA",
                 probe.pct, probe.mv, probe.ma);
    } else {
        ESP_LOGW(BATTERY_TAG, "BQ27220 probe read failed");
    }
    return ESP_OK;
}

static esp_err_t bq27220_read_word(uint8_t reg, uint16_t *out)
{
    uint8_t buf[2] = {0};
    esp_err_t err = i2c_master_transmit_receive(s_dev, &reg, 1, buf, 2,
                                                 BQ27220_XFER_TIMEOUT);
    if (err == ESP_OK) {
        *out = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    }
    return err;
}

esp_err_t battery_read(battery_reading_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    if (s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t voltage = 0;
    uint16_t soc     = 0;
    uint16_t current_raw = 0;
    esp_err_t err = bq27220_read_word(BQ27220_REG_VOLTAGE, &voltage);
    if (err == ESP_OK) err = bq27220_read_word(BQ27220_REG_CURRENT, &current_raw);
    if (err == ESP_OK) err = bq27220_read_word(BQ27220_REG_SOC,     &soc);
    if (err != ESP_OK) {
        return err;
    }

    int16_t current = (int16_t)current_raw;   // signed two's complement
    out->pct   = (int)soc;
    out->mv    = (int)voltage;
    out->ma    = -(int)current;   // BQ27220: +charging; REFERENCE.md: -charging
    out->usb   = current > 0;     // any net charge current means a charger is attached
    out->valid = true;
    return ESP_OK;
}
