/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdio.h>

#include "bsp/esp-bsp.h"
#include "esp_desktop_buddy/esp_desktop_buddy.h"
#include "esp_desktop_buddy/folder_push.h"
#include "esp_desktop_buddy/transport_ble.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "example_app_helpers.h"
#include "example_charpack.h"

#define NOKO_NAME_MAX 32
#define NOKO_BLE_NAME_MAX 32
#define NOKO_OWNER_MAX 32
#define NOKO_STATUS_MAX 64

typedef struct {
    SemaphoreHandle_t mutex;
    esp_desktop_buddy_t *buddy;
    esp_desktop_buddy_transport_ble_t *transport;
    esp_desktop_buddy_folder_push_t *folder_push;
    example_charpack_t *charpack;
    example_buddy_state_cache_t state_cache;
    char display_name[NOKO_NAME_MAX];
    char advertising_name[NOKO_BLE_NAME_MAX];
    char owner_name[NOKO_OWNER_MAX];
    char pack_status[NOKO_STATUS_MAX];
    bool have_active_pack;
    example_charpack_info_t active_pack;
    esp_desktop_buddy_transport_ble_state_t transport_state;
    uint32_t approval_count;
    uint32_t denial_count;
    uint32_t nap_seconds;
    example_progress_state_t progress;
    int32_t tz_offset_seconds;
} noko_app_t;

void noko_app_init(noko_app_t *app);
void noko_stats_load(noko_app_t *app);

esp_desktop_buddy_status_reply_t noko_status_handler(void *ctx, esp_desktop_buddy_t *buddy);
esp_desktop_buddy_command_result_t noko_name_handler(void *ctx, esp_desktop_buddy_t *buddy, const char *name);
esp_desktop_buddy_command_result_t noko_owner_handler(void *ctx, esp_desktop_buddy_t *buddy, const char *name);
esp_desktop_buddy_command_result_t noko_unpair_handler(void *ctx, esp_desktop_buddy_t *buddy);
void noko_buddy_event(void *ctx, const esp_desktop_buddy_event_t *event);
void noko_transport_event(void *ctx, const esp_desktop_buddy_transport_ble_event_t *event);
void noko_charpack_event(void *ctx, const example_charpack_event_t *event);

void noko_print_state(noko_app_t *app, FILE *out);

esp_err_t noko_ui_init(noko_app_t *app);
void noko_ui_start(noko_app_t *app);
void noko_charpack_console_start(noko_app_t *app);
