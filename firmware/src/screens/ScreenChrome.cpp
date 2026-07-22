#include "ScreenChrome.h"

#include <cstdio>

#include "../assets/craudio_sprites.h"
#include "../core/AppState.h"

namespace {

/* Cor de destaque do rodape: a mesma do corpo do Craudinho. */
const uint32_t kAccentHex = 0xCA7D6E;

/* Textos rotativos enquanto sincroniza, como no Claude Code (em ingles,
 * a pedido do usuario; ASCII por limitacao das fontes embutidas). */
constexpr const char *kSpinnerTexts[] = {
    "Baking...",     "Brewing...",   "Pondering...", "Simmering...",
    "Marinating...", "Noodling...",  "Tinkering...", "Percolating...",
    "Mulling...",    "Vibing...",
};
constexpr uint8_t kSpinnerTextCount =
    sizeof(kSpinnerTexts) / sizeof(kSpinnerTexts[0]);

void spinnerOpaCb(void *var, int32_t value) {
    lv_obj_set_style_text_opa(static_cast<lv_obj_t *>(var),
                              static_cast<lv_opa_t>(value), 0);
}

}  // namespace

lv_obj_t *makeChip(lv_obj_t *parent, const char *text) {
    lv_obj_t *chip = lv_label_create(parent);
    lv_label_set_text(chip, text);
    lv_obj_set_style_text_font(chip, &lv_font_montserrat_12, 0);
    lv_obj_set_style_bg_color(chip, lv_color_hex(0x808080), 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_30, 0);
    lv_obj_set_style_radius(chip, 9, 0);
    lv_obj_set_style_pad_hor(chip, 8, 0);
    lv_obj_set_style_pad_ver(chip, 4, 0);
    return chip;
}

void ScreenChrome::create(lv_obj_t *root) {
    /* Header: icone + relogio central + badge do plano. */
    lv_obj_t *icon = lv_image_create(root);
    lv_image_set_src(icon, &craudio_icon_0);
    lv_obj_set_pos(icon, 12, 10);

    clockLabel_ = lv_label_create(root);
    lv_label_set_text(clockLabel_, "--:--");
    lv_obj_set_style_text_font(clockLabel_, &lv_font_montserrat_28, 0);
    lv_obj_align(clockLabel_, LV_ALIGN_TOP_MID, 0, 10);

    planChip_ = makeChip(root, "");
    lv_obj_align(planChip_, LV_ALIGN_TOP_RIGHT, -12, 16);
    lv_obj_add_flag(planChip_, LV_OBJ_FLAG_HIDDEN);

    /* Rodape: asterisco pulsante + texto, centrados como um par. */
    lv_obj_t *footer = lv_obj_create(root);
    lv_obj_set_size(footer, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_set_style_pad_all(footer, 0, 0);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(footer, 6, 0);
    lv_obj_remove_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -6);

    spinnerLabel_ = lv_label_create(footer);
    lv_label_set_text(spinnerLabel_, "*");
    lv_obj_set_style_text_font(spinnerLabel_, &lv_font_montserrat_20, 0);
    /* O glifo "*" fica na zona alta da linha; desce para alinhar com o texto. */
    lv_obj_set_style_translate_y(spinnerLabel_, 4, 0);
    lv_obj_set_style_text_color(spinnerLabel_, lv_color_hex(kAccentHex), 0);

    statusTextLabel_ = lv_label_create(footer);
    lv_label_set_text(statusTextLabel_, kSpinnerTexts[0]);
    lv_obj_set_style_text_font(statusTextLabel_, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(statusTextLabel_, lv_color_hex(kAccentHex), 0);
}

void ScreenChrome::onShow() {
    if (clockTimer_ == nullptr) {
        /* 10 s: relogio vira o minuto com atraso maximo aceitavel. */
        clockTimer_ = lv_timer_create(clockTimerCb, 10000, this);
    }
    if (spinTextTimer_ == nullptr) {
        spinTextTimer_ = lv_timer_create(spinTimerCb, 2600, this);
    }

    /* Pulso continuo do asterisco (respiracao de opacidade). */
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, spinnerLabel_);
    lv_anim_set_exec_cb(&anim, spinnerOpaCb);
    lv_anim_set_values(&anim, LV_OPA_20, LV_OPA_COVER);
    lv_anim_set_duration(&anim, 650);
    lv_anim_set_playback_duration(&anim, 650);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&anim);

    refresh();
}

void ScreenChrome::onHide() {
    if (clockTimer_ != nullptr) {
        lv_timer_delete(clockTimer_);
        clockTimer_ = nullptr;
    }
    if (spinTextTimer_ != nullptr) {
        lv_timer_delete(spinTextTimer_);
        spinTextTimer_ = nullptr;
    }
    lv_anim_delete(spinnerLabel_, nullptr);
}

void ScreenChrome::refresh() {
    const AppState &state = AppState::instance();
    char buf[16];

    const int32_t nowMin = state.currentTimeMin();
    if (nowMin >= 0) {
        snprintf(buf, sizeof(buf), "%02ld:%02ld", static_cast<long>(nowMin / 60),
                 static_cast<long>(nowMin % 60));
        lv_label_set_text(clockLabel_, buf);
    }

    if (state.planName()[0] != '\0') {
        lv_label_set_text(planChip_, state.planName());
        lv_obj_remove_flag(planChip_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(planChip_, LV_ALIGN_TOP_RIGHT, -12, 16);
    }

    updateSpinnerText();
}

void ScreenChrome::updateSpinnerText() {
    const AppState &state = AppState::instance();
    const bool synced = state.bleConnected() && state.usage().syncedAtMs != 0;
    if (synced) {
        lv_label_set_text(statusTextLabel_, "Craudiometro"); /* fontes embutidas sao ASCII */
    } else {
        lv_label_set_text(statusTextLabel_, kSpinnerTexts[textIdx_]);
    }
}

void ScreenChrome::clockTimerCb(lv_timer_t *timer) {
    static_cast<ScreenChrome *>(lv_timer_get_user_data(timer))->refresh();
}

void ScreenChrome::spinTimerCb(lv_timer_t *timer) {
    auto *self = static_cast<ScreenChrome *>(lv_timer_get_user_data(timer));
    self->textIdx_ = (self->textIdx_ + 1) % kSpinnerTextCount;
    self->updateSpinnerText();
}
