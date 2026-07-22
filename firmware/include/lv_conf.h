/* Configuracao minima do LVGL 9 para o Craudiometro.
 * Opcoes nao definidas aqui caem nos defaults de lv_conf_internal.h. */

#ifndef LV_CONF_H
#define LV_CONF_H

/* RGB565 — o swap de bytes para o SPI e feito pelo LovyanGFX (setSwapBytes). */
#define LV_COLOR_DEPTH 16

/* Heap interno do LVGL. */
#define LV_MEM_SIZE (48U * 1024U)

/* Periodo de refresh/leitura de input (ms). */
#define LV_DEF_REFR_PERIOD 16

/* Fontes usadas na UI (Montserrat embutida — sem fontes proprietarias). */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_42 1
#define LV_FONT_DEFAULT (&lv_font_montserrat_14)

/* Log desligado em producao; ligar para depurar renderizacao. */
#define LV_USE_LOG 0

#endif /* LV_CONF_H */
