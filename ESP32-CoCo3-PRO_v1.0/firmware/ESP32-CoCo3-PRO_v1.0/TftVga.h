// TftVga.h - ESP32-CoCo3-PRO on the ESP32-S3 CYD: a drop-in replacement for the VGA class.
// The emulator draws 8-bit pixels (RRRGGGBB) into line buffers exactly as it does for VGA; show() hands the
// finished buffer to a task that converts it to RGB565 and sends it to the ILI9341 over SPI (DMA).
// 640-pixel GIME modes are sent at half width (every second pixel).
#pragma once
#include "PinConfig.h"
#include "Mode.h"
#include <Arduino.h>

class DMAVideoBuffer
{
	public:
	int bufferCount;
	int lines;
	int lineSize;
	bool valid;
	uint8_t *mem[2];

	DMAVideoBuffer(int lines, int lineSize, int clones = 1, bool ring = true, bool psram = true, int bufferCount = 1);
	~DMAVideoBuffer();
	bool isValid() const { return valid; }
	uint8_t *getLineAddr8(int y, int b = 0) { return mem[b] + (size_t)y * lineSize; }
	uint16_t *getLineAddr16(int y, int b = 0) { return (uint16_t *)getLineAddr8(y, b); }
	void flush(int b = 0) {}
	void flush(int b, int y) {}
	void attachBuffer(int b = 0) {}
};

class VGA
{
	public:
	Mode mode;
	int bufferCount;
	int bits;
	PinConfig pins;
	int backBuffer;
	DMAVideoBuffer *dmaBuffer;
	bool usePsram;
	int dmaChannel;
	Mode altMode;
	DMAVideoBuffer *altBuffer;

	VGA();
	~VGA();
	bool init(const PinConfig pins, const Mode mode, int bits);
	bool Reinit(const PinConfig pins, const Mode mode, int bits);
	bool prepareAlternate(const Mode altMode);
	bool hasAlternate(const Mode m) const;
	bool switchToAlternate(void (*waitSync)(void) = 0);
	void deinit();
	bool start();
	bool stop();
	bool show();
	void clear(int rgb = 0);
	void dot(int x, int y, uint8_t r, uint8_t g, uint8_t b);
	void dot(int x, int y, int rgb);
	void dotdit(int x, int y, uint8_t r, uint8_t g, uint8_t b);
	uint8_t getPixel(int x, int y);
	int rgb(uint8_t r, uint8_t g, uint8_t b);
	void drawLineFromMemory8(int x, int y, const uint8_t *memStart);
	void drawLineFromMemory16(int x, int y, const uint8_t *memStart);
	void drawLineFromMemory256(int x, int y, const uint8_t *memStart);
};
