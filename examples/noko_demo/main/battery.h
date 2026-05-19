/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Snapshot of battery state, populated by battery_read().
//
// Sign convention matches Claude Hardware Buddy REFERENCE.md: `ma` is
// negative when charging. The BQ27220's native convention is the opposite,
// so the driver inverts the sign before returning.
typedef struct {
    int  pct;    // state of charge, 0..100
    int  mv;     // pack voltage, millivolts
    int  ma;     // pack current, milliamps (negative = charging)
    bool usb;    // true when a charger is supplying current
    bool valid;  // false if the I2C read failed
} battery_reading_t;

// Attach the BQ27220 (I2C address 0x55) to the BSP I2C bus. Idempotent —
// safe to call once at startup.
esp_err_t battery_init(void);

// Read voltage, current and SoC from the gauge in one call. Returns ESP_OK
// on success; `out->valid` will be true. On bus error returns the I2C error
// code with `out->valid` false (other fields zeroed).
esp_err_t battery_read(battery_reading_t *out);

#ifdef __cplusplus
}
#endif
