# Desktop Buddy ESP-BOX-3 Demo In ESP Launchpad

This guide covers flashing the ESP-BOX-3 Desktop Buddy demo from ESP
Launchpad.

## What You Need

- An ESP32-S3-BOX-3
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
- The ESP-BOX-3 display shows the pairing passkey when one is active.
- Prompt cards are rendered on the display when the desktop bridge asks for
  approval.
- Use `MAIN` to approve the current prompt once.
- Use `BOOT` to deny the current prompt.

## Useful Console Commands

```text
status
reply once
reply deny
packs
pack use <index>
unpair
```
