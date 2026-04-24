# Desktop Buddy M5Stack Chain DualKey In ESP Launchpad

This guide covers flashing the M5Stack Chain DualKey Desktop Buddy example from
ESP Launchpad.

## What You Need

- An M5Stack Chain DualKey connected to an ESP32-S3 host board
- Claude Desktop for macOS or Windows with Developer Mode enabled

## After Flashing

1. Click `Connect Your Device`.
2. Choose the USB Serial/JTAG port exposed by the board.
3. ESP Launchpad erases flash automatically, then flashes the selected
   firmware.
4. After flashing completes, the serial console starts automatically.
5. On boot, the board advertises as `Claude-XXXX`.

## Pairing And Prompts

- Pair from `Developer -> Open Hardware Buddy...` in Claude Desktop.
- If the desktop asks for pairing, enter the passkey shown on the serial
  console.
- The DualKey LEDs blink during pairing and prompt attention states.
- The left key denies the current prompt.
- The right key accepts the current prompt once.

## Useful Console Commands

```text
status
reply once
reply deny
unpair
```
