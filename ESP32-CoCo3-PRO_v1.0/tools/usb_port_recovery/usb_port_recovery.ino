// usb_port_recovery - minimal ESP32-S3 sketch for ESP32-CoCo3-PRO boards.
// Never starts the USB host, so the native USB port stays a USB-Serial/JTAG device the PC can see.
// Install it from the SD card as a .FLH (see tools/make_flh.py), power-cycle, connect the PC to the
// native micro-USB (no hub), then flash the real firmware over USB as usual.
// Build: esp32 core 3.2.1, ESP32S3 Dev Module, 8MB flash, OPI PSRAM, DIO, USB Mode = Hardware CDC and JTAG.
#include "esp_ota_ops.h"

// The CoCo3-PRO updater leaves a new image "pending verify" and rolls back if it is not confirmed.
// Confirm immediately so this image stays installed across resets and power cycles.
extern "C" bool verifyRollbackLater() { return true; }

void setup() {
  esp_ota_mark_app_valid_cancel_rollback();
  Serial.begin(115200);
}
void loop() {
  Serial.println("USB port recovery firmware - port is free, flash now");
  delay(1000);
}
