#pragma once

#include <lvgl.h>

/* KY-040 como input device de encoder do LVGL.
 *
 * - Quadratura CLK/DT lida pelo periferico PCNT (x4, sem perda de passos);
 *   cada detent mecanico equivale a 4 contagens.
 * - Botao SW com debounce por software.
 * - Clique curto vira um clique LVGL (ENTER no widget focado). O clique e
 *   reportado de forma adiada, apenas na soltura: assim um clique longo nunca
 *   dispara clique acidental no widget.
 * - Clique longo (>= 800 ms) NAO vai para o LVGL: dispara o handler global
 *   registrado (usado pelo ScreenManager para abrir o menu/voltar).
 */
class EncoderDriver {
public:
    using LongPressHandler = void (*)();

    static void init();
    static lv_indev_t *indev();
    static void setLongPressHandler(LongPressHandler handler);

    /* Timestamp (millis) da ultima atividade no encoder — base do timeout de
     * tela da Etapa 9. */
    static uint32_t lastActivityMs();

private:
    static void readCb(lv_indev_t *indev, lv_indev_data_t *data);
};
