#pragma once

#include "Screen.h"

/* Configuracoes: timeout de tela, tema e reset do pareamento BLE.
 * Valores persistidos em NVS via Settings. A troca de tema reinicia o
 * dispositivo (o tema do LVGL e aplicado na criacao dos widgets). */
class SettingsScreen : public Screen {
public:
    const char *name() const override { return "configuracoes"; }
    void create() override;
    void onHide() override;

private:
    void disarmReset();

    lv_obj_t *resetBtnLabel_ = nullptr;
    lv_timer_t *disarmTimer_ = nullptr;
    bool resetArmed_ = false;

    friend void settingsResetClicked(lv_event_t *event);
    friend void settingsDisarmTimerCb(lv_timer_t *timer);
};
