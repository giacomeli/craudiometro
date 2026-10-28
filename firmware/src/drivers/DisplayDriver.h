#pragma once

#include <lvgl.h>

/* Inicializa o painel do board (ST7789 via SPI, LovyanGFX) e registra o
 * display no LVGL. Deve ser chamado uma unica vez, antes de qualquer widget.
 *
 * kWidth/kHeight sao definidos no .cpp especifico do board compilado
 * (DisplayDriver_Esp32Dev.cpp ou DisplayDriver_C6Lcd147.cpp, selecionado por
 * build_src_filter no platformio.ini) — este header nao assume nenhum board. */
class DisplayDriver {
public:
    static const int32_t kWidth;
    static const int32_t kHeight;
    /* true quando o board compilado usa orientacao paisagem (largura >
     * altura) — telas que precisam de um layout diferente por orientacao
     * (nao so por largura) checam esta flag em vez de comparar kWidth. */
    static const bool kLandscape;

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
