# Noko — Desktop Buddy on ESP-VoCat

A Claude Desktop Buddy demo that turns the Espressif ESP-VoCat (a.k.a.
EchoEar) into "Noko" — an ambient character that lives off your Claude
sessions. Sleeps when no one is logged in, looks busy while sessions run,
glows red and chimes when a permission prompt is waiting, and lets you
approve or deny right from the touchscreen.

## Hardware

- **Espressif ESP-VoCat** (ESP32-S3, 16 MB flash, octal PSRAM)
- 1.85" round QSPI touch display (ST77916 + CST816S)
- ES8311 speaker, ES7210 mic array
- BQ27220 battery fuel gauge over I2C
- GPIO 43 LED, two capacitive touchpads (unused — they cross-talk)
- BSP: [`espressif/esp_vocat`](https://components.espressif.com/components/espressif/esp_vocat)

## Build

```bash
cd examples/noko_demo
idf.py set-target esp32s3
idf.py flash monitor
```

Requires **ESP-IDF ≥ 5.5**.

On boot the device advertises as `Claude-XXXX` over BLE (Nordic UART
Service). Pair from **Developer → Open Hardware Buddy…** in Claude Desktop.

## Features

- **Touchscreen approval overlay** — full-screen split: green ✓ on top to
  approve, red ✗ on bottom to deny. The two top touchpads are *not* used
  for decisions because they cross-talk badly.
- **Character-pack GIFs** — drop a folder onto the Hardware Buddy window
  to push it over BLE. Generate a custom pack with `tools/make_noko.py`
  in the upstream `claude-desktop-buddy` repo.
- **State-driven character** — sleep / idle / busy / attention GIFs picked
  from the active pack based on Claude session state.
- **Transcript HUD** — last three Claude messages scroll under the
  character, newest line in body color.
- **Stats page** — swipe up from idle to see approve/deny counts, level
  (token-based), and a mood word derived from your average response time.
  Counters persist across reboot via NVS.
- **LED alert** — pulses fast while a permission prompt is waiting.
- **Audio cues** via the ES8311 speaker — chime on prompt arrival, ack
  tone on approve, descending tone on deny. Tones are synthesized at
  runtime; no audio assets ship on flash.
- **Battery panel** — real voltage / current / SoC read from the BQ27220.

## Console (USB serial-JTAG)

```text
status
reply once
reply deny
packs
pack use <index>
unpair
```

## Possible next steps

- Settings menu (sound on/off, LED on/off, factory reset, delete pack)
- Owner / device name keypad on the touchscreen
- TTS announcement of pending prompts via the on-board speaker
- Wake-word voice approval using the ES7210 mic array
- Boot splash (greeting on power-on)
