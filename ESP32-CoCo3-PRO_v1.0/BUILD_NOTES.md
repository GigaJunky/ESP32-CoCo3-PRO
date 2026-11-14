# Building ESP32-CoCo3-PRO v1.0 from source: what was needed

Notes from a first build of the v1.0 source on Linux with the Arduino IDE 2.3.x, on an "ESP32-S3 CoCo VGA"
board (ESP32-S3, 8 MB flash, OPI PSRAM; the same board that runs the ESP32-CoCo2 emulator). The firmware
compiled, flashed, booted and was tested on the real board. This file lists everything that had to change
or be supplied, and the traps that cost time, so the next person (and the README) can skip them.

Nothing here has been committed to the repository. The sketch-folder additions below are local.

## Summary

| # | Problem | Fix |
|---|---|---|
| 1 | `PinConfig.h: No such file` (then `Mode.h`, `DMAVideoBuffer.h`, `GfxWrapper.h`, `ESP32S3vga.h`) | Copy the support files of the bitluni ESP32-S3-VGA library into the sketch folder (details in section 1) |
| 2 | `ESP32S3vga.h` not found on Linux | `main.cpp` includes it with a lowercase `v`; the library file is `ESP32S3VGA.h`. Added a one-line `ESP32S3vga.h` |
| 3 | `spiram.h: No such file` | Commented out the include in `DMAVideoBuffer.h` (not used there) |
| 4 | `FreeMonoBoldOblique24pt7b.h` etc. not found | Copied three Adafruit GFX font headers into the sketch folder |
| 5 | ROMs | Copy `coco3.rom`, `disk21.rom` to `/ROMS` on the SD card (section 4) |
| 6 | Boot loop `E BOD: Brownout detector was triggered` | Use **esp32 core 3.2.1** (3.3.12 loops) |
| 7 | `Detected size(8192k) smaller than the size in the binary image header(16384k)` | Set **Flash Size = 8MB** |
| 8 | `Sketch uses ... (122%)` size error in the IDE | Pick a Partition Scheme with a 3 MB app slot (the sketch's own `partitions.csv` still decides the real layout) |

## Environment that worked

- Arduino IDE 2.3.10, **esp32 core 3.2.1** (Espressif, IDF v5.4.2). Also built from the command line with
  `arduino-cli` 1.5.1.
- Libraries: Adafruit GFX Library 1.12.6 and Adafruit BusIO 1.17.4. The USB host library is in `src/EspUsbHost`.
- Board: ESP32S3 Dev Module with:

| Option | Value |
|---|---|
| Flash Size | 8MB (64Mb) |
| PSRAM | OPI PSRAM |
| Flash Mode | **DIO 80MHz** (see "README differences") |
| CPU Frequency | 240MHz |
| USB Mode | Hardware CDC and JTAG |
| USB CDC On Boot | Disabled |
| Arduino Runs On / Events Run On | Core 1 / Core 1 |
| Partition Scheme | any scheme with a 3 MB app slot, e.g. "8M with spiffs (3MB APP/1.5MB SPIFFS)" |

The result: 1,610,638 bytes (48% of the 3.2 MB slot), 100 KB of RAM (30%).

## 1. Missing VGA library files

`VGA.h` includes `PinConfig.h`, `Mode.h` and `DMAVideoBuffer.h`, and `main.cpp` includes `ESP32S3vga.h` and
`<GfxWrapper.h>`. The sketch ships its own patched `VGA.cpp` / `VGA.h` but not the other files of the
bitluni **ESP32-S3-VGA** library, and the stock library lacks the video modes the project needs
(`MODE_320x240x60_4_3`, `MODE_640x240x60_4_3`).

The files were taken from the ESP32-CoCo2 emulator's copy of the library
(`lib/ESP32-S3-VGA-main/src`), which has those modes, and put in the sketch folder:

- `Mode.h`, `Mode.cpp`, `PinConfig.h`, `PinConfig.cpp`, `DMAVideoBuffer.h`, `GfxWrapper.h`
- the library's own `VGA.cpp` / `VGA.h` were **not** copied (the sketch has patched versions; including both
  gives duplicate symbols)
- `ESP32S3vga.h` containing `#include "VGA.h"` (lowercase `v`; the include in `main.cpp` is case-sensitive on Linux)

The README should say exactly which library revision is expected and, ideally, ship `Mode.*`, `PinConfig.*`,
`DMAVideoBuffer.h` and `GfxWrapper.h` with the sketch, or name a library version that already contains the
`_4_3` modes.

## 2. `spiram.h`

`DMAVideoBuffer.h` has `#include "spiram.h"`. That header does not exist in core 3.3.x and nothing in the
file uses it (`Cache_WriteBack_Addr` is declared in the file itself). The include was commented out. It
compiled with the line commented out on both 3.3.12 and 3.2.1. It was not tested whether 3.2.1 needs the
edit.

## 3. Fonts

`main.cpp` includes `FreeMonoBoldOblique24pt7b.h`, `FreeSerif24pt7b.h` and `FreeMono9pt7b.h` with quotes.
They live in the Adafruit GFX library's `Fonts` folder, which is not on the include path for quoted
includes. They were copied into the sketch folder.

