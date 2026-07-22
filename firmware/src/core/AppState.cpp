#include "AppState.h"

AppState &AppState::instance() {
    static AppState state;
    return state;
}

void AppState::publishUsage(const UsageData &data) {
    portENTER_CRITICAL(&mux_);
    sharedUsage_ = data;
    sharedUsage_.syncedAtMs = millis();
    /* Relogio e plano sobrevivem a payloads de falha (campos ausentes). */
    if (data.timeMin >= 0) {
        sharedClockBase_ = data.timeMin;
        sharedClockAtMs_ = millis();
    }
    if (data.planName[0] != '\0') {
        snprintf(sharedPlan_, sizeof(sharedPlan_), "%s", data.planName);
    }
    dirty_ = true;
    portEXIT_CRITICAL(&mux_);
}

void AppState::publishConnection(bool connected) {
    portENTER_CRITICAL(&mux_);
    sharedConnected_ = connected;
    dirty_ = true;
    portEXIT_CRITICAL(&mux_);
}

void AppState::process() {
    bool changed = false;
    portENTER_CRITICAL(&mux_);
    if (dirty_) {
        uiUsage_ = sharedUsage_;
        uiConnected_ = sharedConnected_;
        uiClockBase_ = sharedClockBase_;
        uiClockAtMs_ = sharedClockAtMs_;
        snprintf(uiPlan_, sizeof(uiPlan_), "%s", sharedPlan_);
        dirty_ = false;
        changed = true;
    }
    portEXIT_CRITICAL(&mux_);

    if (changed && listener_ != nullptr) {
        listener_();
    }
}

void AppState::setListener(Listener listener) {
    listener_ = listener;
}

int32_t AppState::syncAgeMin() const {
    if (uiUsage_.syncedAtMs == 0) {
        return -1;
    }
    return static_cast<int32_t>((millis() - uiUsage_.syncedAtMs) / 60000U);
}

int32_t AppState::currentTimeMin() const {
    if (uiClockBase_ < 0) {
        return -1;
    }
    const int32_t elapsed = static_cast<int32_t>((millis() - uiClockAtMs_) / 60000U);
    return (uiClockBase_ + elapsed) % (24 * 60);
}
