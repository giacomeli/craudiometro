#pragma once

#include <Arduino.h>

/* Dados de uso recebidos do daemon. Percentuais -1 = ainda sem dados. */
struct UsageData {
    int sessionPct = -1;      /* utilizacao da sessao 5h (0-100) */
    int32_t sessionResetMin = -1;
    int weekPct = -1;         /* utilizacao da semana (0-100) */
    int32_t weekResetMin = -1;
    char status[16] = "";     /* ex.: "allowed" */
    bool ok = false;          /* coleta no daemon bem-sucedida */
    uint32_t syncedAtMs = 0;  /* millis() locais do recebimento; 0 = nunca */
    int32_t timeMin = -1;     /* minutos desde a meia-noite (fuso do Mac) */
    char planName[16] = "";   /* ex.: "Max" */
};

/* Estado central do dispositivo.
 *
 * Escrita (publish*) pode vir de qualquer task (ex.: NimBLE); leitura e o
 * process() acontecem exclusivamente no loop do LVGL. O ScreenManager assina
 * as mudancas e repassa apenas para a tela ativa. */
class AppState {
public:
    static AppState &instance();

    /* Thread-safe: chamados da task BLE. */
    void publishUsage(const UsageData &data);
    void publishConnection(bool connected);

    /* Chamado a cada volta do loop LVGL; dispara o listener se algo mudou. */
    void process();

    using Listener = void (*)();
    void setListener(Listener listener);

    /* Leitura — apenas na thread da UI. */
    const UsageData &usage() const { return uiUsage_; }
    bool bleConnected() const { return uiConnected_; }

    /* Minutos desde o ultimo sync; -1 se nunca sincronizou. */
    int32_t syncAgeMin() const;

    /* Hora local corrente (minutos desde a meia-noite), avancada por millis()
     * a partir do ultimo `tm` recebido; -1 se nunca chegou hora. O relogio e
     * o plano sao retidos mesmo quando payloads de falha limpam o restante. */
    int32_t currentTimeMin() const;
    const char *planName() const { return uiPlan_; }

private:
    AppState() = default;

    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    UsageData sharedUsage_;      /* protegido por mux_ */
    bool sharedConnected_ = false;
    bool dirty_ = false;
    int32_t sharedClockBase_ = -1;   /* ultimo tm recebido */
    uint32_t sharedClockAtMs_ = 0;   /* millis() desse recebimento */
    char sharedPlan_[16] = "";

    UsageData uiUsage_;          /* copia estavel para a UI */
    bool uiConnected_ = false;
    int32_t uiClockBase_ = -1;
    uint32_t uiClockAtMs_ = 0;
    char uiPlan_[16] = "";
    Listener listener_ = nullptr;
};
