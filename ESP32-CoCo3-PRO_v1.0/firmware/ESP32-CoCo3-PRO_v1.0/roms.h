// roms.h - ROM and font data, loaded at run time (nothing copyrighted is compiled into the firmware).
//
// ROM images are read from the SD card at boot:  /ROMS/<name>  (or the card root if /ROMS is absent)
//   coco3.rom      32768 bytes  CoCo 3 internal ROM (Super Extended Color BASIC, $8000-$FFFF)   REQUIRED
//   disk21.rom     16128+ bytes Disk BASIC cartridge ROM ($C000-$FEFF)                          needed for disks
//   extbas11.rom    8192 bytes  CoCo 2 Extended Color BASIC 1.1   (optional, CoCo 2 mode only)
//   bas13.rom       8192 bytes  CoCo 2 Color BASIC 1.3            (optional, CoCo 2 mode only)
//   disk11.rom      8192 bytes  Disk Extended Color BASIC 1.1     (optional, CoCo 2 mode only)
// Optional font overrides (the firmware has its own free built-in font, see roms.cpp):
//   font_gimechip.bin  1536 bytes   128 chars x 12 lines, MSB = leftmost pixel
//   cc2_vdg_map.bin   12288 bytes   128 chars x 96 bytes (0x1C = background pixel, other = glyph pixel)
#pragma once
#include <stdint.h>

#define ROM_COCO3_SIZE     32768
#define ROM_DISK21_SIZE    16128
#define ROM_8K_SIZE         8192
#define FONT_GIME_SIZE      1536
#define FONT_VDG_SIZE      12288

extern uint8_t *coco3;       // ROM_COCO3_SIZE   (PSRAM)
extern uint8_t *extbas11;    // ROM_8K_SIZE
extern uint8_t *bas13;       // ROM_8K_SIZE
extern uint8_t *disk11;      // ROM_8K_SIZE
extern uint8_t *disk21;      // ROM_DISK21_SIZE
extern uint8_t font_gimeChip[FONT_GIME_SIZE];
extern uint8_t cc2_VDG_MAP[FONT_VDG_SIZE];

#define ROMS_MISSING_COCO3   0x01
#define ROMS_MISSING_DISK21  0x02

void Roms_InitFonts(void);   // build the free built-in fonts; call first thing in setup()
unsigned Roms_Load(void);    // after the SD card is mounted; returns a ROMS_MISSING_* mask (0 = all required ROMs found)
