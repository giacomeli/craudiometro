#include "BleService.h"

#include <ArduinoJson.h>
#include <NimBLEDevice.h>

#include "../core/AppState.h"

namespace {

void parsePayload(const std::string &payload) {
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        return; /* payload invalido: ignora, mantem ultimo estado */
    }

    UsageData data;
    data.sessionPct = doc["s"] | -1;
    data.sessionResetMin = doc["sr"] | -1;
    data.weekPct = doc["w"] | -1;
    data.weekResetMin = doc["wr"] | -1;
    data.ok = doc["ok"] | false;
    data.timeMin = doc["tm"] | -1;
    const char *status = doc["st"] | "";
    snprintf(data.status, sizeof(data.status), "%s", status);
    const char *plan = doc["pl"] | "";
    snprintf(data.planName, sizeof(data.planName), "%s", plan);

    AppState::instance().publishUsage(data);
}

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *, NimBLEConnInfo &) override {
        AppState::instance().publishConnection(true);
    }

    void onDisconnect(NimBLEServer *server, NimBLEConnInfo &, int) override {
        AppState::instance().publishConnection(false);
        server->startAdvertising();
    }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &) override {
        parsePayload(characteristic->getValue());
    }
};

ServerCallbacks g_serverCallbacks;
RxCallbacks g_rxCallbacks;

}  // namespace

void BleService::init() {
    NimBLEDevice::init(kDeviceName);

    NimBLEServer *server = NimBLEDevice::createServer();
    server->setCallbacks(&g_serverCallbacks);

    NimBLEService *service = server->createService(kServiceUuid);
    NimBLECharacteristic *rx = service->createCharacteristic(
        kRxCharUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    rx->setCallbacks(&g_rxCallbacks);
    service->start();

    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(kServiceUuid);
    adv->setName(kDeviceName);
    /* UUID de 128 bits + nome nao cabem nos 31 bytes do pacote principal:
     * o scan response carrega o nome. */
    adv->enableScanResponse(true);
    adv->start();
}

void BleService::resetBonds() {
    NimBLEDevice::deleteAllBonds();
    NimBLEServer *server = NimBLEDevice::getServer();
    if (server != nullptr) {
        /* Derruba conexoes ativas para forcar novo pareamento limpo. */
        for (uint16_t handle : server->getPeerDevices()) {
            server->disconnect(handle);
        }
        server->startAdvertising();
    }
}
