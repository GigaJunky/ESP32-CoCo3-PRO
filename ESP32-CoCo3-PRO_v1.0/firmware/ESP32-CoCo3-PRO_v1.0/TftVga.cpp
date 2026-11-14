// TftVga.cpp - ESP32-S3 CYD back end for the VGA interface (see TftVga.h).
#include "BoardSelect.h"
#ifdef BOARD_CYD_S3
#include "TftVga.h"
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <driver/gptimer.h>
#include <esp_lcd_panel_io.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <soc/gpio_reg.h>

// ---------------------------------------------------------------------------
// frame buffers
// ---------------------------------------------------------------------------
DMAVideoBuffer::DMAVideoBuffer(int lines, int lineSize, int clones, bool ring, bool psram, int bufferCount)
{
	this->lines = lines;
	this->lineSize = lineSize;
	this->bufferCount = bufferCount > 2 ? 2 : (bufferCount < 1 ? 1 : bufferCount);
	valid = false;
	mem[0] = mem[1] = nullptr;
	const size_t n = (size_t)lines * lineSize;
	for (int b = 0; b < this->bufferCount; b++)
	{
		mem[b] = (uint8_t *)heap_caps_calloc(1, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
		if (!mem[b]) mem[b] = (uint8_t *)heap_caps_calloc(1, n, MALLOC_CAP_8BIT);
		if (!mem[b]) return;
	}
	valid = true;
}

DMAVideoBuffer::~DMAVideoBuffer()
{
	for (int b = 0; b < 2; b++) if (mem[b]) free(mem[b]);
}

// ---------------------------------------------------------------------------
// screen
// ---------------------------------------------------------------------------
static const int CHUNK_LINES = 16;
static const int CHUNK_BYTES = CYD_TFT_W * CHUNK_LINES * 2;

static esp_lcd_panel_io_handle_t s_io = nullptr;
static SemaphoreHandle_t s_free = nullptr;     // counts free DMA chunk buffers
static uint16_t *s_chunk[2] = {nullptr, nullptr};
static uint16_t s_lut[256];                    // RRRGGGBB -> RGB565, bytes swapped for the SPI
static bool s_hw_ok = false;

static bool onDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *)
{
	BaseType_t w = pdFALSE;
	xSemaphoreGiveFromISR(s_free, &w);
	return w == pdTRUE;
}

static void cmd(uint8_t c, const uint8_t *d = nullptr, size_t n = 0)
{
	esp_lcd_panel_io_tx_param(s_io, c, d, n);
}

