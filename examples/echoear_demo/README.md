# EchoEar / VoCat Demo

Desktop Buddy demo for the Espressif EchoEar (a.k.a. ESP-VoCat) AI dev kit:
1.85" round QSPI touch display, ES8311 speaker, ES7210 mic array, buttons,
SD card, LED. ESP32-S3.

> **Status: scaffold.** The protocol, BLE transport, and character-pack
> install path are wired up from the upstream `esp_box_3_demo` and should
> work as-is. The on-device UI (`main/ui.c`) was designed for the BOX-3's
> 320×240 rectangular display and will lay out poorly on the round screen
> until it is reworked. See **TODO** below.

## Hardware

- Espressif EchoEar / ESP-VoCat (ESP32-S3, 16 MB flash, octal PSRAM)
- BSP: [`espressif/esp_vocat`](https://components.espressif.com/components/espressif/esp_vocat)

## Build

```bash
cd examples/echoear_demo
idf.py set-target esp32s3
idf.py flash monitor
```

Requires **ESP-IDF ≥ 5.5** (the `esp_vocat` BSP depends on it).

On boot the device advertises as `Claude-XXXX` over BLE (Nordic UART
Service). Pair from **Developer → Open Hardware Buddy…** in Claude Desktop.

## Inputs

The VoCat has on-board buttons handled via the `espressif/button` component.
Approval/deny mapping is inherited from the BOX-3 example for now (`MAIN`
approves, `BOOT` denies) — adjust in `main/app_commands.c` once you've
chosen which physical buttons (or touch zones) to bind.

## Console (USB serial-JTAG)

```text
status
reply once
reply deny
packs
pack use <index>
unpair
```

## TODO before this is usable

- [ ] Redesign `main/ui.c` for the 360×360 round display (CST816S touch)
- [ ] Bind approval/deny to VoCat buttons or touch zones in `app_commands.c`
- [ ] Optional: wire a soft chime on prompt arrival via the ES8311 codec
- [ ] Optional: stage a character pack from
      `../../../claude-desktop-buddy/characters/bufo` into the SPIFFS image
      (see `partitions.csv` → `storage`)
- [ ] Rename internal `box_demo_*` symbols to `echoear_*` for clarity
