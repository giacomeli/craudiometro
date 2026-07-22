#include "DisplayDriver.h"

#include <Arduino.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#ifndef CRAUDIO_DISPLAY_INVERT
#define CRAUDIO_DISPLAY_INVERT 1
#endif

namespace {

/* Fiacao definida na spec (specs/plans/2026-07-21-202823_craudiometro-v1). */
constexpr int kPinSclk = 18;
constexpr int kPinMosi = 23;
constexpr int kPinDc = 2;
constexpr int kPinCs = 5;
constexpr int kPinRst = 4;

class Gmt020Panel : public lgfx::LGFX_Device {
public:
    Gmt020Panel() {
        {
            auto cfg = bus_.config();
            cfg.spi_host = VSPI_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 40000000;
            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = kPinSclk;
            cfg.pin_mosi = kPinMosi;
            cfg.pin_miso = -1;
            cfg.pin_dc = kPinDc;
            bus_.config(cfg);
            panel_.setBus(&bus_);
        }
        {
            auto cfg = panel_.config();
            cfg.pin_cs = kPinCs;
            cfg.pin_rst = kPinRst;
            cfg.pin_busy = -1;
            cfg.panel_width = DisplayDriver::kWidth;
            cfg.panel_height = DisplayDriver::kHeight;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.readable = false;
            cfg.invert = (CRAUDIO_DISPLAY_INVERT != 0);
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            panel_.config(cfg);
        }
        setPanel(&panel_);
    }

private:
    lgfx::Panel_ST7789 panel_;
    lgfx::Bus_SPI bus_;
};

Gmt020Panel gfx;

/* Buffer parcial de 60 linhas (240 * 60 * 2 bytes = 28,8 KB em DRAM). */
constexpr uint32_t kBufLines = 60;
uint16_t drawBuf[DisplayDriver::kWidth * kBufLines];

}  // namespace

void DisplayDriver::init() {
    gfx.init();
    /* O LVGL renderiza RGB565 nativo; o LovyanGFX faz o swap no push. */
    gfx.setSwapBytes(true);

    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return millis(); });

    lv_display_t *disp = lv_display_create(kWidth, kHeight);
    lv_display_set_flush_cb(disp, flushCb);
    lv_display_set_buffers(disp, drawBuf, nullptr, sizeof(drawBuf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
}

void DisplayDriver::flushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *pxMap) {
    const uint32_t w = lv_area_get_width(area);
    const uint32_t h = lv_area_get_height(area);

    gfx.startWrite();
    gfx.setAddrWindow(area->x1, area->y1, w, h);
    gfx.writePixels(reinterpret_cast<uint16_t *>(pxMap), w * h);
    gfx.endWrite();

    lv_display_flush_ready(disp);
}
