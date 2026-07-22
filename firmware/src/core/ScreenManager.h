#pragma once

#include <lvgl.h>

#include "../screens/Screen.h"

/* Registro e navegacao de telas.
 *
 * - Telas sao registradas antes de init(); o menu global e gerado a partir
 *   desse registro (adicionar tela nova nao exige mexer no menu).
 * - Clique longo do encoder: abre o menu; dentro do menu, volta.
 * - O handler de clique longo apenas agenda a acao; a troca de tela acontece
 *   em tick(), fora do processamento de input do LVGL.
 * - Timeout de tela: sem atividade do encoder pelo tempo configurado, um
 *   overlay preto cobre a UI e o grupo do indev e esvaziado; a primeira acao
 *   do encoder apenas acorda (e descartada, nunca chega aos widgets). */
class ScreenManager {
public:
    static constexpr int kMaxScreens = 8;

    static ScreenManager &instance();

    /* Chamar antes de init(). A primeira tela registrada e a inicial. */
    void registerScreen(Screen *screen);

    void init();
    void show(Screen *screen);
    /* Troca de tela pelo nome do registro ("menu" abre o menu global).
     * Usado pelos comandos seriais (tools/screenshot.py). */
    void showByName(const char *name);
    void tick();  /* chamar a cada volta do loop */

    Screen *const *screens() const { return screens_; }
    int screenCount() const { return count_; }
    Screen *current() const { return current_; }

private:
    ScreenManager() = default;

    void handleLongPress();
    void tickDimming();
    void dim();
    void wake();
    static void longPressTrampoline();
    static void stateChangedTrampoline();

    Screen *screens_[kMaxScreens] = {};
    int count_ = 0;
    Screen *current_ = nullptr;
    Screen *beforeMenu_ = nullptr;
    Screen *menu_ = nullptr;
    volatile bool pendingLongPress_ = false;

    lv_group_t *emptyGroup_ = nullptr; /* usado enquanto a tela esta apagada */
    lv_obj_t *dimOverlay_ = nullptr;
    uint32_t activityAtDim_ = 0;
    uint32_t wakeAtMs_ = 0; /* aguarda o clique que acordou drenar */
};
