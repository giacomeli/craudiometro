#include "DashboardScreen.h"

#include <cstdio>

#include "../assets/craudio_sprites.h"
#include "../core/AppState.h"

namespace {

/* Cor por faixa de utilizacao: verde < 60, ambar < 85, vermelho acima. */
lv_color_t colorForPct(int pct) {
    if (pct < 60) {
        return lv_palette_main(LV_PALETTE_GREEN);
    }
    if (pct < 85) {
        return lv_palette_main(LV_PALETTE_AMBER);
    }
    return lv_palette_main(LV_PALETTE_RED);
}

/* "6d 19h" / "3h 24m" / "45m" / "--" para minutos restantes. */
void fmtDuration(char *out, size_t outLen, int32_t minutes) {
    if (minutes < 0) {
        snprintf(out, outLen, "--");
    } else if (minutes >= 24 * 60) {
        snprintf(out, outLen, "%ldd %ldh", static_cast<long>(minutes / (24 * 60)),
                 static_cast<long>((minutes % (24 * 60)) / 60));
    } else if (minutes >= 60) {
        snprintf(out, outLen, "%ldh %02ldm", static_cast<long>(minutes / 60),
                 static_cast<long>(minutes % 60));
    } else {
        snprintf(out, outLen, "%ldm", static_cast<long>(minutes));
    }
}

/* Minutos restantes corrigidos pela idade do sync (contagem local). */
int32_t remainingMin(int32_t resetMin, int32_t syncAgeMin) {
    if (resetMin < 0 || syncAgeMin < 0) {
        return -1;
    }
    const int32_t remaining = resetMin - syncAgeMin;
    return remaining > 0 ? remaining : 0;
}

/* Cor de destaque do rodape: a mesma do corpo do Craudinho. */
const uint32_t kAccentHex = 0xCA7D6E;

/* Textos rotativos enquanto sincroniza, no espirito do Claude Code
 * (ASCII: as fontes Montserrat embutidas nao tem acentos). */
constexpr const char *kSpinnerTexts[] = {
    "Matutando...", "Assando...",    "Cozinhando...", "Maquinando...",
    "Ruminando...", "Tramando...",   "Fervilhando...", "Marinando...",
    "Devaneando...", "Fritando...",
};
constexpr uint8_t kSpinnerTextCount =
    sizeof(kSpinnerTexts) / sizeof(kSpinnerTexts[0]);

void spinnerOpaCb(void *var, int32_t value) {
    lv_obj_set_style_text_opa(static_cast<lv_obj_t *>(var),
                              static_cast<lv_opa_t>(value), 0);
}

/* Chip: label pequeno com fundo arredondado, neutro nos dois temas. */
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

}  // namespace

