/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Short notification cues played through the on-board ES8311 speaker. Tones
// are synthesized at runtime so no audio assets need to live on flash.
typedef enum {
    AUDIO_CUE_CHIME,   // permission prompt arrived: ascending two-tone
    AUDIO_CUE_ACK,     // user approved: bright single tone
    AUDIO_CUE_DENY,    // user denied: descending two-tone
} audio_cue_t;

// Brings up the codec (I2S + ES8311 via the VoCat BSP) and starts a small
// FreeRTOS task that drains a play queue. Safe to call once during app_main.
esp_err_t audio_init(void);

// Fire-and-forget. Drops the cue if the play queue is already full (rapid
// repeated triggers overlap and would garble anyway).
void audio_play(audio_cue_t cue);

#ifdef __cplusplus
}
#endif
