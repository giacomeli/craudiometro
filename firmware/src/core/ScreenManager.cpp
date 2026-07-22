#include "ScreenManager.h"

#include <Arduino.h>
#include <cstring>

#include "../drivers/EncoderDriver.h"
#include "../screens/MenuScreen.h"
#include "AppState.h"
#include "Settings.h"

namespace {
MenuScreen g_menuScreen;
}

ScreenManager &ScreenManager::instance() {
    static ScreenManager manager;
    return manager;
}

void ScreenManager::registerScreen(Screen *screen) {
    if (count_ < kMaxScreens) {
        screens_[count_++] = screen;
    }
}

void ScreenManager::init() {
    menu_ = &g_menuScreen;
    emptyGroup_ = lv_group_create();

    for (int i = 0; i < count_; i++) {
        screens_[i]->create();
    }
    menu_->create();

    EncoderDriver::setLongPressHandler(longPressTrampoline);
    AppState::instance().setListener(stateChangedTrampoline);

    if (count_ > 0) {
        show(screens_[0]);
    }
}

void ScreenManager::show(Screen *screen) {
    if (screen == nullptr || screen == current_) {
        return;
    }
    if (current_ != nullptr) {
        current_->onHide();
    }
    current_ = screen;
    lv_screen_load_anim(screen->root(), LV_SCR_LOAD_ANIM_FADE_IN, 120, 0, false);
    lv_indev_set_group(EncoderDriver::indev(), screen->group());
    screen->onShow();
    screen->onStateChanged();
}

void ScreenManager::showByName(const char *name) {
    if (strcmp(name, "menu") == 0) {
        beforeMenu_ = current_;
        show(menu_);
        return;
    }
    for (int i = 0; i < count_; i++) {
        if (strcmp(screens_[i]->name(), name) == 0) {
            show(screens_[i]);
            return;
        }
    }
}

void ScreenManager::tick() {
    if (pendingLongPress_) {
        pendingLongPress_ = false;
        if (dimOverlay_ == nullptr) {
            handleLongPress();
        } /* apagado: o proprio wake em tickDimming trata a atividade */
    }
    tickDimming();
}

void ScreenManager::tickDimming() {
    const uint32_t timeoutMs = Settings::instance().screenTimeoutMs();
    const uint32_t lastActivity = EncoderDriver::lastActivityMs();

    if (dimOverlay_ != nullptr) {
        /* Apagado: qualquer atividade nova acorda. */
        if (lastActivity != activityAtDim_) {
            lv_obj_delete(dimOverlay_);
            dimOverlay_ = nullptr;
            wakeAtMs_ = millis();
        }
        return;
    }

    if (wakeAtMs_ != 0) {
        /* Acordou: mantem o grupo vazio ate a acao que acordou drenar. */
        if (millis() - wakeAtMs_ >= 200) {
            wake();
        }
        return;
    }

    if (timeoutMs > 0 && (millis() - lastActivity) >= timeoutMs) {
        dim();
    }
}

void ScreenManager::dim() {
    activityAtDim_ = EncoderDriver::lastActivityMs();
    wakeAtMs_ = 0;

    dimOverlay_ = lv_obj_create(lv_layer_top());
    lv_obj_set_size(dimOverlay_, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(dimOverlay_, 0, 0);
    lv_obj_set_style_bg_color(dimOverlay_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(dimOverlay_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dimOverlay_, 0, 0);
    lv_obj_set_style_radius(dimOverlay_, 0, 0);

    /* Descarta toda entrada do encoder enquanto apagado. */
    lv_indev_set_group(EncoderDriver::indev(), emptyGroup_);
}

void ScreenManager::wake() {
    if (current_ != nullptr) {
        lv_indev_set_group(EncoderDriver::indev(), current_->group());
    }
    wakeAtMs_ = 0;
}

void ScreenManager::handleLongPress() {
    if (current_ == menu_) {
        show(beforeMenu_ != nullptr ? beforeMenu_ : screens_[0]);
    } else {
        beforeMenu_ = current_;
        show(menu_);
    }
}

void ScreenManager::longPressTrampoline() {
    /* Chamado de dentro do read callback do indev: apenas agenda. */
    instance().pendingLongPress_ = true;
}

void ScreenManager::stateChangedTrampoline() {
    Screen *current = instance().current_;
    if (current != nullptr) {
        current->onStateChanged();
    }
}