static bool tftBegin(void)
{
	if (s_hw_ok) return true;
	for (int i = 0; i < 256; i++)
	{
		// pixel byte as the emulator builds it (gime_video.cpp): R = bits 7-6, G = bits 4-3, B = bits 1-0
		const uint16_t r = ((i >> 6) & 3) * 31 / 3, g = ((i >> 3) & 3) * 63 / 3, b = (i & 3) * 31 / 3;
		const uint16_t c = (r << 11) | (g << 5) | b;
		s_lut[i] = (c >> 8) | (c << 8);
	}
	pinMode(CYD_TFT_BL, OUTPUT);
	digitalWrite(CYD_TFT_BL, LOW);

	spi_bus_config_t bus = {};
	bus.mosi_io_num = CYD_TFT_MOSI;
	bus.miso_io_num = -1;
	bus.sclk_io_num = CYD_TFT_SCLK;
	bus.quadwp_io_num = -1;
	bus.quadhd_io_num = -1;
	bus.max_transfer_sz = CHUNK_BYTES + 8;
	if (spi_bus_initialize(CYD_TFT_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return false;

	s_free = xSemaphoreCreateCounting(2, 2);
	esp_lcd_panel_io_spi_config_t io = {};
	io.cs_gpio_num = CYD_TFT_CS;
	io.dc_gpio_num = CYD_TFT_DC;
	io.spi_mode = 0;
	io.pclk_hz = CYD_TFT_HZ;
	io.trans_queue_depth = 2;
	io.lcd_cmd_bits = 8;
	io.lcd_param_bits = 8;
	io.on_color_trans_done = onDone;
	if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)CYD_TFT_HOST, &io, &s_io) != ESP_OK) return false;

	for (int i = 0; i < 2; i++)
	{
		s_chunk[i] = (uint16_t *)heap_caps_malloc(CHUNK_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
		if (!s_chunk[i]) return false;
	}

	// ILI9341 power-up sequence (same registers as Adafruit's driver / TFT_eSPI)
	cmd(0x01); vTaskDelay(pdMS_TO_TICKS(130));
	static const uint8_t cf[] = {0x00, 0xC1, 0x30}, ed[] = {0x64, 0x03, 0x12, 0x81}, e8[] = {0x85, 0x00, 0x78},
	    cb[] = {0x39, 0x2C, 0x00, 0x34, 0x02}, f7[] = {0x20}, ea[] = {0x00, 0x00}, c0[] = {0x23}, c1[] = {0x10},
	    c5[] = {0x3E, 0x28}, c7[] = {0x86}, madctl[] = {CYD_TFT_MADCTL}, colmod[] = {0x55}, b1[] = {0x00, 0x18},
	    b6[] = {0x08, 0x82, 0x27}, f2[] = {0x00}, g26[] = {0x01},
	    gp[] = {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00},
	    gn[] = {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F};
	cmd(0xCF, cf, sizeof cf); cmd(0xED, ed, sizeof ed); cmd(0xE8, e8, sizeof e8); cmd(0xCB, cb, sizeof cb);
	cmd(0xF7, f7, 1); cmd(0xEA, ea, 2); cmd(0xC0, c0, 1); cmd(0xC1, c1, 1); cmd(0xC5, c5, 2); cmd(0xC7, c7, 1);
	cmd(0x36, madctl, 1); cmd(0x3A, colmod, 1); cmd(0xB1, b1, 2); cmd(0xB6, b6, 3); cmd(0xF2, f2, 1);
	cmd(0x26, g26, 1); cmd(0xE0, gp, 15); cmd(0xE1, gn, 15);
	if (CYD_TFT_INVERT) cmd(0x21);
	cmd(0x11); vTaskDelay(pdMS_TO_TICKS(130));
	cmd(0x29); vTaskDelay(pdMS_TO_TICKS(20));

	// black screen before the backlight comes on
	static const uint8_t ca[] = {0, 0, (CYD_TFT_W - 1) >> 8, (CYD_TFT_W - 1) & 255},
	    ra[] = {0, 0, (CYD_TFT_H - 1) >> 8, (CYD_TFT_H - 1) & 255};
	cmd(0x2A, ca, 4); cmd(0x2B, ra, 4);
	memset(s_chunk[0], 0, CHUNK_BYTES);
	for (int k = 0; k < CYD_TFT_H / CHUNK_LINES; k++)
	{
		xSemaphoreTake(s_free, portMAX_DELAY);
		esp_lcd_panel_io_tx_color(s_io, k == 0 ? 0x2C : 0x3C, s_chunk[0], CHUNK_BYTES);
		xSemaphoreTake(s_free, portMAX_DELAY);   // wait for it: only one buffer in use here
		xSemaphoreGive(s_free);
	}
	digitalWrite(CYD_TFT_BL, HIGH);
	s_hw_ok = true;
	return true;
}

// one frame: 8-bit lines -> RGB565 chunks -> SPI
static void tftPush(DMAVideoBuffer *buf, int b, int hRes, int vRes)
{
	const int step = hRes >= 480 ? 2 : 1;
	int off = ((int)hRes - CYD_TFT_W * step) / 2;
	if (off < 0) off = 0;
	int w = ((int)hRes - off) / step;
	if (w > CYD_TFT_W) w = CYD_TFT_W;
	const int rows = vRes < CYD_TFT_H ? vRes : CYD_TFT_H;
	const int yoff = (CYD_TFT_H - rows) / 2;
	static const uint8_t ca[] = {0, 0, (CYD_TFT_W - 1) >> 8, (CYD_TFT_W - 1) & 255};
	const uint8_t ra[] = {0, 0, (uint8_t)((CYD_TFT_H - 1) >> 8), (uint8_t)((CYD_TFT_H - 1) & 255)};
	cmd(0x2A, ca, 4);
	cmd(0x2B, ra, 4);
	for (int k = 0; k < CYD_TFT_H / CHUNK_LINES; k++)
	{
		xSemaphoreTake(s_free, portMAX_DELAY);
		uint16_t *d = s_chunk[k & 1];
		for (int l = 0; l < CHUNK_LINES; l++)
		{
			const int y = k * CHUNK_LINES + l - yoff;
			uint16_t *o = d + l * CYD_TFT_W;
			if (y < 0 || y >= rows) { memset(o, 0, CYD_TFT_W * 2); continue; }
			const uint8_t *s = buf->getLineAddr8(y, b) + off;
			int x = 0;
			if (step == 1) for (; x < w; x++) o[x] = s_lut[s[x]];
			else for (; x < w; x++) o[x] = s_lut[s[x * 2]];
			for (; x < CYD_TFT_W; x++) o[x] = 0;
		}
		esp_lcd_panel_io_tx_color(s_io, k == 0 ? 0x2C : 0x3C, d, CHUNK_BYTES);
	}
}

