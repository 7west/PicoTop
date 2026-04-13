# PicoTop

PicoTop is a small computer built around the RP2350. It outputs a black and white terminal over HDMI and takes input from a USB keyboard. It runs its own keyboard-only OS with a shell, text editor, and a small set of programs.

## Features

- **HDMI out** — 640x480 terminal display over DVI
- **USB keyboard** — standard USB-A keyboard input
- **Rechargeable** — onboard LiPo battery with USB-C charging; battery level visible in the top bar
- **SD card file system** — FAT16 on a micro SD card (FAT16 only; cards must be 2GB or smaller. I have only tested 2GB and 512MB)
- **RTC** — onboard real-time clock keeps time across power cycles

## Programs

| Program | Description |
|---|---|
| `shell` | Command-line interface; entry point for all programs and commands |
| `uEdit` | Text editor |
| `calc` | Calculator Workbook |
| `hangman` | Hangman game |

### Shell Commands

`history`, `debug-log`, `timeset`, `battv`, `beep`, `clear`, `ls`, `pwd`, `cd`, `mkdir`, `rmdir`, `rm`, `cp`, `mv`, `sd-format`, `sd-mount`, `sd-unmount`, and GPIO interaction commands. All commands have a `-h` option to describe what they do and how to use them.

## Notes

This repository is not used for active development, so everything is in one commit. It will be updated again at the next major milestone.
