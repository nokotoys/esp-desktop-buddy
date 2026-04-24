# Desktop Buddy Generic Headless In ESP Launchpad

This guide covers flashing the generic headless Desktop Buddy example from ESP
Launchpad.

## What You Need

- A BLE-capable ESP32 dev board with a working serial console
- Claude Desktop for macOS or Windows with Developer Mode enabled

## After Flashing

1. Click `Connect Your Device`.
2. Choose the serial port for the board.
3. ESP Launchpad erases flash automatically, then flashes the selected
   firmware.
4. After flashing completes, the serial console starts automatically.
5. On boot, the board advertises as `Claude-XXXX`.

## Pairing And Prompts

- In Claude Desktop, enable developer mode from `Help -> Troubleshooting ->
  Enable Developer Mode`.
- Open `Developer -> Open Hardware Buddy...` and select the device.
- If the desktop asks for pairing, enter the passkey shown on the serial
  console.
- The onboard LED blinks during pairing.
- The LED blinks faster while a permission prompt is waiting for accept or
  deny in the desktop app.

## Useful Console Commands

```text
status
reply once
reply deny
unpair
```
