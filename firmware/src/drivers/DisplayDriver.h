#pragma once

#include <lvgl.h>

/* Inicializa o painel GMT020-02 (ST7789V via SPI) com LovyanGFX e registra o
 * display no LVGL. Deve ser chamado uma unica vez, antes de qualquer widget. */
class DisplayDriver {
public:
    static constexpr int32_t kWidth = 240;
    static constexpr int32_t kHeight = 320;

    static void init();

    /* Arma um screenshot: forca o redesenho da tela inteira e transmite cada
     * faixa renderizada pela Serial (protocolo SNAP, RGB565 little-endian).
     * Consumido por tools/screenshot.py. */
    static void requestSnapshot();

    /* true enquanto um snapshot esta sendo transmitido: outros logs seriais
     * devem se calar para nao corromper o fluxo binario. */
    static bool snapshotArmed();

private:
    static void flushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *pxMap);
};
