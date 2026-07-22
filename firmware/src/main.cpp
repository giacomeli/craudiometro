/* Craudiômetro — dashboard de uso do Claude Code.
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

/* Comandos pela Serial (usados por tools/screenshot.py):
 *   shot            - transmite um screenshot da tela atual (protocolo SNAP)
 *   screen <nome>   - troca para a tela do registro ("menu" abre o menu) */
void handleSerialCommands() {
    static char buf[48];
    static size_t len = 0;
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') {
            buf[len] = '\0';
            len = 0;
            if (strcmp(buf, "shot") == 0) {
                DisplayDriver::requestSnapshot();
            } else if (strncmp(buf, "screen ", 7) == 0) {
                ScreenManager::instance().showByName(buf + 7);
            }
        } else if (len < sizeof(buf) - 1) {
            buf[len++] = c;
        }
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    Serial.println("Craudiômetro: iniciando");

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

    Serial.println("Craudiômetro: UI e BLE prontos");
}

void loop() {
    lv_timer_handler();
    AppState::instance().process();
    ScreenManager::instance().tick();
    handleSerialCommands();
    delay(5);
}
