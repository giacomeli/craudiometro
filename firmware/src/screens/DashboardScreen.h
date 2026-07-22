#pragma once

#include "Screen.h"

/* Tela inicial em cards: header com icone do Craudio, relogio central (hora
 * local recebida do daemon, avancada por millis() entre syncs) e badge do
 * plano; um card por janela de uso (sessao 5h e semana) com percentual,
 * barra por faixa de cor e countdown; rodape com asterisco pulsante estilo
 * Claude Code — textos rotativos enquanto sincroniza, "craudiometro" depois. */
class DashboardScreen : public Screen {
public:
    const char *name() const override { return "dashboard"; }
    void create() override;
    void onShow() override;
    void onHide() override;
    void onStateChanged() override;

private:
    struct UsageCard {
        lv_obj_t *pctLabel = nullptr;
        lv_obj_t *bar = nullptr;
        lv_obj_t *resetLabel = nullptr;
    };

    UsageCard makeCard(int y, const char *chipText);
    void refresh();
    void refreshCard(const UsageCard &card, int pct, int32_t remainingMin);
    void updateSpinnerText();
    static void timerCb(lv_timer_t *timer);
    static void spinTimerCb(lv_timer_t *timer);

    lv_obj_t *clockLabel_ = nullptr;
    lv_obj_t *planChip_ = nullptr;
    UsageCard sessionCard_;
    UsageCard weekCard_;
    lv_obj_t *spinnerLabel_ = nullptr;    /* o asterisco */
    lv_obj_t *statusTextLabel_ = nullptr; /* texto ao lado */
    lv_timer_t *refreshTimer_ = nullptr;
    lv_timer_t *spinTextTimer_ = nullptr;
    uint8_t textIdx_ = 0;
};
