#include "MenuScreen.h"

#include "../core/ScreenManager.h"

namespace {

void onItemClicked(lv_event_t *event) {
    auto *target = static_cast<Screen *>(lv_event_get_user_data(event));
    ScreenManager::instance().show(target);
}

}  // namespace

void MenuScreen::create() {
    initRoot();

    lv_obj_t *title = lv_label_create(root_);
    lv_label_set_text(title, "menu");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    list_ = lv_list_create(root_);
    lv_obj_set_size(list_, 220, 260);
    lv_obj_align(list_, LV_ALIGN_BOTTOM_MID, 0, -10);

    ScreenManager &manager = ScreenManager::instance();
    for (int i = 0; i < manager.screenCount(); i++) {
        Screen *screen = manager.screens()[i];
        lv_obj_t *btn = lv_list_add_button(list_, nullptr, screen->name());
        lv_obj_add_event_cb(btn, onItemClicked, LV_EVENT_CLICKED, screen);
        lv_group_add_obj(group_, btn);
    }
}

void MenuScreen::onShow() {
    /* Foca o primeiro item a cada abertura do menu. */
    if (lv_group_get_obj_count(group_) > 0) {
        lv_group_focus_obj(lv_obj_get_child(list_, 0));
    }
}
