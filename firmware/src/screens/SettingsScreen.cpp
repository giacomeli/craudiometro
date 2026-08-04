#include "SettingsScreen.h"

#include <Arduino.h>

#include "../core/Settings.h"
#include "../drivers/BleService.h"
#include "../drivers/DisplayDriver.h"

void settingsResetClicked(lv_event_t *event);
void settingsDisarmTimerCb(lv_timer_t *timer);

namespace {

constexpr const char *kResetIdleText = "resetar pareamento BLE";
constexpr const char *kResetArmedText = "clique de novo p/ confirmar";
constexpr const char *kFirmwareVersion = "Craudiometro v1.0.0";

void timeoutChanged(lv_event_t *event) {
    auto *dropdown = static_cast<lv_obj_t *>(lv_event_get_target(event));
    Settings::instance().setScreenTimeoutIdx(
        static_cast<uint8_t>(lv_dropdown_get_selected(dropdown)));
}

void themeChanged(lv_event_t *event) {
    auto *sw = static_cast<lv_obj_t *>(lv_event_get_target(event));
    const bool light = lv_obj_has_state(sw, LV_STATE_CHECKED);
    Settings::instance().setTheme(light ? Settings::Theme::Light
                                        : Settings::Theme::Dark);
    /* Tema do LVGL e aplicado na criacao dos widgets: reinicia para valer. */
    delay(150);
    ESP.restart();
}

lv_obj_t *makeRow(lv_obj_t *parent, const char *text) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(row, 8, 0);

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, text);
    return row;
}

}  // namespace

void settingsResetClicked(lv_event_t *event) {
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    if (!self->resetArmed_) {
        self->resetArmed_ = true;
        lv_label_set_text(self->resetBtnLabel_, kResetArmedText);
        self->disarmTimer_ = lv_timer_create(settingsDisarmTimerCb, 5000, self);
        lv_timer_set_repeat_count(self->disarmTimer_, 1);
    } else {
        BleService::resetBonds();
        self->disarmReset();
    }
}

void settingsDisarmTimerCb(lv_timer_t *timer) {
    auto *self = static_cast<SettingsScreen *>(lv_timer_get_user_data(timer));
    self->disarmTimer_ = nullptr; /* repeat_count=1: o timer se autodestroi */
    self->disarmReset();
}

void SettingsScreen::create() {
    initRoot();

    lv_obj_t *title = lv_label_create(root_);
    lv_label_set_text(title, "configuracoes");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *panel = lv_obj_create(root_);
    lv_obj_set_size(panel, DisplayDriver::kWidth - 16, 240);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 10, 0);

    /* Timeout de tela. */
    lv_obj_t *timeoutRow = makeRow(panel, "timeout de tela");
    lv_obj_t *dropdown = lv_dropdown_create(timeoutRow);
    lv_dropdown_set_options(dropdown, "sempre acesa\n1 min\n5 min\n15 min");
    lv_dropdown_set_selected(dropdown, Settings::instance().screenTimeoutIdx());
    lv_obj_set_width(dropdown, 110);
    lv_obj_add_event_cb(dropdown, timeoutChanged, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_group_add_obj(group_, dropdown);

    /* Tema. */
    lv_obj_t *themeRow = makeRow(panel, "tema claro");
    lv_obj_t *sw = lv_switch_create(themeRow);
    if (Settings::instance().theme() == Settings::Theme::Light) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw, themeChanged, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_group_add_obj(group_, sw);

    /* Reset de pareamento (clique duplo confirma). */
    lv_obj_t *resetBtn = lv_button_create(panel);
    lv_obj_set_width(resetBtn, lv_pct(100));
    resetBtnLabel_ = lv_label_create(resetBtn);
    lv_label_set_text(resetBtnLabel_, kResetIdleText);
    lv_obj_center(resetBtnLabel_);
    lv_obj_add_event_cb(resetBtn, settingsResetClicked, LV_EVENT_CLICKED, this);
    lv_group_add_obj(group_, resetBtn);

    lv_obj_t *version = lv_label_create(root_);
    lv_label_set_text(version, kFirmwareVersion);
    lv_obj_set_style_text_opa(version, LV_OPA_50, 0);
    lv_obj_align(version, LV_ALIGN_BOTTOM_MID, 0, -8);
}

void SettingsScreen::onHide() {
    disarmReset();
}

void SettingsScreen::disarmReset() {
    if (disarmTimer_ != nullptr) {
        lv_timer_delete(disarmTimer_);
        disarmTimer_ = nullptr;
    }
    resetArmed_ = false;
    if (resetBtnLabel_ != nullptr) {
        lv_label_set_text(resetBtnLabel_, kResetIdleText);
    }
}
