// BoardCyd.h - pin map and display settings for the ESP32-S3 CYD 2.8in (LCDWiki ES3C28P).
// ESP32-S3R8 (8 MB OPI PSRAM, 16 MB flash), ILI9341V IPS 320x240 on SPI, USB-C on the S3's native USB.
// Pins from the board's documentation (same as the micro-js "cyd-s3" profile). GPIO 26-32 (flash) and
// 35-37 (octal PSRAM) are never usable. Free on this board: 2, 6, 14, 17, 21, 43, 44.
#pragma once

// ---- screen (SPI, write only) ----
#define CYD_TFT_HOST   SPI2_HOST
#define CYD_TFT_MOSI   11
#define CYD_TFT_SCLK   12
#define CYD_TFT_CS     10
#define CYD_TFT_DC     46
#define CYD_TFT_BL     45          // backlight, high = on
#define CYD_TFT_HZ     40000000    // SPI clock: 80000000 gives ~55 fps, 40000000 ~30 fps and draws less peak current
#define CYD_TFT_INVERT 1           // IPS panel: colour inversion on
#define CYD_TFT_MADCTL 0x28        // landscape, BGR. Try 0xE8 (rotated 180 deg) or 0x20 (RGB) if the picture is wrong
#define CYD_TFT_W      320
#define CYD_TFT_H      240

// ---- the emulator's field sync (the VGA board reads its VSYNC pin back; here a timer drives a free pin) ----
#define CYD_VSYNC_GPIO 14
#define CYD_VSYNC_PERIOD_US 16647  // 60.07 Hz, same as the VGA mode
#define CYD_VSYNC_PULSE_US  127

// ---- pins the VGA board uses for other things, moved to GPIOs that connect to nothing ----
// NOT usable: 41, 47, 48 (SD card DAT1 / DAT3 / DAT2: driving DAT3 low makes the card switch to SPI mode, so the
// SD mount fails), 42 (WS2812 LED), 45/46/10-13 (screen), 4/5/7/8/1 (audio codec), 15/16/18 (touch), 9 (battery).
// The LED macros write GPIO_OUT1, so these two must be GPIO 32 or above: 43 and 44 (UART0 pins on the header).
#define CYD_LED_ANODE    43
#define CYD_LED_CATHODE  44
#define CYD_DEBUG_PIN    21
#define CYD_AUDIO_GPIO   17        // PDM audio output: not wired to the codec yet, so the emulator is silent
#define CYD_AUDIO_R_GPIO 21
#define CYD_SERIAL1_TX   2
#define CYD_SERIAL1_RX   6

// ---- sound: ES8311 codec (I2C 0x18 on the touch bus) fed by I2S; the amplifier is enabled by GPIO 1 low ----
#define CYD_I2C_SDA 16
#define CYD_I2C_SCL 15
#define CYD_CODEC_ADDR 0x18
#define CYD_I2S_MCLK 4
#define CYD_I2S_BCK  5
#define CYD_I2S_WS   7
#define CYD_I2S_DOUT 8
#define CYD_AMP_EN   1
#define CYD_CODEC_VOL 0xCF         // ES8311 DAC volume: 0xBF = 0 dB, 0xFF = +32 dB (0.5 dB steps)

// ---- SD card (SDIO, 1-bit) ----
#define CYD_SD_CMD 40
#define CYD_SD_CLK 38
#define CYD_SD_D0  39
#define CYD_SD_KHZ 10000           // SD clock (20000 is the default; lower = smaller current spikes)
