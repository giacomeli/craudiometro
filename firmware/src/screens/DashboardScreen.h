#pragma once

#include "Screen.h"
#include "ScreenChrome.h"

/* Tela inicial em cards: chrome compartilhado (header com icone/relogio/plano
 * e rodape com asterisco pulsante) + um card por janela de uso (sessao 5h e
 * semana) com percentual, barra por faixa de cor e countdown. */
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
    static void timerCb(lv_timer_t *timer);

    ScreenChrome chrome_;
    UsageCard sessionCard_;
    UsageCard weekCard_;
    lv_timer_t *refreshTimer_ = nullptr;
};
