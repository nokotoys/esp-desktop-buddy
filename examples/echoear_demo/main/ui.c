/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "esp_check.h"
#include "esp_log.h"
#include "led_indicator.h"
#include "widgets/gif/lv_gif.h"

#include "app_shared.h"
#include "audio.h"
#include "example_app_helpers.h"

#define BOX_DEMO_UI_STACK 8192
#define BOX_DEMO_UI_POLL_MS 200
#define BOX_DEMO_UI_LOCK_TIMEOUT_MS 1000
#define BOX_DEMO_UI_MARGIN 8
#define BOX_DEMO_UI_CARD_WIDTH 304
#define BOX_DEMO_UI_PROMPT_WIDTH 198
#define BOX_DEMO_UI_PROMPT_HEIGHT 126
#define BOX_DEMO_UI_PROMPT_TEXT_WIDTH 182
#define BOX_DEMO_UI_PROMPT_BODY_HEIGHT 58
#define BOX_DEMO_UI_GIF_WIDTH 98
#define BOX_DEMO_UI_GIF_HEIGHT 126
#define BOX_DEMO_UI_GIF_TEXT_WIDTH 78
#define BOX_DEMO_COLOR_BG 0xF3F4F6
#define BOX_DEMO_COLOR_PANEL 0xFFFFFF
#define BOX_DEMO_COLOR_PANEL_ALT 0xE5E7EB
#define BOX_DEMO_COLOR_TEXT 0x111111
#define BOX_DEMO_COLOR_MUTED 0x4B5563
#define BOX_DEMO_COLOR_ALLOW 0x166534
#define BOX_DEMO_COLOR_DENY 0x991B1B
#define BOX_DEMO_COLOR_APPROVE_BG 0x16A34A
#define BOX_DEMO_COLOR_DENY_BG    0xDC2626
#define BOX_DEMO_COLOR_ON_DARK    0xFFFFFF
#define BOX_DEMO_GIF_PATH_MAX 224

#define BOX_DEMO_FONT_BODY (&lv_font_montserrat_16)

#if CONFIG_LV_FONT_MONTSERRAT_24
#define BOX_DEMO_FONT_PASSKEY (&lv_font_montserrat_24)
#else
#define BOX_DEMO_FONT_PASSKEY BOX_DEMO_FONT_BODY
#endif

#define BOX_DEMO_FONT_ACTION (&lv_font_montserrat_12)

#define BOX_DEMO_FONT_TITLE (&lv_font_montserrat_16)
#define BOX_DEMO_FONT_META (&lv_font_montserrat_12)

// Idle screen layout for the 360x360 round display. Widgets sit on a
// vertical centerline so they stay inside the visible circle. The GIF
// card holds the character (or a placeholder); the passkey label takes
// over the same zone during BLE pairing.
static lv_obj_t *s_title_label;       // y=40, centered, big
static lv_obj_t *s_transport_label;   // y=74, centered, small muted
static lv_obj_t *s_sessions_label;    // y=92, centered, small muted
static lv_obj_t *s_gif_card;          // y=114, 200x120, centered character zone
static lv_obj_t *s_gif_obj;
static lv_obj_t *s_gif_label;
static lv_obj_t *s_passkey_label;     // takes over gif zone during pairing
#define BOX_DEMO_TRANSCRIPT_LINES 3
static lv_obj_t *s_transcript_labels[BOX_DEMO_TRANSCRIPT_LINES];  // y=238/254/270
static lv_obj_t *s_approval_overlay;
static lv_obj_t *s_approval_tool_label;
static lv_obj_t *s_approval_hint_label;
static box_demo_app_t *s_ui_app;
static char s_gif_pack_id[EXAMPLE_CHARPACK_PACK_ID_MAX + 1];
static char s_gif_src[BOX_DEMO_GIF_PATH_MAX];

// On-board LED. Pulses fast when a permission prompt is waiting so the user
// notices even when looking away from the screen.
static led_indicator_handle_t s_led_handle;
static bool s_led_is_attention;

static void box_demo_style_label(lv_obj_t *label, const lv_font_t *font, lv_color_t color)
{
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
}

