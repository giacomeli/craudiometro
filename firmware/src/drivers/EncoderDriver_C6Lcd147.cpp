#include "EncoderDriver.h"

#include <Arduino.h>

namespace {

/* Waveshare ESP32-C6-LCD-1.47-H: sem encoder rotativo onboard, so o botao
 * BOOT (GPIO9, ativo-baixo com pull-up). Sem CLK/DT nao ha sinal de
 * rotacao, mas o LVGL LV_INDEV_TYPE_ENCODER depende de enc_diff para
 * navegar entre widgets/opcoes (inclusive dentro de dropdown/roller) e de
 * um pulso PRESSED/RELEASED para confirmar. Este driver sintetiza os dois
 * a partir da cadencia de clique de um unico botao, para que nenhuma
 * Screen precise saber que esta placa nao tem encoder de verdade:
 *
 * - Clique simples (aperta e solta, sem um segundo clique em seguida):
 *   vira enc_diff = +1 ("girar um detent" para frente). So da pra avancar,
 *   nunca voltar — limitacao aceita de um input de botao unico.
 * - Clique duplo (dois cliques dentro de kDoubleClickWindowMs): vira o
 *   pulso PRESSED->RELEASED que representa o clique do encoder (abre
 *   dropdown, confirma selecao, ativa o widget focado).
 * - Clique longo (>= kLongPressMs, contado a partir do primeiro toque em
 *   qualquer um dos dois cliques): NAO vai para o LVGL — dispara o
 *   handler global (mesmo papel do clique longo do KY-040 no outro
 *   board).
 *
 * Trade-off: um clique simples so e entregue depois que a janela de
 * clique duplo expira sem um segundo toque (~350 ms de latencia), para
 * nao confundir "avancar" com o primeiro toque de um clique duplo. */
constexpr int kPinSw = 9;

constexpr uint32_t kDebounceMs = 20;
constexpr uint32_t kLongPressMs = 800;
constexpr uint32_t kDoubleClickWindowMs = 350;

lv_indev_t *g_indev = nullptr;
EncoderDriver::LongPressHandler g_longPress = nullptr;
volatile uint32_t g_lastActivity = 0;

enum class ButtonPhase {
    Idle,
    FirstPress,
    WaitSecondPress,
    SecondPress,
    LongFired,
};
ButtonPhase g_phase = ButtonPhase::Idle;
uint32_t g_pressStart = 0;
uint32_t g_windowStart = 0;
uint32_t g_lastEdge = 0;
bool g_debounced = false; /* estado estavel: true = pressionado */

/* Clique simples resolvido: entregue como um unico enc_diff = +1. */
bool g_singleClickPending = false;

/* Clique duplo resolvido: entregue como PRESSED e depois RELEASED,
 * mesmo padrao de entrega adiada do driver do KY-040. */
bool g_doubleClickPending = false;
bool g_doubleClickDelivered = false;

void resolveLongPress(uint32_t now) {
    g_phase = ButtonPhase::LongFired;
    g_lastActivity = now;
    if (g_longPress != nullptr) {
        g_longPress();
    }
}

void updatePhase() {
    const uint32_t now = millis();
    const bool raw = digitalRead(kPinSw) == LOW;

    if (raw != g_debounced && (now - g_lastEdge) >= kDebounceMs) {
        g_debounced = raw;
        g_lastEdge = now;

        if (g_debounced) {
            /* Toque comecando: primeiro clique ou segundo clique da janela. */
            g_lastActivity = now;
            if (g_phase == ButtonPhase::WaitSecondPress) {
                g_phase = ButtonPhase::SecondPress;
                g_pressStart = now;
            } else {
                g_phase = ButtonPhase::FirstPress;
                g_pressStart = now;
            }
        } else {
            /* Soltura. */
            if (g_phase == ButtonPhase::FirstPress) {
                g_phase = ButtonPhase::WaitSecondPress;
                g_windowStart = now;
            } else if (g_phase == ButtonPhase::SecondPress) {
                g_doubleClickPending = true;
                g_doubleClickDelivered = false;
                g_phase = ButtonPhase::Idle;
            } else if (g_phase == ButtonPhase::LongFired) {
                g_phase = ButtonPhase::Idle;
            }
        }
    } else if (raw == g_debounced) {
        g_lastEdge = now;
    }

    if ((g_phase == ButtonPhase::FirstPress || g_phase == ButtonPhase::SecondPress) &&
        (now - g_pressStart) >= kLongPressMs) {
        resolveLongPress(now);
    }

    if (g_phase == ButtonPhase::WaitSecondPress &&
        (now - g_windowStart) >= kDoubleClickWindowMs) {
        g_singleClickPending = true;
        g_phase = ButtonPhase::Idle;
    }
}

}  // namespace

void EncoderDriver::init() {
    pinMode(kPinSw, INPUT_PULLUP);

    g_indev = lv_indev_create();
    lv_indev_set_type(g_indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(g_indev, readCb);
}

lv_indev_t *EncoderDriver::indev() {
    return g_indev;
}

void EncoderDriver::setLongPressHandler(LongPressHandler handler) {
    g_longPress = handler;
}

uint32_t EncoderDriver::lastActivityMs() {
    return g_lastActivity;
}

void EncoderDriver::readCb(lv_indev_t *, lv_indev_data_t *data) {
    updatePhase();

    data->enc_diff = 0;
    if (g_singleClickPending) {
        g_singleClickPending = false;
        data->enc_diff = 1;
    }

    data->state = LV_INDEV_STATE_RELEASED;
    if (g_doubleClickPending) {
        if (!g_doubleClickDelivered) {
            g_doubleClickDelivered = true;
            data->state = LV_INDEV_STATE_PRESSED;
        } else {
            g_doubleClickPending = false;
        }
    }
}