// ---------------------------------------------------------------------------
// push task: show() wakes it
// ---------------------------------------------------------------------------
static TaskHandle_t s_task = nullptr;
static volatile bool s_run = false, s_busy = false;
static DMAVideoBuffer *volatile s_pbuf = nullptr;
static volatile int s_pidx = 0, s_phres = 320, s_pvres = 240;

static void pushTask(void *)
{
	for (;;)
	{
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		if (!s_run) continue;
		DMAVideoBuffer *buf = s_pbuf;
		const int b = s_pidx, h = s_phres, v = s_pvres;
		if (!buf || !buf->isValid()) continue;
		s_busy = true;
		tftPush(buf, b, h, v);
		s_busy = false;
	}
}

// ---------------------------------------------------------------------------
// field sync: a timer drives a free GPIO low for ~127 us every frame; the emulator reads / interrupts on it
// exactly as it does with the VGA board's VSYNC
// ---------------------------------------------------------------------------
static gptimer_handle_t s_vs = nullptr;
static volatile bool s_vs_low = false;

static bool IRAM_ATTR vsAlarm(gptimer_handle_t t, const gptimer_alarm_event_data_t *e, void *)
{
	gptimer_alarm_config_t a = {};
	if (!s_vs_low)
	{
		REG_WRITE(GPIO_OUT_W1TC_REG, 1u << CYD_VSYNC_GPIO);
		s_vs_low = true;
		a.alarm_count = e->alarm_value + CYD_VSYNC_PULSE_US;
	}
	else
	{
		REG_WRITE(GPIO_OUT_W1TS_REG, 1u << CYD_VSYNC_GPIO);
		s_vs_low = false;
		a.alarm_count = e->alarm_value + (CYD_VSYNC_PERIOD_US - CYD_VSYNC_PULSE_US);
	}
	gptimer_set_alarm_action(t, &a);
	return false;
}

static void vsyncBegin(void)
{
	if (s_vs) return;
	gpio_config_t g = {};
	g.pin_bit_mask = 1ULL << CYD_VSYNC_GPIO;
	g.mode = GPIO_MODE_INPUT_OUTPUT;
	gpio_config(&g);
	gpio_set_level((gpio_num_t)CYD_VSYNC_GPIO, 1);
	gptimer_config_t c = {};
	c.clk_src = GPTIMER_CLK_SRC_DEFAULT;
	c.direction = GPTIMER_COUNT_UP;
	c.resolution_hz = 1000000;
	if (gptimer_new_timer(&c, &s_vs) != ESP_OK) { s_vs = nullptr; return; }
	gptimer_event_callbacks_t cb = {};
	cb.on_alarm = vsAlarm;
	gptimer_register_event_callbacks(s_vs, &cb, nullptr);
	gptimer_enable(s_vs);
	gptimer_alarm_config_t a = {};
	a.alarm_count = CYD_VSYNC_PERIOD_US;
	gptimer_set_alarm_action(s_vs, &a);
	gptimer_start(s_vs);
}

// ---------------------------------------------------------------------------
// VGA interface
// ---------------------------------------------------------------------------
VGA::VGA()
{
	bufferCount = 2;
	bits = 8;
	backBuffer = 0;
	dmaBuffer = nullptr;
	altBuffer = nullptr;
	usePsram = true;
	dmaChannel = 0;
}

VGA::~VGA() { deinit(); }