static void box_demo_style_card(lv_obj_t *obj, uint32_t bg_color, uint32_t border_color)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(bg_color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(border_color), 0);
    lv_obj_set_style_radius(obj, 10, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

static void box_demo_copy_ellipsized(char *dst,
                                     size_t dst_size,
                                     const char *src,
                                     size_t max_chars)
{
    size_t len;
    size_t keep;

    if (dst == NULL || dst_size == 0) {
        return;
    }
    if (src == NULL || src[0] == '\0') {
        dst[0] = '\0';
        return;
    }

    len = strlen(src);
    if (len <= max_chars || max_chars + 1 >= dst_size) {
        strlcpy(dst, src, dst_size);
        return;
    }

    keep = max_chars > 3 ? max_chars - 3 : max_chars;
    if (keep >= dst_size) {
        keep = dst_size - 1;
    }

    memcpy(dst, src, keep);
    if (keep + 3 < dst_size) {
        memcpy(dst + keep, "...", 3);
        dst[keep + 3] = '\0';
    } else {
        dst[keep] = '\0';
    }
}

static bool box_demo_path_exists(const char *path)
{
    struct stat st;

    if (path == NULL) {
        return false;
    }

    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool box_demo_has_gif_suffix(const char *name)
{
    size_t len;

    if (name == NULL) {
        return false;
    }

    len = strlen(name);
    if (len < 4) {
        return false;
    }

    return strcasecmp(name + len - 4, ".gif") == 0;
}

static bool box_demo_pick_gif_asset(const char *pack_id,
                                    char *asset_name,
                                    size_t asset_name_size)
{
    static const char *preferred_assets[] = {
        "idle_0.gif",
        "idle.gif",
        "attention.gif",
        "attention_0.gif",
        "busy.gif",
        "sleep.gif",
    };
    char candidate_path[BOX_DEMO_GIF_PATH_MAX];
    char pack_root[BOX_DEMO_GIF_PATH_MAX];
    const char *packs_root = CONFIG_EXAMPLE_CHARPACK_PACKS_ROOT;
    DIR *dir;
    struct dirent *entry;

    if (pack_id == NULL || pack_id[0] == '\0' || asset_name == NULL || asset_name_size == 0) {
        return false;
    }

    if (snprintf(pack_root,
                 sizeof(pack_root),
                 "%s/%s",
                 packs_root,
                 pack_id) >= (int)sizeof(pack_root)) {
        return false;
    }

    for (size_t i = 0; i < sizeof(preferred_assets) / sizeof(preferred_assets[0]); ++i) {
        if (snprintf(candidate_path,
                     sizeof(candidate_path),
                     "%s/%s",
                     pack_root,
                     preferred_assets[i]) >= (int)sizeof(candidate_path)) {
            continue;
        }
        if (box_demo_path_exists(candidate_path)) {
            strlcpy(asset_name, preferred_assets[i], asset_name_size);
            return true;
        }
    }

    dir = opendir(pack_root);
    if (dir == NULL) {
        return false;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        if (!box_demo_has_gif_suffix(entry->d_name)) {
            continue;
        }
        strlcpy(asset_name, entry->d_name, asset_name_size);
        closedir(dir);
        return true;
    }

    closedir(dir);
    return false;
}

// Buddy state derived from Claude session state. The active pack supplies
// a GIF per state; if the named file isn't in the pack we fall back to
// whatever generic asset the pack provides.
typedef enum {
    BUDDY_STATE_SLEEP,       // not connected over BLE
    BUDDY_STATE_IDLE,        // connected, no urgent activity
    BUDDY_STATE_BUSY,        // many sessions running
    BUDDY_STATE_ATTENTION,   // permission prompt waiting
} buddy_state_t;

static const char *buddy_state_filename(buddy_state_t s)
{
    switch (s) {
    case BUDDY_STATE_SLEEP:     return "sleep.gif";
    case BUDDY_STATE_BUSY:      return "busy.gif";
    case BUDDY_STATE_ATTENTION: return "attention.gif";
    case BUDDY_STATE_IDLE:
    default:                    return "idle_0.gif";
    }
}

// Decide what the buddy should be doing right now. Order matters: attention
// trumps everything (a pending prompt is the most urgent signal), then
// busy/idle when connected, sleep when offline.
static buddy_state_t derive_buddy_state(const example_buddy_state_cache_t *sc,
                                          const esp_desktop_buddy_transport_ble_state_t *tx)
{
    if (sc->has_state && sc->prompt.present) return BUDDY_STATE_ATTENTION;
    if (!tx->connected)                      return BUDDY_STATE_SLEEP;
    if (sc->has_state && sc->running >= 3)   return BUDDY_STATE_BUSY;
    return BUDDY_STATE_IDLE;
}

// Build a LVGL GIF source path "S:packs/<pack>/<filename>" for a specific
// file. Returns false if the file isn't actually on disk (lets callers try
// a fallback).
static bool box_demo_build_named_gif_src(const char *pack_id,
                                          const char *filename,
                                          char *out_src,
                                          size_t out_src_size)
{
    char candidate_path[BOX_DEMO_GIF_PATH_MAX];
    const char *packs_root = CONFIG_EXAMPLE_CHARPACK_PACKS_ROOT;
    const char *mount_point = CONFIG_EXAMPLE_CHARPACK_MOUNT_POINT;
    const char *relative_root = packs_root;
    size_t mount_len;

    if (pack_id == NULL || pack_id[0] == '\0' ||
        filename == NULL || filename[0] == '\0' ||
        out_src == NULL || out_src_size == 0) {
        return false;
    }

    if (snprintf(candidate_path, sizeof(candidate_path), "%s/%s/%s",
                 packs_root, pack_id, filename) >= (int)sizeof(candidate_path)) {
        return false;
    }
    if (!box_demo_path_exists(candidate_path)) {
        return false;
    }

    mount_len = strlen(mount_point);
    if (strncmp(relative_root, mount_point, mount_len) == 0) {
        relative_root += mount_len;
        while (*relative_root == '/') {
            relative_root++;
        }
    }

    return snprintf(out_src, out_src_size, "%c:%s/%s/%s",
                    (char)LV_FS_STDIO_LETTER,
                    relative_root, pack_id, filename) < (int)out_src_size;
}

static bool box_demo_build_gif_src(const char *pack_id,
                                   char *out_src,
                                   size_t out_src_size)
{
    char asset_name[BOX_DEMO_GIF_PATH_MAX];
    const char *packs_root = CONFIG_EXAMPLE_CHARPACK_PACKS_ROOT;
    const char *mount_point = CONFIG_EXAMPLE_CHARPACK_MOUNT_POINT;
    const char *relative_root = packs_root;
    size_t mount_len;

    if (pack_id == NULL || pack_id[0] == '\0' || out_src == NULL || out_src_size == 0) {
        return false;
    }

    if (!box_demo_pick_gif_asset(pack_id, asset_name, sizeof(asset_name))) {
        return false;
    }

    mount_len = strlen(mount_point);
    if (strncmp(relative_root, mount_point, mount_len) == 0) {
        relative_root += mount_len;
        while (*relative_root == '/') {
            relative_root++;
        }
    }

    return snprintf(out_src,
                    out_src_size,
                    "%c:%s/%s/%s",
                    (char)LV_FS_STDIO_LETTER,
                    relative_root,
                    pack_id,
                    asset_name) < (int)out_src_size;
}

static void box_demo_ui_set_gif_placeholder(const char *text)
{
    lv_gif_set_src(s_gif_obj, NULL);
    lv_obj_add_flag(s_gif_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_gif_label, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_gif_label, text);
    s_gif_pack_id[0] = '\0';
    s_gif_src[0] = '\0';
}

static void box_demo_ui_update_gif(bool have_active,
                                   const example_charpack_info_t *active_pack,
                                   buddy_state_t desired_state)
{
#if CONFIG_LV_USE_GIF
    char desired_src[BOX_DEMO_GIF_PATH_MAX];
    bool got = false;

    if (!have_active || active_pack == NULL || active_pack->pack_id[0] == '\0') {
        box_demo_ui_set_gif_placeholder("No active pack");
        return;
    }

    // Prefer the GIF that matches the current buddy state. If that file
    // isn't in the pack, fall back to whatever the generic picker finds
    // so we still show something.
    got = box_demo_build_named_gif_src(active_pack->pack_id,
                                         buddy_state_filename(desired_state),
                                         desired_src, sizeof(desired_src));
    if (!got) {
        got = box_demo_build_gif_src(active_pack->pack_id, desired_src, sizeof(desired_src));
    }
    if (!got) {
        box_demo_ui_set_gif_placeholder("Pack GIF missing");
        return;
    }

    if (strcmp(s_gif_pack_id, active_pack->pack_id) == 0 &&
        strcmp(s_gif_src, desired_src) == 0) {
        return;
    }

    lv_obj_clear_flag(s_gif_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_gif_label, LV_OBJ_FLAG_HIDDEN);
    lv_gif_set_src(s_gif_obj, desired_src);
    if (!lv_gif_is_loaded(s_gif_obj)) {
        box_demo_ui_set_gif_placeholder("GIF load failed");
        return;
    }

    strlcpy(s_gif_pack_id, active_pack->pack_id, sizeof(s_gif_pack_id));
    strlcpy(s_gif_src, desired_src, sizeof(s_gif_src));
    lv_obj_center(s_gif_obj);
#else
    (void)have_active;
    (void)active_pack;
    (void)desired_state;
    box_demo_ui_set_gif_placeholder("GIF support off");
#endif
}

// VoCat's two top capacitive touchpads cross-talk badly (touching one fires
// both). Approve/deny is high-stakes, so we route those decisions through
// the touchscreen overlay only and leave the touchpads unregistered.
static void box_demo_send_decision(box_demo_app_t *app,
                                   esp_desktop_buddy_permission_decision_t decision,
                                   const char *label)
{
    if (app == NULL || app->buddy == NULL) {
        return;
    }
    if (example_reply_current_prompt(app->mutex,
                                       app->buddy,
                                       app->transport,
                                       &app->state_cache,
                                       decision) != ESP_OK) {
        ESP_LOGW("box_demo_ui", "no active prompt for %s tap", label);
    }
}

static void box_demo_approval_approve_cb(lv_event_t *e)
{
    audio_play(AUDIO_CUE_ACK);
    box_demo_send_decision((box_demo_app_t *)lv_event_get_user_data(e),
                           ESP_DESKTOP_BUDDY_PERMISSION_DECISION_ONCE,
                           "approve");
}

static void box_demo_approval_deny_cb(lv_event_t *e)
{
    audio_play(AUDIO_CUE_DENY);
    box_demo_send_decision((box_demo_app_t *)lv_event_get_user_data(e),
                           ESP_DESKTOP_BUDDY_PERMISSION_DECISION_DENY,
                           "deny");
}

static void box_demo_copy_or_default(char *dst,
                                     size_t dst_size,
                                     const char *preferred,
                                     const char *fallback)
{
    if (preferred != NULL && preferred[0] != '\0') {
        strlcpy(dst, preferred, dst_size);
    } else if (fallback != NULL) {
        strlcpy(dst, fallback, dst_size);
    } else if (dst_size > 0) {
        dst[0] = '\0';
    }
}

static void box_demo_ui_refresh(box_demo_app_t *app)
{
    example_buddy_state_cache_t state_cache = {0};
    esp_desktop_buddy_transport_ble_state_t transport = {0};
    example_charpack_info_t active_pack = {0};
    char display_name[BOX_DEMO_NAME_MAX];
    char advertising_name[BOX_DEMO_BLE_NAME_MAX];
    char owner_name[BOX_DEMO_OWNER_MAX];
    bool have_active;
    bool passkey_active;
    bool prompt_active;
    char title_text[BOX_DEMO_NAME_MAX + BOX_DEMO_OWNER_MAX + 20];
    char transport_text[80];
    char sessions_text[64];
    char passkey_text[8];
    lv_color_t transport_color;

    xSemaphoreTake(app->mutex, portMAX_DELAY);
    state_cache = app->state_cache;
    transport = app->transport_state;
    active_pack = app->active_pack;
    have_active = app->have_active_pack;
    strlcpy(display_name, app->display_name, sizeof(display_name));
    strlcpy(advertising_name, app->advertising_name, sizeof(advertising_name));
    strlcpy(owner_name, app->owner_name, sizeof(owner_name));
    xSemaphoreGive(app->mutex);

    prompt_active = state_cache.has_state && state_cache.prompt.present;
    passkey_active = transport.has_passkey;

    // Title: prefer the friendly owner greeting, fall back through display
    // name, owner alone, then a generic label. Keep it short so it fits
    // the circle's chord at y=70 (~280px safe).
    if (owner_name[0] != '\0') {
        char owner_compact[BOX_DEMO_OWNER_MAX];
        box_demo_copy_ellipsized(owner_compact, sizeof(owner_compact), owner_name, 14);
        snprintf(title_text, sizeof(title_text), "Hi %s!", owner_compact);
    } else if (display_name[0] != '\0') {
        box_demo_copy_ellipsized(title_text, sizeof(title_text), display_name, 14);
    } else {
        strlcpy(title_text, "EchoEar", sizeof(title_text));
    }

    // Single-line transport status. Colors: green when ready, muted while
    // negotiating, plain text during pairing so it doesn't compete with
    // the big passkey in the middle.
    if (passkey_active) {
        strlcpy(transport_text, "Pairing — enter code on desktop", sizeof(transport_text));
        transport_color = lv_color_hex(BOX_DEMO_COLOR_TEXT);
    } else if (!transport.connected) {
        if (advertising_name[0] != '\0') {
            snprintf(transport_text, sizeof(transport_text),
                     "Advertising as %s", advertising_name);
        } else {
            strlcpy(transport_text, "Waiting for Claude over BLE", sizeof(transport_text));
        }
        transport_color = lv_color_hex(BOX_DEMO_COLOR_MUTED);
    } else if (prompt_active) {
        strlcpy(transport_text, "Approval needed", sizeof(transport_text));
        transport_color = lv_color_hex(BOX_DEMO_COLOR_ALLOW);
    } else if (transport.tx_ready) {
        strlcpy(transport_text, "Connected and ready", sizeof(transport_text));
        transport_color = lv_color_hex(BOX_DEMO_COLOR_ALLOW);
    } else {
        strlcpy(transport_text, "Securing channel…", sizeof(transport_text));
        transport_color = lv_color_hex(BOX_DEMO_COLOR_MUTED);
    }

    if (state_cache.has_state) {
        if (state_cache.running == 0 && state_cache.waiting == 0) {
            strlcpy(sessions_text, "idle", sizeof(sessions_text));
        } else {
            snprintf(sessions_text, sizeof(sessions_text),
                     "%lu running · %lu waiting",
                     (unsigned long)state_cache.running,
                     (unsigned long)state_cache.waiting);
        }
    } else {
        sessions_text[0] = '\0';
    }

    // LED alert + audio chime: both fire on the false→true edge of "a
    // permission prompt is waiting." LED keeps pulsing until the prompt
    // resolves; chime is a one-shot.
    bool want_attention = prompt_active && !passkey_active;
    if (want_attention != s_led_is_attention) {
        if (want_attention) {
            if (s_led_handle) led_indicator_start(s_led_handle, BSP_LED_BLINK_FAST);
            audio_play(AUDIO_CUE_CHIME);
        } else if (s_led_handle) {
            led_indicator_stop(s_led_handle, BSP_LED_BLINK_FAST);
        }
        s_led_is_attention = want_attention;
    }

    // Passkey takeover: hide the GIF zone (and transcript) and show the big
    // 6-digit code centered. Transcript belongs to the connected experience.
    if (passkey_active) {
        snprintf(passkey_text, sizeof(passkey_text), "%06lu", (unsigned long)transport.passkey);
        lv_label_set_text(s_passkey_label, passkey_text);
        lv_obj_clear_flag(s_passkey_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_gif_card, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < BOX_DEMO_TRANSCRIPT_LINES; i++) {
            lv_obj_add_flag(s_transcript_labels[i], LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_obj_add_flag(s_passkey_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_gif_card, LV_OBJ_FLAG_HIDDEN);
        buddy_state_t bs = derive_buddy_state(&state_cache, &transport);
        box_demo_ui_update_gif(have_active, &active_pack, bs);

        // Transcript: SDK delivers entries newest-first. Display chat-style
        // with newest at the bottom line (brighter). Empty slots clear out
        // when older messages roll off the top.
        for (int i = 0; i < BOX_DEMO_TRANSCRIPT_LINES; i++) {
            lv_obj_clear_flag(s_transcript_labels[i], LV_OBJ_FLAG_HIDDEN);
            // bottom row = newest = entries[0]; row above = entries[1]; ...
            int entry_idx = (BOX_DEMO_TRANSCRIPT_LINES - 1) - i;
            bool is_newest = (i == BOX_DEMO_TRANSCRIPT_LINES - 1);
            if ((size_t)entry_idx < state_cache.entry_count &&
                state_cache.entries[entry_idx][0] != '\0') {
                lv_label_set_text(s_transcript_labels[i], state_cache.entries[entry_idx]);
                lv_obj_set_style_text_color(s_transcript_labels[i],
                                            lv_color_hex(is_newest ? BOX_DEMO_COLOR_TEXT
                                                                    : BOX_DEMO_COLOR_MUTED), 0);
            } else {
                lv_label_set_text(s_transcript_labels[i], "");
            }
        }
    }

    lv_label_set_text(s_title_label, title_text);
    lv_label_set_text(s_transport_label, transport_text);
    lv_obj_set_style_text_color(s_transport_label, transport_color, 0);
    lv_label_set_text(s_sessions_label, sessions_text);

    // Approval overlay take-over. Only when a real permission prompt is
    // active (not during BLE pairing — that uses the passkey card below).
    if (prompt_active && !passkey_active) {
        char tool_text[48];
        char hint_text[128];
        snprintf(tool_text, sizeof(tool_text), "%s?",
                 state_cache.prompt.tool[0] ? state_cache.prompt.tool : "Approval");
        box_demo_copy_or_default(hint_text, sizeof(hint_text),
                                 state_cache.prompt.hint,
                                 state_cache.msg[0] ? state_cache.msg : "");
        lv_label_set_text(s_approval_tool_label, tool_text);
        lv_label_set_text(s_approval_hint_label, hint_text);
        lv_obj_clear_flag(s_approval_overlay, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_approval_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void box_demo_ui_task(void *arg)
{
    box_demo_app_t *app = (box_demo_app_t *)arg;

    while (true) {
        if (bsp_display_lock(BOX_DEMO_UI_LOCK_TIMEOUT_MS)) {
            box_demo_ui_refresh(app);
            bsp_display_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(BOX_DEMO_UI_POLL_MS));
    }
}

esp_err_t box_demo_ui_init(box_demo_app_t *app)
{
    lv_obj_t *scr;

    ESP_RETURN_ON_FALSE(bsp_display_start() != NULL, ESP_FAIL, "box_demo_ui", "display start");
    bsp_display_backlight_on();
    s_ui_app = app;
    // Top capacitive touchpads are intentionally left unregistered: the
    // two pads cross-talk on this board, and approve/deny is high-stakes.
    // Decisions go through the touchscreen overlay instead.

    // LED indicator on GPIO_43. Used for attention alerts.
    led_indicator_handle_t leds[BSP_LED_NUM] = {0};
    int led_cnt = 0;
    if (bsp_led_indicator_create(leds, &led_cnt, BSP_LED_NUM) == ESP_OK && led_cnt > 0) {
        s_led_handle = leds[0];
    } else {
        ESP_LOGW("box_demo_ui", "LED indicator init failed");
        s_led_handle = NULL;
    }
    s_led_is_attention = false;

    if (!bsp_display_lock(BOX_DEMO_UI_LOCK_TIMEOUT_MS)) {
        return ESP_FAIL;
    }

    scr = lv_disp_get_scr_act(NULL);
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(BOX_DEMO_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(BOX_DEMO_COLOR_TEXT), 0);

    // Round-display idle layout: vertical stack down the centerline, sized
    // so each row stays inside the visible circle (radius 180, center 180,180).
    // Shifted up ~18px from the original to use the wasted top-arc pixels;
    // pack-name label removed so transcript breathes.
    // Top: title | transport | sessions text bands
    // Middle: 200x120 character zone (GIF or placeholder); passkey takes
    // over the same zone during pairing.
    // Bottom: 3-line transcript.

    s_title_label = lv_label_create(scr);
    lv_obj_set_pos(s_title_label, 20, 40);
    lv_obj_set_size(s_title_label, 320, 30);
    box_demo_style_label(s_title_label, BOX_DEMO_FONT_PASSKEY, lv_color_hex(BOX_DEMO_COLOR_TEXT));
    lv_obj_set_style_text_align(s_title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_title_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_title_label, "EchoEar");

    s_transport_label = lv_label_create(scr);
    lv_obj_set_pos(s_transport_label, 20, 74);
    lv_obj_set_size(s_transport_label, 320, 16);
    box_demo_style_label(s_transport_label, BOX_DEMO_FONT_META, lv_color_hex(BOX_DEMO_COLOR_MUTED));
    lv_obj_set_style_text_align(s_transport_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_transport_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_transport_label, "Waiting for Claude over BLE");

    s_sessions_label = lv_label_create(scr);
    lv_obj_set_pos(s_sessions_label, 20, 92);
    lv_obj_set_size(s_sessions_label, 320, 16);
    box_demo_style_label(s_sessions_label, BOX_DEMO_FONT_META, lv_color_hex(BOX_DEMO_COLOR_MUTED));
    lv_obj_set_style_text_align(s_sessions_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_sessions_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_sessions_label, "");

    s_gif_card = lv_obj_create(scr);
    lv_obj_set_pos(s_gif_card, 80, 114);
    lv_obj_set_size(s_gif_card, 200, 120);
    box_demo_style_card(s_gif_card, BOX_DEMO_COLOR_PANEL_ALT, BOX_DEMO_COLOR_PANEL_ALT);
    lv_obj_clear_flag(s_gif_card, LV_OBJ_FLAG_SCROLLABLE);

    s_gif_obj = lv_gif_create(s_gif_card);
    lv_gif_set_color_format(s_gif_obj, LV_COLOR_FORMAT_RGB565);
    lv_obj_center(s_gif_obj);
    lv_obj_add_flag(s_gif_obj, LV_OBJ_FLAG_HIDDEN);

    s_gif_label = lv_label_create(s_gif_card);
    lv_obj_set_width(s_gif_label, 180);
    lv_obj_center(s_gif_label);
    lv_label_set_long_mode(s_gif_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(s_gif_label, LV_TEXT_ALIGN_CENTER, 0);
    box_demo_style_label(s_gif_label, BOX_DEMO_FONT_META, lv_color_hex(BOX_DEMO_COLOR_MUTED));
    lv_label_set_text(s_gif_label, "No active pack");

    // Passkey label: hidden by default. During pairing it replaces the GIF
    // card and shows the 6-digit code at title-size font.
    s_passkey_label = lv_label_create(scr);
    lv_obj_set_pos(s_passkey_label, 60, 160);
    lv_obj_set_size(s_passkey_label, 240, 40);
    box_demo_style_label(s_passkey_label, BOX_DEMO_FONT_PASSKEY, lv_color_hex(BOX_DEMO_COLOR_TEXT));
    lv_obj_set_style_text_align(s_passkey_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_passkey_label, "");
    lv_obj_add_flag(s_passkey_label, LV_OBJ_FLAG_HIDDEN);

    // Transcript: last few Claude messages, newest at the bottom (chat-like).
    // Oldest dimmed, newest in body color. Stays inside the circle's chord at
    // these y positions (~290 wide at y=270). Truncate with ... on overflow.
    static const int transcript_y[BOX_DEMO_TRANSCRIPT_LINES] = { 238, 254, 270 };
    for (int i = 0; i < BOX_DEMO_TRANSCRIPT_LINES; i++) {
        s_transcript_labels[i] = lv_label_create(scr);
        lv_obj_set_pos(s_transcript_labels[i], 35, transcript_y[i]);
        lv_obj_set_size(s_transcript_labels[i], 290, 14);
        box_demo_style_label(s_transcript_labels[i], BOX_DEMO_FONT_META,
                             lv_color_hex(BOX_DEMO_COLOR_MUTED));
        lv_obj_set_style_text_align(s_transcript_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(s_transcript_labels[i], LV_LABEL_LONG_DOT);
        lv_label_set_text(s_transcript_labels[i], "");
    }

    // Approval overlay: a full-screen take-over that appears whenever a
    // permission prompt is active. Top half = APPROVE (green), bottom half
    // = DENY (red), with the tool name + hint in the band between them.
    // On the 360x360 round display the rectangular halves naturally clip
    // to half-circles, giving big unambiguous touch targets.
    s_approval_overlay = lv_obj_create(scr);
    lv_obj_set_size(s_approval_overlay, 360, 360);
    lv_obj_set_pos(s_approval_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_approval_overlay, lv_color_hex(BOX_DEMO_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(s_approval_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_approval_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_approval_overlay, 0, 0);
    lv_obj_set_style_radius(s_approval_overlay, 0, 0);
    lv_obj_clear_flag(s_approval_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_approval_overlay, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *approve_btn = lv_obj_create(s_approval_overlay);
    lv_obj_set_size(approve_btn, 360, 160);
    lv_obj_set_pos(approve_btn, 0, 0);
    lv_obj_set_style_bg_color(approve_btn, lv_color_hex(BOX_DEMO_COLOR_APPROVE_BG), 0);
    lv_obj_set_style_bg_opa(approve_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(approve_btn, 0, 0);
    lv_obj_set_style_radius(approve_btn, 0, 0);
    lv_obj_set_style_pad_all(approve_btn, 0, 0);
    lv_obj_clear_flag(approve_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(approve_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(approve_btn, box_demo_approval_approve_cb, LV_EVENT_CLICKED, s_ui_app);

    lv_obj_t *approve_lbl = lv_label_create(approve_btn);
    lv_obj_center(approve_lbl);
    box_demo_style_label(approve_lbl, BOX_DEMO_FONT_PASSKEY, lv_color_hex(BOX_DEMO_COLOR_ON_DARK));
    lv_label_set_text(approve_lbl, "APPROVE");

    lv_obj_t *deny_btn = lv_obj_create(s_approval_overlay);
    lv_obj_set_size(deny_btn, 360, 160);
    lv_obj_set_pos(deny_btn, 0, 200);
    lv_obj_set_style_bg_color(deny_btn, lv_color_hex(BOX_DEMO_COLOR_DENY_BG), 0);
    lv_obj_set_style_bg_opa(deny_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(deny_btn, 0, 0);
    lv_obj_set_style_radius(deny_btn, 0, 0);
    lv_obj_set_style_pad_all(deny_btn, 0, 0);
    lv_obj_clear_flag(deny_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(deny_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(deny_btn, box_demo_approval_deny_cb, LV_EVENT_CLICKED, s_ui_app);

    lv_obj_t *deny_lbl = lv_label_create(deny_btn);
    lv_obj_center(deny_lbl);
    box_demo_style_label(deny_lbl, BOX_DEMO_FONT_PASSKEY, lv_color_hex(BOX_DEMO_COLOR_ON_DARK));
    lv_label_set_text(deny_lbl, "DENY");

    s_approval_tool_label = lv_label_create(s_approval_overlay);
    lv_obj_set_pos(s_approval_tool_label, 0, 166);
    lv_obj_set_size(s_approval_tool_label, 360, 18);
    box_demo_style_label(s_approval_tool_label, BOX_DEMO_FONT_TITLE, lv_color_hex(BOX_DEMO_COLOR_TEXT));
    lv_obj_set_style_text_align(s_approval_tool_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_approval_tool_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_approval_tool_label, "");

    s_approval_hint_label = lv_label_create(s_approval_overlay);
    lv_obj_set_pos(s_approval_hint_label, 30, 184);
    lv_obj_set_size(s_approval_hint_label, 300, 14);
    box_demo_style_label(s_approval_hint_label, BOX_DEMO_FONT_META, lv_color_hex(BOX_DEMO_COLOR_MUTED));
    lv_obj_set_style_text_align(s_approval_hint_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_approval_hint_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_approval_hint_label, "");

    s_gif_pack_id[0] = '\0';
    s_gif_src[0] = '\0';

    bsp_display_unlock();
    return ESP_OK;
}

void box_demo_ui_start(box_demo_app_t *app)
{
    xTaskCreate(box_demo_ui_task, "box_demo_ui", BOX_DEMO_UI_STACK, app, 4, NULL);
}
