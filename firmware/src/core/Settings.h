#pragma once

#include <Arduino.h>
#include <Preferences.h>

/* Preferencias persistidas em NVS (namespace "craudio"). */
class Settings {
public:
    enum class Theme : uint8_t { Dark = 0, Light = 1 };

    /* Indices do timeout de tela: 0 = desligado, 1/5/15 minutos. */
    static constexpr uint32_t kTimeoutMinutes[] = {0, 1, 5, 15};
    static constexpr uint8_t kTimeoutOptionCount = 4;

    static Settings &instance();

    void begin();  /* carrega da NVS; chamar uma vez no setup */

    uint8_t screenTimeoutIdx() const { return timeoutIdx_; }
    uint32_t screenTimeoutMs() const { return kTimeoutMinutes[timeoutIdx_] * 60000U; }
    Theme theme() const { return theme_; }

    void setScreenTimeoutIdx(uint8_t idx);
    void setTheme(Theme theme);

private:
    Settings() = default;

    Preferences prefs_;
    uint8_t timeoutIdx_ = 2; /* default: 5 min */
    Theme theme_ = Theme::Dark;
};
