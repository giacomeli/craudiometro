#pragma once

#include <lvgl.h>

/* Inicializa o painel GMT020-02 (ST7789V via SPI) com LovyanGFX e registra o
 * display no LVGL. Deve ser chamado uma unica vez, antes de qualquer widget. */
class DisplayDriver {
public:
    static constexpr int32_t kWidth = 240;
    static constexpr int32_t kHeight = 320;

    static void init();

private:
    static void flushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *pxMap);
};
