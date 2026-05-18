/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "bsp/esp_vocat.h"
#include "esp_codec_dev.h"
#include "esp_log.h"

#include "audio.h"

#define AUDIO_TAG          "audio"
#define AUDIO_SAMPLE_RATE  16000
#define AUDIO_BITS         16
#define AUDIO_CHANNELS     1
#define AUDIO_VOLUME       70     // 0-100; comfortable for desk-side notification

// Worst-case tone size: 200 ms @ 16 kHz mono 16-bit = 6400 bytes. Static
// allocation in DRAM so writes can DMA without per-play malloc.
#define AUDIO_MAX_TONE_MS  220
#define AUDIO_MAX_SAMPLES  (AUDIO_SAMPLE_RATE / 1000 * AUDIO_MAX_TONE_MS)
static int16_t s_tone_buf[AUDIO_MAX_SAMPLES];

typedef struct {
    uint16_t freq_hz;
    uint16_t duration_ms;
} note_t;

// Each cue is a NULL-terminated sequence of notes. A note with duration_ms
// == 0 ends the sequence. Total duration of any cue must stay short so
// it doesn't block subsequent cues queued behind it.
static const note_t cue_chime[] = {
    {660,  80},
    {990, 120},
    {0,     0},
};
static const note_t cue_ack[] = {
    {1046, 120},
    {0,      0},
};
static const note_t cue_deny[] = {
    {440,  80},
    {220, 160},
    {0,     0},
};

static const note_t *cue_notes(audio_cue_t c)
{
    switch (c) {
    case AUDIO_CUE_CHIME: return cue_chime;
    case AUDIO_CUE_ACK:   return cue_ack;
    case AUDIO_CUE_DENY:  return cue_deny;
    }
    return NULL;
}

static esp_codec_dev_handle_t s_spk;
static QueueHandle_t          s_queue;

// Sine wave with a short linear attack and decay to suppress clicks at the
// note boundaries. Amplitude is well below int16 max so we don't clip.
static void render_tone(uint16_t freq, uint16_t ms, int16_t *buf, size_t n_samples)
{
    const float w = 2.0f * (float)M_PI * (float)freq / (float)AUDIO_SAMPLE_RATE;
    const float amp = 12000.0f;
    const size_t attack = AUDIO_SAMPLE_RATE / 1000 * 5;   // 5 ms
    const size_t decay  = AUDIO_SAMPLE_RATE / 1000 * 15;  // 15 ms
    for (size_t i = 0; i < n_samples; i++) {
        float env = 1.0f;
        if (i < attack) {
            env = (float)i / (float)attack;
        } else if (i + decay > n_samples) {
            env = (float)(n_samples - i) / (float)decay;
        }
        buf[i] = (int16_t)(amp * env * sinf(w * (float)i));
    }
    (void)ms;
}

static void play_cue(audio_cue_t cue)
{
    const note_t *notes = cue_notes(cue);
    if (!notes || !s_spk) {
        return;
    }
    for (const note_t *n = notes; n->duration_ms; n++) {
        size_t samples = (size_t)AUDIO_SAMPLE_RATE * n->duration_ms / 1000;
        if (samples > AUDIO_MAX_SAMPLES) {
            samples = AUDIO_MAX_SAMPLES;
        }
        render_tone(n->freq_hz, n->duration_ms, s_tone_buf, samples);
        esp_codec_dev_write(s_spk, s_tone_buf, samples * sizeof(int16_t));
    }
}

static void audio_task(void *arg)
{
    (void)arg;
    audio_cue_t cue;
    while (true) {
        if (xQueueReceive(s_queue, &cue, portMAX_DELAY) == pdTRUE) {
            play_cue(cue);
        }
    }
}

esp_err_t audio_init(void)
{
    esp_err_t err = bsp_audio_init(NULL);
    if (err != ESP_OK) {
        ESP_LOGE(AUDIO_TAG, "bsp_audio_init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_spk = bsp_audio_codec_speaker_init();
    if (s_spk == NULL) {
        ESP_LOGE(AUDIO_TAG, "speaker codec init failed");
        return ESP_FAIL;
    }

    esp_codec_dev_set_out_vol(s_spk, AUDIO_VOLUME);

    // Open once and keep open — opening per-play adds latency and risks
    // pops. The codec sits quiet when no audio is being written.
    esp_codec_dev_sample_info_t fs = {
        .sample_rate     = AUDIO_SAMPLE_RATE,
        .channel         = AUDIO_CHANNELS,
        .bits_per_sample = AUDIO_BITS,
    };
    err = esp_codec_dev_open(s_spk, &fs);
    if (err != ESP_OK) {
        ESP_LOGE(AUDIO_TAG, "codec open failed: %s", esp_err_to_name(err));
        return err;
    }

    s_queue = xQueueCreate(4, sizeof(audio_cue_t));
    if (s_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(audio_task, "audio", 4096, NULL, 3, NULL) != pdPASS) {
        return ESP_FAIL;
    }
    ESP_LOGI(AUDIO_TAG, "audio ready (volume %d%%)", AUDIO_VOLUME);
    return ESP_OK;
}

void audio_play(audio_cue_t cue)
{
    if (s_queue != NULL) {
        xQueueSend(s_queue, &cue, 0);
    }
}
