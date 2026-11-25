// ============================================================
//  ble_ctrl.h  —  BLE + Classic BT control
// ============================================================
#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "config.h"

// Forward declare callback type
typedef std::function<void(const String&)> CmdCallback;

class BleCtrl : public BLEServerCallbacks, public BLECharacteristicCallbacks {
public:
    BleCtrl() {}

    void begin(CmdCallback cb) {
        _cb = cb;
        BLEDevice::init(BLE_DEVICE_NAME);
        _server = BLEDevice::createServer();
        _server->setCallbacks(this);

        BLEService* svc = _server->createService(BLE_SERVICE_UUID);

        // Command characteristic (write)
        _charCmd = svc->createCharacteristic(
            BLE_CHAR_CMD_UUID,
            BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
        );
        _charCmd->setCallbacks(this);

        // Status characteristic (notify)
        _charStatus = svc->createCharacteristic(
            BLE_CHAR_STATUS_UUID,
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
        );
        _charStatus->addDescriptor(new BLE2902());

        svc->start();

        BLEAdvertising* adv = BLEDevice::getAdvertising();
        adv->addServiceUUID(BLE_SERVICE_UUID);
        adv->setScanResponse(true);
        adv->setMinPreferred(0x06);
        BLEDevice::startAdvertising();

        Serial.println("[BLE] advertising as " BLE_DEVICE_NAME);
    }

    bool isConnected() const { return _connected; }

    void sendStatus(const String& json) {
        if (_connected && _charStatus) {
            _charStatus->setValue(json.c_str());
            _charStatus->notify();
        }
    }

    // BLEServerCallbacks
    void onConnect(BLEServer*) override {
        _connected = true;
        Serial.println("[BLE] client connected");
    }
    void onDisconnect(BLEServer*) override {
        _connected = false;
        Serial.println("[BLE] client disconnected — restarting advertising");
        BLEDevice::startAdvertising();
    }

    // BLECharacteristicCallbacks
    void onWrite(BLECharacteristic* c) override {
        std::string val = c->getValue();
        if (!val.empty() && _cb) {
            _cb(String(val.c_str()));
        }
    }

private:
    BLEServer*         _server     = nullptr;
    BLECharacteristic* _charCmd    = nullptr;
    BLECharacteristic* _charStatus = nullptr;
    bool               _connected  = false;
    CmdCallback        _cb;
};
