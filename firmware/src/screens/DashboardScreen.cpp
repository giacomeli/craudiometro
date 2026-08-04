#include "DashboardScreen.h"

#include <cstdio>

#include "../core/AppState.h"
#include "../drivers/DisplayDriver.h"

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

}  // namespace

DashboardScreen::UsageCard DashboardScreen::makeCard(int y, const char *chipText) {
    /* Margem de 12px de cada lado da tela; o cartao ocupa o resto da largura. */
    const int32_t cardWidth = DisplayDriver::kWidth - 24;

    lv_obj_t *card = lv_obj_create(root_);
    lv_obj_set_size(card, cardWidth, 96);
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
    /* Padding interno de 12px de cada lado do cartao (pad_all acima). */
    lv_obj_set_size(result.bar, cardWidth - 24, 10);
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

    chrome_.create(root_);
    sessionCard_ = makeCard(54, "sessao");
    weekCard_ = makeCard(158, "semana");
}

void DashboardScreen::onShow() {
    chrome_.onShow();
    if (refreshTimer_ == nullptr) {
        /* Countdowns avancam mesmo sem dados novos. */
        refreshTimer_ = lv_timer_create(timerCb, 10000, this);
    }
    refresh();
}

void DashboardScreen::onHide() {
    chrome_.onHide();
    if (refreshTimer_ != nullptr) {
        lv_timer_delete(refreshTimer_);
        refreshTimer_ = nullptr;
    }
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

    refreshCard(sessionCard_, usage.sessionPct,
                remainingMin(usage.sessionResetMin, age));
    refreshCard(weekCard_, usage.weekPct, remainingMin(usage.weekResetMin, age));

    chrome_.refresh();
}

void DashboardScreen::timerCb(lv_timer_t *timer) {
    static_cast<DashboardScreen *>(lv_timer_get_user_data(timer))->refresh();
}
