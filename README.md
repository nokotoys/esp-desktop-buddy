# ESP Desktop Buddy

An ESP-IDF SDK for building Buddy devices that work with Claude Desktop.

This repository implements the [`protocol`](https://github.com/anthropics/claude-desktop-buddy/blob/main/REFERENCE.md) defined in the upstream [`claude-desktop-buddy`](https://github.com/anthropics/claude-desktop-buddy) repository.

<table>
  <tr>
    <td align="center">
      <a href="docs/static/m5-dualkey-headless.gif">
        <img src="docs/static/m5-dualkey-headless.gif" alt="M5 DualKey Desktop Buddy prompt controls with BLE status LEDs" width="280">
      </a>
      <br>
      <a href="examples/m5_dualkey_headless/README.md"><strong>M5Stack Chain DualKey</strong></a>
    </td>
    <td align="center">
      <a href="docs/static/esp-box-3-demo.png">
        <img src="docs/static/esp-box-3-demo.png" alt="ESP-BOX-3 Desktop Buddy demo showing on-device prompt approval UI" width="280">
      </a>
      <br>
      <a href="examples/esp_box_3_demo/README.md"><strong>ESP32-S3-BOX-3</strong></a>
    </td>
    <td align="center">
      <a href="docs/static/noko-demo.png">
        <img src="docs/static/noko-demo.png" alt="Noko character on the ESP-VoCat round display, an ambient Desktop Buddy" width="280">
      </a>
      <br>
      <a href="examples/noko_demo/README.md"><strong>Noko (ESP-VoCat / EchoEar)</strong></a>
    </td>
  </tr>
</table>

**Noko** is the full-featured demo on Espressif's round-display dev kit:
touchscreen approve/deny, state-driven character GIFs, transcript HUD,
audio chimes, LED alert, BQ27220 battery, and NVS-persisted stats.
See [`examples/noko_demo/`](examples/noko_demo/README.md).

Public components:

- [`esp_desktop_buddy`](components/esp_desktop_buddy/README.md): protocol core
- [`esp_desktop_buddy_transport_ble`](components/esp_desktop_buddy_transport_ble/README.md): NimBLE BLE transport
- [`esp_desktop_buddy_folder_push`](components/esp_desktop_buddy_folder_push/README.md): optional `char_*` folder-push workflow

## Start Here

<a href="https://espressif.github.io/esp-desktop-buddy/">
  <img alt="Try it with ESP Launchpad" src="https://espressif.github.io/esp-launchpad/assets/try_with_launchpad.png" width="220" height="62">
</a>

- [Getting Started](docs/getting-started.md)
- [Examples](examples/README.md)
- [Integration](docs/integration.md)
- [Testing](docs/testing.md)
