#include "DisplayDriver.h"

#include <Arduino.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#ifndef CRAUDIO_DISPLAY_INVERT
#define CRAUDIO_DISPLAY_INVERT 1
#endif

namespace {

/* Waveshare ESP32-C6-LCD-1.47-H em paisagem: mesmo painel, mesmos pinos e
 * mesma config SPI da variante retrato (DisplayDriver_C6Lcd147.cpp) — a
 * unica diferenca e a chamada a setRotation() em init(). panel_width/height/
 * offset_x abaixo sao a geometria NATIVA (retrato) do controlador; o
 * LovyanGFX remapeia as coordenadas internamente ao girar. */
constexpr int kPinSclk = 7;
constexpr int kPinMosi = 6;
constexpr int kPinDc = 15;
constexpr int kPinCs = 14;
constexpr int kPinRst = 21;
constexpr int kPinBacklight = 22;

constexpr int32_t kPanelWidthNative = 172;
constexpr int32_t kPanelHeightNative = 320;

/* Dimensoes logicas apos a rotacao de 90 graus. */
constexpr int32_t kPanelWidth = 320;
constexpr int32_t kPanelHeight = 172;

class C6Lcd147LandscapePanel : public lgfx::LGFX_Device {
public:
    C6Lcd147LandscapePanel() {
        {
            auto cfg = bus_.config();
            /* O ESP32-C6 nao tem VSPI/HSPI: o barramento SPI de proposito
             * geral e o SPI2_HOST. */
            cfg.spi_host = SPI2_HOST;
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
            cfg.panel_width = kPanelWidthNative;
            cfg.panel_height = kPanelHeightNative;
            /* Geometria nativa (retrato) do controlador ST7789 desta placa;
             * ver DisplayDriver_C6Lcd147.cpp para o motivo do offset_x=34. */
            cfg.offset_x = 34;
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

C6Lcd147LandscapePanel gfx;

/* Buffer parcial de 60 linhas na largura JA GIRADA (320 * 60 * 2 bytes =
 * 38,4 KB em DRAM). Ver DisplayDriver_Esp32Dev.cpp para o motivo do
 * alignas(4). */
constexpr uint32_t kBufLines = 60;
alignas(4) uint16_t drawBuf[kPanelWidth * kBufLines];

bool g_snapshotArmed = false;

}  // namespace

const int32_t DisplayDriver::kWidth = kPanelWidth;
const int32_t DisplayDriver::kHeight = kPanelHeight;
const bool DisplayDriver::kLandscape = true;

void DisplayDriver::init() {
    pinMode(kPinBacklight, OUTPUT);
    digitalWrite(kPinBacklight, HIGH);

    gfx.init();
    /* Gira 90 graus: o LovyanGFX passa a reportar/aceitar coordenadas na
     * geometria paisagem (320x172) a partir daqui. */
    gfx.setRotation(1);
    gfx.setSwapBytes(true);

    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return millis(); });

    lv_display_t *disp = lv_display_create(kWidth, kHeight);
    lv_display_set_flush_cb(disp, flushCb);
    lv_display_set_buffers(disp, drawBuf, nullptr, sizeof(drawBuf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
}

bool DisplayDriver::snapshotArmed() {
    return g_snapshotArmed;
}

void DisplayDriver::requestSnapshot() {
    g_snapshotArmed = true;
    lv_obj_invalidate(lv_screen_active());
    lv_obj_invalidate(lv_layer_top());
    Serial.printf("SNAP BEGIN %d %d\n", kWidth, kHeight);
}

void DisplayDriver::flushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *pxMap) {
    const uint32_t w = lv_area_get_width(area);
    const uint32_t h = lv_area_get_height(area);

    gfx.startWrite();
    gfx.setAddrWindow(area->x1, area->y1, w, h);
    gfx.writePixels(reinterpret_cast<uint16_t *>(pxMap), w * h);
    gfx.endWrite();

    if (g_snapshotArmed) {
        Serial.printf("SNAP AREA %d %d %u %u\n", static_cast<int>(area->x1),
                      static_cast<int>(area->y1), static_cast<unsigned>(w),
                      static_cast<unsigned>(h));
        Serial.write(pxMap, w * h * 2);
        Serial.print("\n");
        if (lv_display_flush_is_last(disp)) {
            Serial.print("SNAP END\n");
            g_snapshotArmed = false;
        }
    }

    lv_display_flush_ready(disp);
}
