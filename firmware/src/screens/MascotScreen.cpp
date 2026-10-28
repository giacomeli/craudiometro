#include "MascotScreen.h"

#include <cstdio>

#include "../assets/craudio_sprites.h"
#include "../core/AppState.h"
#include "../drivers/DisplayDriver.h"

namespace {

struct MoodSpec {
    const lv_image_dsc_t *const *frames;
    uint8_t frameCount;
    uint32_t intervalMs;
    const char *caption;
};

const MoodSpec kMoods[] = {
    {kCraudioSleep, kCraudioSleepCount, 900, "zzz... (sem daemon)"},
    {kCraudioIdle, kCraudioIdleCount, 600, "tranquilo"},
    {kCraudioNormal, kCraudioNormalCount, 280, "trabalhando"},
    {kCraudioFrantic, kCraudioFranticCount, 130, "no talo!"},
};

}  // namespace

void MascotScreen::create() {
    initRoot();
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    chrome_.create(root_);

    image_ = lv_image_create(root_);
    lv_image_set_src(image_, kCraudioSleep[0]);

    captionLabel_ = lv_label_create(root_);
    lv_label_set_text(captionLabel_, "");
    lv_obj_set_style_text_opa(captionLabel_, LV_OPA_60, 0);

    if (DisplayDriver::kLandscape) {
        /* Sprite de 128x128 nao cabe na faixa de conteudo de ~98px de
         * altura (ver ScreenChrome): encolhe e poe lado a lado com a
         * legenda em vez de empilhado. */
        lv_image_set_scale(image_, 170); /* ~66%, ~84x84 efetivos */
        lv_obj_align(image_, LV_ALIGN_LEFT_MID, 24, 0);

        lv_obj_set_width(captionLabel_, 150);
        lv_label_set_long_mode(captionLabel_, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(captionLabel_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(captionLabel_, LV_ALIGN_RIGHT_MID, -16, 0);
    } else {
        lv_obj_align(image_, LV_ALIGN_CENTER, 0, -14);
        lv_obj_align(captionLabel_, LV_ALIGN_CENTER, 0, 72);
    }
}

void MascotScreen::onShow() {
    chrome_.onShow();
    if (animTimer_ == nullptr) {
        animTimer_ = lv_timer_create(timerCb, kMoods[0].intervalMs, this);
    }
    applyMood(moodFromState());
}

void MascotScreen::onHide() {
    chrome_.onHide();
    if (animTimer_ != nullptr) {
        lv_timer_delete(animTimer_);
        animTimer_ = nullptr;
    }
}

void MascotScreen::onStateChanged() {
    applyMood(moodFromState());
    chrome_.refresh();
}

MascotScreen::Mood MascotScreen::moodFromState() const {
    const AppState &state = AppState::instance();
    if (!state.bleConnected() || state.usage().sessionPct < 0) {
        return Mood::Sleep;
    }
    const int pct = state.usage().sessionPct > state.usage().weekPct
                        ? state.usage().sessionPct
                        : state.usage().weekPct;
    if (pct < 30) {
        return Mood::Idle;
    }
    if (pct < 70) {
        return Mood::Normal;
    }
    return Mood::Frantic;
}

void MascotScreen::applyMood(Mood mood) {
    const MoodSpec &spec = kMoods[static_cast<uint8_t>(mood)];
    if (mood != mood_) {
        mood_ = mood;
        frame_ = 0;
    }
    if (animTimer_ != nullptr) {
        lv_timer_set_period(animTimer_, spec.intervalMs);
    }
    lv_image_set_src(image_, spec.frames[frame_ % spec.frameCount]);
    lv_label_set_text(captionLabel_, spec.caption);
}

void MascotScreen::nextFrame() {
    const MoodSpec &spec = kMoods[static_cast<uint8_t>(mood_)];
    frame_ = (frame_ + 1) % spec.frameCount;
    lv_image_set_src(image_, spec.frames[frame_]);
}

void MascotScreen::timerCb(lv_timer_t *timer) {
    static_cast<MascotScreen *>(lv_timer_get_user_data(timer))->nextFrame();
}
