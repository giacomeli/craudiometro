#pragma once

#include <lvgl.h>

/* Chip: label pequeno com fundo arredondado, neutro nos dois temas. */
lv_obj_t *makeChip(lv_obj_t *parent, const char *text);

/* Header (icone do Craudinho + relogio central + badge do plano) e rodape
 * (asterisco pulsante + textos rotativos / "craudiometro") compartilhados
 * entre telas.
 *
 * Uso pela tela dona: create() dentro do create() da tela; onShow()/onHide()
 * no ciclo de vida (timers e animacao so rodam com a tela visivel); refresh()
 * quando o AppState muda. */
class ScreenChrome {
public:
    void create(lv_obj_t *root);
    void onShow();
    void onHide();
    void refresh();

private:
    void updateSpinnerText();
    static void clockTimerCb(lv_timer_t *timer);
    static void spinTimerCb(lv_timer_t *timer);

    lv_obj_t *clockLabel_ = nullptr;
    lv_obj_t *planChip_ = nullptr;
    lv_obj_t *spinnerLabel_ = nullptr;
    lv_obj_t *statusTextLabel_ = nullptr;
    lv_timer_t *clockTimer_ = nullptr;
    lv_timer_t *spinTextTimer_ = nullptr;
    uint8_t textIdx_ = 0;
};
