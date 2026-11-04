#pragma once
#include "NimBLEDevice.h"
class NimBLECharacteristic {
public:
    std::vector<uint8_t> value;
    std::vector<std::vector<uint8_t>> reports;
    void setValue(const uint8_t *data,size_t size) { value.assign(data,data+size); }
    void setValue(const char *) {}
    void notify() { reports.push_back(value); bleTest::events.push_back("release/report"); }
};
class NimBLEHIDDevice {
    NimBLECharacteristic manufacturer_;
public:
    static NimBLECharacteristic keyboard,mouse;
    explicit NimBLEHIDDevice(NimBLEServer *) {}
    NimBLECharacteristic *inputReport(int id) { return id==1?&keyboard:&mouse; }
    void outputReport(int) {}
    NimBLECharacteristic *manufacturer() { return &manufacturer_; }
    void pnp(int,int,int,int) {}
    void hidInfo(int,int) {}
    void reportMap(uint8_t *,size_t) {}
    void startServices() {}
    NimBLEHIDDevice *hidService() { return this; }
    int getUUID() { return 1; }
};
