// BoardSelect.h - which hardware the firmware is built for.
// Default: the ESP32-S3 CoCo VGA board (VGA output on the LCD_CAM pins).
// For the ESP32-S3 "CYD" 2.8in (ES3C28P, ILI9341 SPI screen) uncomment the next line, or pass -DBOARD_CYD_S3.
// #define BOARD_CYD_S3
#pragma once
#ifdef BOARD_CYD_S3
#include "BoardCyd.h"
#endif