bool VGA::init(const PinConfig p, const Mode m, int b)
{
	pins = p;
	mode = m;
	bits = 8;
	if (!tftBegin()) return false;
	if (dmaBuffer) { delete dmaBuffer; dmaBuffer = nullptr; }
	dmaBuffer = new DMAVideoBuffer(mode.vRes, mode.hRes, 1, true, usePsram, bufferCount);
	if (!dmaBuffer->isValid()) return false;
	backBuffer = 0;
	if (!s_task) xTaskCreatePinnedToCore(pushTask, "TftPush", 4096, nullptr, 2, &s_task, 0);
	vsyncBegin();
	return start();
}

bool VGA::Reinit(const PinConfig p, const Mode m, int b)
{
	stop();
	if (altBuffer) { delete altBuffer; altBuffer = nullptr; }
	return init(p, m, b);
}

void VGA::deinit()
{
	stop();
	if (dmaBuffer) { delete dmaBuffer; dmaBuffer = nullptr; }
	if (altBuffer) { delete altBuffer; altBuffer = nullptr; }
}

bool VGA::start() { s_run = true; return true; }

bool VGA::stop()
{
	s_run = false;
	for (int i = 0; i < 100 && s_busy; i++) vTaskDelay(1);
	return true;
}

bool VGA::show()
{
	if (!dmaBuffer) return false;
	s_pbuf = dmaBuffer;
	s_pidx = backBuffer;
	s_phres = mode.hRes;
	s_pvres = mode.vRes;
	if (s_task) xTaskNotifyGive(s_task);
	if (bufferCount > 1) backBuffer = (backBuffer + 1) % bufferCount;
	return true;
}

bool VGA::prepareAlternate(const Mode m)
{
	if (altBuffer) { delete altBuffer; altBuffer = nullptr; }
	if (m.vRes != mode.vRes) return false;
	altBuffer = new DMAVideoBuffer(m.vRes, m.hRes, 1, true, usePsram, bufferCount);
	if (!altBuffer->isValid()) { delete altBuffer; altBuffer = nullptr; return false; }
	altMode = m;
	return true;
}

bool VGA::hasAlternate(const Mode m) const { return altBuffer && altMode.hRes == m.hRes; }

bool VGA::switchToAlternate(void (*waitSync)(void))
{
	if (!altBuffer) return false;
	if (waitSync) waitSync();
	Mode m = mode; mode = altMode; altMode = m;
	DMAVideoBuffer *t = dmaBuffer; dmaBuffer = altBuffer; altBuffer = t;   // the old one stays valid for the push task
	backBuffer = 0;
	return true;
}

int VGA::rgb(uint8_t r, uint8_t g, uint8_t b) { return (r >> 5) | ((g >> 5) << 3) | (b & 0b11000000); }

void VGA::dot(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
	if (x < 0 || y < 0 || x >= (int)mode.hRes || y >= (int)mode.vRes) return;
	dmaBuffer->getLineAddr8(y, backBuffer)[x] = (r >> 5) | ((g >> 5) << 3) | (b & 0b11000000);
}

void VGA::dot(int x, int y, int rgb)
{
	if (x < 0 || y < 0 || x >= (int)mode.hRes || y >= (int)mode.vRes) return;
	dmaBuffer->getLineAddr8(y, backBuffer)[x] = rgb;
}

void VGA::dotdit(int x, int y, uint8_t r, uint8_t g, uint8_t b) { dot(x, y, r, g, b); }

uint8_t VGA::getPixel(int x, int y)
{
	if (x < 0 || y < 0 || x >= (int)mode.hRes || y >= (int)mode.vRes) return 0;
	return dmaBuffer->getLineAddr8(y, backBuffer)[x];
}

void VGA::clear(int rgb)
{
	for (int y = 0; y < (int)mode.vRes; y++) memset(dmaBuffer->getLineAddr8(y, backBuffer), rgb, mode.hRes);
}

void VGA::drawLineFromMemory8(int x, int y, const uint8_t *m) { memcpy(&dmaBuffer->getLineAddr8(y, backBuffer)[x], m, 8); }
void VGA::drawLineFromMemory16(int x, int y, const uint8_t *m) { memcpy(&dmaBuffer->getLineAddr8(y, backBuffer)[x], m, 16); }
IRAM_ATTR void VGA::drawLineFromMemory256(int x, int y, const uint8_t *m) { memcpy(&dmaBuffer->getLineAddr8(y, backBuffer)[x], m, 256); }

#endif // BOARD_CYD_S3