## 4. ROMs and fonts: now loaded from the SD card (no copyrighted data in the firmware)

`ROMS_Source.h` and `tools/make_roms_source.py` are gone. Nothing copyrighted is compiled in any more
(`roms.h` / `roms.cpp` replace them). At boot the firmware reads, from `/ROMS/` on the SD card (or the card
root):

| File | Size | Use |
|---|---|---|
| `coco3.rom` | 32,768 | CoCo 3 internal ROM. **Required.** If missing, an on-screen message is shown and the CPU idles. |
| `disk21.rom` | 16,128 (a 16,384 dump is also fine) | Disk BASIC cartridge ROM (`$C000-$FEFF`). Needed for disks. |
| `extbas11.rom`, `bas13.rom`, `disk11.rom` | 8,192 each | CoCo 2 mode only, optional. |
| `font_gimechip.bin`, `cc2_vdg_map.bin` | 1,536 / 12,288 | Optional overrides of the built-in font. |

The text font is a built-in original: a public-domain 5x7 dot-matrix table expanded at boot into
`font_gimeChip` (8x12 cells) and `cc2_VDG_MAP` (VDG text codes). It is not derived from any Tandy/Motorola ROM.
The three unused Adafruit GFX FreeFont headers (GPL-with-font-exception) were removed from the sketch.

The ROM images themselves (Tandy/Microsoft) still have to be supplied by the user from their own machine.

## 5. Brownout boot loop with core 3.3.12

With esp32 core **3.3.12** the board reset continuously:

```
E BOD: Brownout detector was triggered
rst:0x3 (RTC_SW_SYS_RST) ... entry 0x403c8898
E BOD: Brownout detector was triggered
```

The loop happened before any firmware output, and:

- It happened with DIO 80 MHz and with QIO 120 MHz.
- It happened with the brownout register cleared both at the top of `setup()` and from an early constructor
  (so that does not work as a workaround on this core).
- A trivial sketch with the same board settings ran fine, so the cable, supply, board and the core's start-up
  were not the problem.
- Other firmware (ESP32-CoCo2, MicroJS) ran fine on the same board.

Installing **core 3.2.1** (the version the README names) fixed it. The README's warning that other core
versions are untested is accurate. The firmware's own app descriptor confirms the release was built with
Arduino core 3.2.1 (IDF `v5.4.2-25-g858a988d6e`). The root cause on 3.3.x was not tracked down.

## 6. Flash Size and Partition Scheme

- Choosing a 16 MB Flash Size on an 8 MB board gives
  `spi_flash: Detected size(8192k) smaller than the size in the binary image header(16384k)` followed by an
  assert and a reset loop. Use **8MB**.
- After a core change the IDE resets the Tools options to defaults. The default Partition Scheme allows only
  1.25 MB for the app, so the IDE reports "Sketch uses ... (122%)". The actual layout is taken from the sketch's
  `partitions.csv` (two 0x330000 app slots); the menu value only sets the size limit that is checked. Pick a
  scheme with a 3 MB app slot.

## README differences found

- The README's "QIO 120MHz" does not match the released image: the author's `.FLH` has a DIO, 8 MB, 80 MHz
  header. DIO 80 MHz is what was built and tested here. QIO 120 MHz was only tried on core 3.3.12 (which
  looped for other reasons) and not on 3.2.1.
- The sketch path in the README omits the `_v1.0` suffix.
- `disk21` size (see above).
- The `.EFW` in the repository root is a different, older image than `ESP32-CoCo3-PRO_v1.0.FLH` (it has no
  `v1.0 (2026-10-26)` string).

## Flashing and USB

The firmware uses the ESP32-S3's native USB port (GPIO 19/20) as a USB host for keyboards, mice and gamepads.
After start-up the PC no longer sees a serial port, which blocks normal USB flashing. Options:

1. Hold **BOOT**, tap **RESET**, release BOOT, then flash.
2. Update from the SD card: build a `.FLH` with `tools/make_flh.py` from the application `.bin`
   (not the merged image), copy it to `/FLASHNOW` and power-cycle.

A `.FLH` built here with core 3.2.1 passes the tool's checksum check and has the same chip, segment count,
flash header and IDF version as the author's release. It is about 11.9 KB (0.7%) smaller; the likely cause is
different ROM or font data in `ROMS_Source.h`. Installing it from the SD card was not tested.

## What was verified

- Compiled with core 3.2.1 (IDE and `arduino-cli`), flashed over USB, booted on the real board and tested by
  the person doing the build.
- Not verified: the QIO 120 MHz setting on core 3.2.1, the `spiram.h` edit on 3.2.1, installing the generated
  `.FLH` from the SD card.

## Files added to the sketch folder (not committed)

`Mode.h`, `Mode.cpp`, `PinConfig.h`, `PinConfig.cpp`, `DMAVideoBuffer.h` (include edited), `GfxWrapper.h`,
`ESP32S3vga.h`, `FreeMono9pt7b.h`, `FreeMonoBoldOblique24pt7b.h`, `FreeSerif24pt7b.h`, `ROMS_Source.h`
(generated; copyrighted, never commit), plus `tools/make_roms_source.py` and this file.
