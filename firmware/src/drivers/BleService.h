#pragma once

/* GATT server do Craudiometro (NimBLE).
 *
 * O daemon no macOS escreve o payload JSON na characteristic RX:
 *   { "s": 45, "sr": 120, "w": 28, "wr": 7200, "st": "allowed", "ok": true }
 * O parse acontece na task do NimBLE e o resultado e publicado no AppState —
 * nenhum acesso direto a UI a partir desta camada.
 *
 * UUIDs proprios do projeto (nao derivados de nenhum outro). */
class BleService {
public:
    static constexpr const char *kDeviceName = "craudiometro";
    static constexpr const char *kServiceUuid = "6a485858-14a2-4918-9841-d3c2d2474fbc";
    static constexpr const char *kRxCharUuid = "23aefc82-8cba-470b-ba8f-d1c54f9f0656";

    static void init();

    /* Apaga bonds e reinicia o advertising (usado pelo SettingsScreen). */
    static void resetBonds();
};
