#pragma once

#include "Screen.h"
#include "ScreenChrome.h"

/* Craudinho, releitura pixel-art do mascote do Claude Code.
 *
 * A animacao segue a maior utilizacao entre sessao e semana:
 *   desconectado -> dormindo; < 30% -> parado; < 70% -> normal; senao frenetico.
 * A intensidade tambem acelera a troca de quadros. */
class MascotScreen : public Screen {
public:
    const char *name() const override { return "craudinho"; }
    void create() override;
    void onShow() override;
    void onHide() override;
    void onStateChanged() override;

private:
    enum class Mood : uint8_t { Sleep, Idle, Normal, Frantic };

    Mood moodFromState() const;
    void applyMood(Mood mood);
    void nextFrame();
    static void timerCb(lv_timer_t *timer);

    ScreenChrome chrome_;
    lv_obj_t *image_ = nullptr;
    lv_obj_t *captionLabel_ = nullptr;
    lv_timer_t *animTimer_ = nullptr;
    Mood mood_ = Mood::Sleep;
    uint8_t frame_ = 0;
};