DashboardScreen::UsageCard DashboardScreen::makeCard(int y, const char *chipText) {
    lv_obj_t *card = lv_obj_create(root_);
    lv_obj_set_size(card, 216, 96);
    lv_obj_set_pos(card, 12, y);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    UsageCard result;
    result.pctLabel = lv_label_create(card);
    lv_label_set_text(result.pctLabel, "--");
    lv_obj_set_style_text_font(result.pctLabel, &lv_font_montserrat_28, 0);
    lv_obj_align(result.pctLabel, LV_ALIGN_TOP_LEFT, 0, -4);

    lv_obj_t *chip = makeChip(card, chipText);
    lv_obj_align(chip, LV_ALIGN_TOP_RIGHT, 0, 0);

    result.bar = lv_bar_create(card);
    lv_obj_set_size(result.bar, 192, 10);
    lv_obj_align(result.bar, LV_ALIGN_BOTTOM_LEFT, 0, -26);
    lv_bar_set_range(result.bar, 0, 100);

    result.resetLabel = lv_label_create(card);
    lv_label_set_text(result.resetLabel, "reinicia em --");
    lv_obj_set_style_text_font(result.resetLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_opa(result.resetLabel, LV_OPA_60, 0);
    lv_obj_align(result.resetLabel, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    return result;
}

void DashboardScreen::create() {
    initRoot();
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    /* Header: icone + relogio central + badge do plano. */
    lv_obj_t *icon = lv_image_create(root_);
    lv_image_set_src(icon, &craudio_icon_0);
    lv_obj_set_pos(icon, 12, 10);

    clockLabel_ = lv_label_create(root_);
    lv_label_set_text(clockLabel_, "--:--");
    lv_obj_set_style_text_font(clockLabel_, &lv_font_montserrat_28, 0);
    lv_obj_align(clockLabel_, LV_ALIGN_TOP_MID, 0, 10);

    planChip_ = makeChip(root_, "");
    lv_obj_align(planChip_, LV_ALIGN_TOP_RIGHT, -12, 16);
    lv_obj_add_flag(planChip_, LV_OBJ_FLAG_HIDDEN);

    /* Cards de uso. */
    sessionCard_ = makeCard(54, "sessao");
    weekCard_ = makeCard(158, "semana");

    /* Rodape: asterisco pulsante + texto, centrados como um par. */
    lv_obj_t *footer = lv_obj_create(root_);
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

void DashboardScreen::onShow() {
    if (refreshTimer_ == nullptr) {
        /* 10 s: relogio vira o minuto com atraso maximo aceitavel. */
        refreshTimer_ = lv_timer_create(timerCb, 10000, this);
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

void DashboardScreen::onHide() {
    if (refreshTimer_ != nullptr) {
        lv_timer_delete(refreshTimer_);
        refreshTimer_ = nullptr;
    }
    if (spinTextTimer_ != nullptr) {
        lv_timer_delete(spinTextTimer_);
        spinTextTimer_ = nullptr;
    }
    lv_anim_delete(spinnerLabel_, nullptr);
}

void DashboardScreen::onStateChanged() {
    refresh();
}

void DashboardScreen::refreshCard(const UsageCard &card, int pct,
                                  int32_t remaining) {
    if (pct < 0) {
        return;
    }
    char buf[40];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(card.pctLabel, buf);

    lv_bar_set_value(card.bar, pct, LV_ANIM_ON);
    lv_obj_set_style_bg_color(card.bar, colorForPct(pct), LV_PART_INDICATOR);

    char dur[24];
    fmtDuration(dur, sizeof(dur), remaining);
    snprintf(buf, sizeof(buf), "reinicia em %s", dur);
    lv_label_set_text(card.resetLabel, buf);
}

void DashboardScreen::refresh() {
    const AppState &state = AppState::instance();
    const UsageData &usage = state.usage();
    const int32_t age = state.syncAgeMin();
    char buf[48];

    /* Relogio. */
    const int32_t nowMin = state.currentTimeMin();
    if (nowMin >= 0) {
        snprintf(buf, sizeof(buf), "%02ld:%02ld", static_cast<long>(nowMin / 60),
                 static_cast<long>(nowMin % 60));
        lv_label_set_text(clockLabel_, buf);
    }

    /* Badge do plano. */
    if (state.planName()[0] != '\0') {
        lv_label_set_text(planChip_, state.planName());
        lv_obj_remove_flag(planChip_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(planChip_, LV_ALIGN_TOP_RIGHT, -12, 16);
    }

    refreshCard(sessionCard_, usage.sessionPct,
                remainingMin(usage.sessionResetMin, age));
    refreshCard(weekCard_, usage.weekPct, remainingMin(usage.weekResetMin, age));

    updateSpinnerText();
}

void DashboardScreen::updateSpinnerText() {
    const AppState &state = AppState::instance();
    const bool synced =
        state.bleConnected() && state.usage().syncedAtMs != 0;
    if (synced) {
        lv_label_set_text(statusTextLabel_, "craudiometro");
    } else {
        lv_label_set_text(statusTextLabel_, kSpinnerTexts[textIdx_]);
    }
}

void DashboardScreen::timerCb(lv_timer_t *timer) {
    static_cast<DashboardScreen *>(lv_timer_get_user_data(timer))->refresh();
}

void DashboardScreen::spinTimerCb(lv_timer_t *timer) {
    auto *self = static_cast<DashboardScreen *>(lv_timer_get_user_data(timer));
    self->textIdx_ = (self->textIdx_ + 1) % kSpinnerTextCount;
    self->updateSpinnerText();
}
