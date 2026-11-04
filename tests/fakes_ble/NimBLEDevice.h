#pragma once
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>
struct ble_addr_t { uint8_t type; uint8_t val[6]; };
struct ble_gap_conn_desc {
    uint16_t conn_handle;
    ble_addr_t peer_id_addr;
    struct { bool encrypted,bonded; } sec_state;
};
constexpr int BLE_GAP_EVENT_CONNECT=0,BLE_GAP_EVENT_DISCONNECT=1,BLE_GAP_EVENT_ENC_CHANGE=2,BLE_GAP_EVENT_REPEAT_PAIRING=3,BLE_GAP_EVENT_PASSKEY_ACTION=4;
struct ble_gap_event {
    int type;
    struct { int status; uint16_t conn_handle; } connect;
    struct { int reason; ble_gap_conn_desc conn; } disconnect;
    struct { int status; uint16_t conn_handle; } enc_change;
    struct { uint16_t conn_handle; } repeat_pairing;
    struct { uint16_t conn_handle; struct { int action; } params; } passkey;
};
class NimBLEAddress {
    ble_addr_t address_={};
public:
    NimBLEAddress(ble_addr_t address) : address_(address) {}
    NimBLEAddress(uint64_t address,uint8_t type=0) { address_.type=type; std::memcpy(address_.val,&address,6); }
    const uint8_t *getNative() const { return address_.val; }
    uint8_t getType() const { return address_.type; }
    operator uint64_t() const { uint64_t value=0; std::memcpy(&value,address_.val,6); return value; }
    std::string toString() const {
        char text[18]; std::snprintf(text,sizeof(text),"%02x:%02x:%02x:%02x:%02x:%02x",address_.val[5],address_.val[4],address_.val[3],address_.val[2],address_.val[1],address_.val[0]); return text;
    }
};
class NimBLEServer;
class NimBLEServerCallbacks {
public:
    virtual ~NimBLEServerCallbacks() {}
    virtual void onConnect(NimBLEServer *,ble_gap_conn_desc *) {}
    virtual void onDisconnect(NimBLEServer *) {}
    virtual void onAuthenticationComplete(ble_gap_conn_desc *) {}
};
namespace bleTest {
extern std::vector<ble_addr_t> bonds;
extern std::vector<std::string> events;
extern ble_addr_t allowed;
extern bool whitelistFail;
}
inline bool sameAddress(const ble_addr_t &a,const ble_addr_t &b) { return a.type==b.type && !std::memcmp(a.val,b.val,6); }
inline int ble_gap_wl_set(const ble_addr_t *addresses,int count) {
    bleTest::events.push_back("filter");
    if(bleTest::whitelistFail || count!=1) return -1;
    bleTest::allowed=*addresses; return 0;
}
class NimBLEAdvertising {
public:
    bool filtered=false, advertising=false;
    int starts=0;
    void addServiceUUID(int) {}
    void setAppearance(int) {}
    void setScanResponse(bool) {}
    void setScanFilter(bool,bool filter) { filtered=filter; }
    void stop() { advertising=false; bleTest::events.push_back("stop"); }
    bool start() { ++starts; advertising=true; bleTest::events.push_back(filtered?"start-filtered":"start-open"); return true; }
};
class NimBLEServer {
    NimBLEAdvertising advertising_;
public:
    NimBLEServerCallbacks *callbacks=nullptr;
    int disconnects=0;
    void setCallbacks(NimBLEServerCallbacks *value,bool) { callbacks=value; }
    void advertiseOnDisconnect(bool) {}
    NimBLEAdvertising *getAdvertising() { return &advertising_; }
    void disconnect(uint16_t) { ++disconnects; bleTest::events.push_back("disconnect"); }
};
class NimBLEDevice {
public:
    static NimBLEServer server;
    static void init(const char *) {}
    static void setCustomGapHandler(int (*)(ble_gap_event *,void *)) {}
    static void setSecurityAuth(bool,bool,bool) {}
    static void setSecurityIOCap(int) {}
    static void startSecurity(uint16_t) { bleTest::events.push_back("security"); }
    static NimBLEServer *createServer() { return &server; }
    static int getNumBonds() { return int(bleTest::bonds.size()); }
    static NimBLEAddress getBondedAddress(int index) { return NimBLEAddress(bleTest::bonds.at(size_t(index))); }
    static bool isBonded(const NimBLEAddress &address) {
        ble_addr_t key={}; key.type=address.getType(); std::memcpy(key.val,address.getNative(),6);
        return std::any_of(bleTest::bonds.begin(),bleTest::bonds.end(),[&](const ble_addr_t &other){return sameAddress(key,other);});
    }
    static bool deleteBond(const NimBLEAddress &address) {
        auto it=std::find_if(bleTest::bonds.begin(),bleTest::bonds.end(),[&](const ble_addr_t &other){return uint64_t(NimBLEAddress(other))==uint64_t(address) && other.type==address.getType();});
        if(it==bleTest::bonds.end()) return false;
        bleTest::bonds.erase(it); return true;
    }
};
constexpr int BLE_HS_IO_NO_INPUT_OUTPUT=0,HID_KEYBOARD=1;
