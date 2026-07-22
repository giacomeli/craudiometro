/* Craudiometro — dashboard de uso do Claude Code.
 * ESP32 DevKit + GMT020-02 (ST7789V) + encoder KY-040. */

#include <Arduino.h>
#include <lvgl.h>

#include "core/AppState.h"
#include "core/ScreenManager.h"
#include "core/Settings.h"
#include "drivers/BleService.h"
#include "drivers/DisplayDriver.h"
#include "drivers/EncoderDriver.h"
#include "screens/DashboardScreen.h"
#include "screens/MascotScreen.h"
#include "screens/SettingsScreen.h"

namespace {

DashboardScreen g_dashboard;
MascotScreen g_mascot;
SettingsScreen g_settings;

}  // namespace

void setup() {
    Serial.begin(115200);
    Serial.println("craudiometro: iniciando");

    Settings::instance().begin();
    DisplayDriver::init();

    /* Tema aplicado na criacao dos widgets; trocar exige reiniciar. */
    const bool dark = Settings::instance().theme() == Settings::Theme::Dark;
    lv_theme_default_init(lv_display_get_default(),
                          lv_palette_main(LV_PALETTE_ORANGE),
                          lv_palette_main(LV_PALETTE_TEAL), dark,
                          &lv_font_montserrat_14);

    EncoderDriver::init();

    /* A primeira tela registrada e a inicial; o menu global e gerado deste
     * registro automaticamente. */
    ScreenManager &manager = ScreenManager::instance();
    manager.registerScreen(&g_dashboard);
    manager.registerScreen(&g_mascot);
    manager.registerScreen(&g_settings);
    manager.init();

    BleService::init();

    Serial.println("craudiometro: UI e BLE prontos");
}

void loop() {
    lv_timer_handler();
    AppState::instance().process();
    ScreenManager::instance().tick();
    delay(5);
}
