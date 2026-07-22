#include "Settings.h"

constexpr uint32_t Settings::kTimeoutMinutes[];

namespace {
constexpr const char *kNamespace = "craudio";
constexpr const char *kKeyTimeout = "timeout";
constexpr const char *kKeyTheme = "theme";
}  // namespace

Settings &Settings::instance() {
    static Settings settings;
    return settings;
}

void Settings::begin() {
    prefs_.begin(kNamespace, false);
    timeoutIdx_ = prefs_.getUChar(kKeyTimeout, timeoutIdx_);
    if (timeoutIdx_ >= kTimeoutOptionCount) {
        timeoutIdx_ = 2;
    }
    theme_ = static_cast<Theme>(prefs_.getUChar(kKeyTheme, static_cast<uint8_t>(theme_)));
}

void Settings::setScreenTimeoutIdx(uint8_t idx) {
    if (idx >= kTimeoutOptionCount) {
        return;
    }
    timeoutIdx_ = idx;
    prefs_.putUChar(kKeyTimeout, idx);
}

void Settings::setTheme(Theme theme) {
    theme_ = theme;
    prefs_.putUChar(kKeyTheme, static_cast<uint8_t>(theme));
}
