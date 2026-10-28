#include "EncoderDriver.h"

#include <Arduino.h>
#include <driver/pcnt.h>

namespace {

/* Fiacao definida na spec. */
constexpr int kPinClk = 32;  /* quadratura A */
constexpr int kPinDt = 33;   /* quadratura B */
constexpr int kPinSw = 25;   /* botao */

constexpr pcnt_unit_t kPcntUnit = PCNT_UNIT_0;
constexpr int kCountsPerDetent = 4;
constexpr uint32_t kDebounceMs = 20;
constexpr uint32_t kLongPressMs = 800;

lv_indev_t *g_indev = nullptr;
EncoderDriver::LongPressHandler g_longPress = nullptr;
volatile uint32_t g_lastActivity = 0;

/* Estado da rotacao. */
int16_t g_lastCount = 0;
int32_t g_rawAccum = 0;

/* Estado do botao. */
enum class ButtonPhase { Idle, Pressed, LongFired };
ButtonPhase g_phase = ButtonPhase::Idle;
uint32_t g_pressStart = 0;
uint32_t g_lastEdge = 0;
bool g_debounced = false;    /* estado estavel: true = pressionado */
bool g_clickPending = false; /* clique curto aguardando entrega ao LVGL */
bool g_clickDelivered = false;

void setupPcnt() {
    /* Quadratura x4: dois canais espelhados, um por fase. */
    pcnt_config_t ch0 = {};
    ch0.unit = kPcntUnit;
    ch0.channel = PCNT_CHANNEL_0;
    ch0.pulse_gpio_num = kPinClk;
    ch0.ctrl_gpio_num = kPinDt;
    ch0.pos_mode = PCNT_COUNT_DEC;
    ch0.neg_mode = PCNT_COUNT_INC;
    ch0.lctrl_mode = PCNT_MODE_KEEP;
    ch0.hctrl_mode = PCNT_MODE_REVERSE;
    ch0.counter_h_lim = 32767;
    ch0.counter_l_lim = -32768;
    pcnt_unit_config(&ch0);

    pcnt_config_t ch1 = ch0;
    ch1.channel = PCNT_CHANNEL_1;
    ch1.pulse_gpio_num = kPinDt;
    ch1.ctrl_gpio_num = kPinClk;
    ch1.pos_mode = PCNT_COUNT_INC;
    ch1.neg_mode = PCNT_COUNT_DEC;
    pcnt_unit_config(&ch1);

    /* Filtro de glitch no maximo (~12,5 us @ APB 80 MHz). */
    pcnt_set_filter_value(kPcntUnit, 1023);
    pcnt_filter_enable(kPcntUnit);

    pcnt_counter_pause(kPcntUnit);
    pcnt_counter_clear(kPcntUnit);
    pcnt_counter_resume(kPcntUnit);
}

int readDetentSteps() {
    int16_t count = 0;
    pcnt_get_counter_value(kPcntUnit, &count);
    const int32_t delta = static_cast<int16_t>(count - g_lastCount);
    g_lastCount = count;

    g_rawAccum += delta;
    const int steps = g_rawAccum / kCountsPerDetent;
    g_rawAccum -= steps * kCountsPerDetent;
    return steps;
}

/* Debounce + fases do botao. Retorna o estado a reportar ao LVGL. */
lv_indev_state_t updateButton() {
    const uint32_t now = millis();
    const bool raw = digitalRead(kPinSw) == LOW;

    if (raw != g_debounced && (now - g_lastEdge) >= kDebounceMs) {
        g_debounced = raw;
        g_lastEdge = now;

        if (g_debounced) {
            g_phase = ButtonPhase::Pressed;
            g_pressStart = now;
            g_lastActivity = now;
        } else {
            if (g_phase == ButtonPhase::Pressed) {
                g_clickPending = true; /* clique curto: entrega adiada */
                g_clickDelivered = false;
            }
            g_phase = ButtonPhase::Idle;
        }
    } else if (raw == g_debounced) {
        g_lastEdge = now;
    }

    if (g_phase == ButtonPhase::Pressed && (now - g_pressStart) >= kLongPressMs) {
        g_phase = ButtonPhase::LongFired;
        g_lastActivity = now;
        if (g_longPress != nullptr) {
            g_longPress();
        }
    }

    /* Entrega do clique curto em dois ciclos: PRESSED e depois RELEASED. */
    if (g_clickPending) {
        if (!g_clickDelivered) {
            g_clickDelivered = true;
            return LV_INDEV_STATE_PRESSED;
        }
        g_clickPending = false;
        return LV_INDEV_STATE_RELEASED;
    }
    return LV_INDEV_STATE_RELEASED;
}

}  // namespace

void EncoderDriver::init() {
    pinMode(kPinClk, INPUT);  /* pull-ups de 10k no proprio modulo KY-040 */
    pinMode(kPinDt, INPUT);
    pinMode(kPinSw, INPUT_PULLUP);

    setupPcnt();

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
    const int steps = readDetentSteps();
    if (steps != 0) {
        g_lastActivity = millis();
    }
    data->enc_diff = static_cast<int16_t>(steps);
    data->state = updateButton();
}
