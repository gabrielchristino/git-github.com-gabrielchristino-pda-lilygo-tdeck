#include "bluetooth.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <atomic>
#include <Preferences.h>
#include "nimble/nimble/host/include/host/ble_store.h"

namespace pda { namespace hal {
namespace {
NimBLEServer *server = nullptr;
NimBLEHIDDevice *hid = nullptr;
NimBLECharacteristic *keyboard = nullptr, *mouse = nullptr;
std::atomic<uint16_t> connection{0xffff};
std::atomic<bool> active{false}, linked{false}, authenticated{false}, advertiseAgain{false};
std::atomic<bool> switching{false}, pairing{false}, admitted{false}, pairedPending{false};
portMUX_TYPE peerMux=portMUX_INITIALIZER_UNLOCKED;
uint64_t targetId=0, connectedId=0;
::Preferences peerSettings;
bool settingsReady=false;
uint32_t pairingAt=0, lastAdvertise=0;
std::string targetName, advertiseError;
bool dragging = false;
struct BtTrace { uint32_t at; int value; uint16_t handle; uint8_t kind; uint64_t id; };
BtTrace traces[24]={};
uint32_t traceCount=0;
void trace(uint8_t kind,uint16_t handle,int value,uint64_t id=0) {
    const uint32_t at=millis();
    portENTER_CRITICAL(&peerMux);
    traces[traceCount++%24]={at,value,handle,kind,id};
    portEXIT_CRITICAL(&peerMux);
}
int traceGap(ble_gap_event *event,void *) {
    switch(event->type) {
    case BLE_GAP_EVENT_CONNECT: trace(10,event->connect.conn_handle,event->connect.status); break;
    case BLE_GAP_EVENT_DISCONNECT: trace(11,event->disconnect.conn.conn_handle,event->disconnect.reason); break;
    case BLE_GAP_EVENT_ENC_CHANGE: trace(12,event->enc_change.conn_handle,event->enc_change.status); break;
    case BLE_GAP_EVENT_REPEAT_PAIRING: trace(13,event->repeat_pairing.conn_handle,0); break;
    case BLE_GAP_EVENT_PASSKEY_ACTION: trace(14,event->passkey.conn_handle,event->passkey.params.action); break;
    }
    return 0;
}
uint64_t identity(const NimBLEAddress &address) {
    return (uint64_t(address)&0xffffffffffffULL)|(uint64_t(address.getType()&1)<<48);
}
NimBLEAddress addressOf(uint64_t id) { return NimBLEAddress(id&0xffffffffffffULL,uint8_t(id>>48)); }
uint64_t target() { portENTER_CRITICAL(&peerMux); const auto id=targetId; portEXIT_CRITICAL(&peerMux); return id; }
uint64_t peer() { portENTER_CRITICAL(&peerMux); const auto id=connectedId; portEXIT_CRITICAL(&peerMux); return id; }
void setTarget(uint64_t id) { portENTER_CRITICAL(&peerMux); targetId=id; portEXIT_CRITICAL(&peerMux); }
void setPeer(uint64_t id) { portENTER_CRITICAL(&peerMux); connectedId=id; portEXIT_CRITICAL(&peerMux); }
std::string peerKey(uint64_t id) { return std::to_string(unsigned(id>>48))+":"+addressOf(id).toString(); }
std::string labelKey(uint64_t id) {
    std::string key="n"; for(char c:peerKey(id)) if(c!=':') key+=c; return key;
}
std::string peerName(uint64_t id) {
    const auto key=labelKey(id);
    if(settingsReady && peerSettings.isKey(key.c_str())) { const String name=peerSettings.getString(key.c_str(),""); if(name.length()) return name.c_str(); }
    return "Aparelho "+addressOf(id).toString().substr(12);
}
std::vector<NimBLEAddress> bondedPeers() {
    // Read one bounded snapshot; bonding can finish on the host task while the
    // UI is polling, so count+index calls would race changes to the bond list.
    ble_addr_t addresses[3]={}; int count=0;
    std::vector<NimBLEAddress> result;
    if(ble_store_util_bonded_peers(addresses,&count,3)==0)
        for(int i=0;i<count;++i) result.emplace_back(addresses[i]);
    return result;
}
uint64_t findPeer(const std::string &id) {
    if(!server) return 0;
    for(const auto &address:bondedPeers()) { const auto key=identity(address); if(peerKey(key)==id) return key; }
    return 0;
}
uint64_t firstPeer() { const auto peers=bondedPeers(); return peers.empty()?0:identity(peers[0]); }
bool remember(uint64_t id) {
    return settingsReady && peerSettings.putString("target",id?peerKey(id).c_str():"")>0;
}
// Separate report IDs: standard 8-byte keyboard and 4-byte relative mouse.
uint8_t reportMap[] = {
    0x05,0x01, 0x09,0x06, 0xA1,0x01, 0x85,0x01,
    0x05,0x07, 0x19,0xE0, 0x29,0xE7, 0x15,0x00, 0x25,0x01,
    0x75,0x01, 0x95,0x08, 0x81,0x02,
    0x75,0x08, 0x95,0x01, 0x81,0x01,
    0x05,0x08, 0x19,0x01, 0x29,0x05, 0x75,0x01, 0x95,0x05, 0x91,0x02,
    0x75,0x03, 0x95,0x01, 0x91,0x01,
    0x05,0x07, 0x19,0x00, 0x29,0x65, 0x15,0x00, 0x25,0x65,
    0x75,0x08, 0x95,0x06, 0x81,0x00, 0xC0,
    0x05,0x01, 0x09,0x02, 0xA1,0x01, 0x85,0x02, 0x09,0x01, 0xA1,0x00,
    0x05,0x09, 0x19,0x01, 0x29,0x03, 0x15,0x00, 0x25,0x01,
    0x95,0x03, 0x75,0x01, 0x81,0x02, 0x95,0x01, 0x75,0x05, 0x81,0x01,
    0x05,0x01, 0x09,0x30, 0x09,0x31, 0x09,0x38,
    0x15,0x81, 0x25,0x7F, 0x75,0x08, 0x95,0x03, 0x81,0x06, 0xC0,0xC0
};
bool ready() { return active && linked && authenticated && !switching; }
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *s, ble_gap_conn_desc *desc) override {
        connection = desc->conn_handle;
        authenticated = false; admitted=false; linked = true;
        const NimBLEAddress address(desc->peer_id_addr);
        const auto id=identity(address); setPeer(id);
        trace(1,desc->conn_handle,int(bool(active))|int(bool(switching))<<1|int(bool(pairing))<<2|int(NimBLEDevice::isBonded(address))<<3,id);
        // Filtering at the controller prevents a previously paired host from
        // stealing the connection; recheck identity before allowing HID too.
        if (!active || switching || (pairing?NimBLEDevice::isBonded(address):id!=target())) {
            s->disconnect(desc->conn_handle); return;
        }
        admitted=true;
        NimBLEDevice::startSecurity(desc->conn_handle);
    }
    void onDisconnect(NimBLEServer *) override {
        trace(3,connection,0,peer());
        linked = false; authenticated = false; admitted=false; setPeer(0); advertiseAgain = true;
    }
    void onAuthenticationComplete(ble_gap_conn_desc *desc) override {
        if(desc->conn_handle!=connection || !linked) return;
        const auto id=identity(NimBLEAddress(desc->peer_id_addr));
        trace(2,desc->conn_handle,int(desc->sec_state.encrypted)|int(desc->sec_state.bonded)<<1|int(bool(active))<<2|
            int(bool(switching))<<3|int(bool(admitted))<<4|int(bool(pairing))<<5|int(id==target())<<6,id);
        authenticated = desc->sec_state.encrypted && desc->sec_state.bonded && active && !switching && admitted && (pairing || id==target());
        if (!authenticated) server->disconnect(desc->conn_handle);
        else { setPeer(id); if(pairing) pairedPending=true; }
    }
} serverCallbacks;
void mouseReport(uint8_t buttons, int x = 0, int y = 0, int wheel = 0) {
    if (!ready()) return;
    auto clamp = [](int n) { return uint8_t(int8_t(n < -127 ? -127 : n > 127 ? 127 : n)); };
    uint8_t report[] = {buttons, clamp(x), clamp(y), clamp(wheel)};
    mouse->setValue(report, sizeof(report)); mouse->notify();
}
void releaseReports() {
    if(ready()) {
        mouseReport(0);
        uint8_t release[8]={}; keyboard->setValue(release,sizeof(release)); keyboard->notify();
        delay(10);
    }
    dragging=false;
}
void changeDestination(uint64_t id,bool newPair) {
    releaseReports(); switching=true; pairing=newPair; pairedPending=false;
    setTarget(id); targetName=id?peerName(id):""; pairingAt=millis(); advertiseError.clear();
    server->getAdvertising()->stop();
    if(linked) server->disconnect(connection);
    advertiseAgain=true;
}
std::vector<BluetoothPeer> peers() {
    std::vector<BluetoothPeer> result;
    if(!server) return result;
    const auto selected=target(), connected=peer();
    for(const auto &address:bondedPeers()) {
        const auto id=identity(address); BluetoothPeer p;
        p.id=peerKey(id); p.name=peerName(id); p.selected=!pairing && selected==id;
        p.connected=ready() && connected==id; result.push_back(p);
    }
    return result;
}
bool selectPeer(const std::string &key) {
    const auto id=findPeer(key); if(!id || !active || !remember(id)) return false;
    if(!pairing && target()==id && ready() && peer()==id) return true;
    changeDestination(id,false); return true;
}
bool pairNew() {
    if(!server || !active || NimBLEDevice::getNumBonds()>=3) return false;
    // Keep the previous target in NVS so a cancelled/expired pairing returns to it.
    changeDestination(0,true); return true;
}
bool renamePeer(const std::string &key,const std::string &name) {
    const auto id=findPeer(key);
    if(!id || !settingsReady || name.empty() || name.size()>24 || name.find('\0')!=std::string::npos) return false;
    if(!peerSettings.putString(labelKey(id).c_str(),name.c_str())) return false;
    if(target()==id) targetName=name;
    return true;
}
bool forgetPeer(const std::string &key) {
    const auto id=findPeer(key); if(!id || !active) return false;
    releaseReports(); switching=true; server->getAdvertising()->stop();
    if(linked) server->disconnect(connection);
    if(!NimBLEDevice::deleteBond(addressOf(id))) { advertiseAgain=true; return false; }
    if(settingsReady) peerSettings.remove(labelKey(id).c_str());
    auto selected=target(); if(selected==id || !selected) selected=firstPeer();
    const bool saved=remember(selected);
    changeDestination(selected,!selected);
    return saved;
}
void enable(bool on) {
    if (!on) {
        releaseReports();
        active = false; dragging = false;
        if (server) {
            server->getAdvertising()->stop();
            if (linked) server->disconnect(connection);
        }
        return;
    }
    active = true; dragging = false;
    if (!server) {
        NimBLEDevice::init("T-Deck PDA");
        NimBLEDevice::setCustomGapHandler(traceGap);
        NimBLEDevice::setSecurityAuth(true, false, true);
        NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
        server = NimBLEDevice::createServer(); server->setCallbacks(&serverCallbacks, false);
        server->advertiseOnDisconnect(false);
        hid = new NimBLEHIDDevice(server);
        keyboard = hid->inputReport(1); hid->outputReport(1);
        mouse = hid->inputReport(2);
        hid->manufacturer()->setValue("T-Deck PDA");
        hid->pnp(0x02, 0, 0, 0x0100);
        hid->hidInfo(0, 1); hid->reportMap(reportMap, sizeof(reportMap));
        hid->startServices();
        auto *advertising = server->getAdvertising();
        advertising->setAppearance(HID_KEYBOARD);
        advertising->addServiceUUID(hid->hidService()->getUUID());
        advertising->setScanResponse(true);
        settingsReady=peerSettings.begin("pda-bt",false);
    }
    const uint64_t selected=settingsReady && peerSettings.isKey("target")?findPeer(peerSettings.getString("target","").c_str()):0;
    const auto next=selected?selected:firstPeer();
    if(next && !selected) remember(next);
    changeDestination(next,!next && NimBLEDevice::getNumBonds()==0);
}
const char *status() {
    if (!active) return "Bluetooth desligado";
    if(!advertiseError.empty()) return advertiseError.c_str();
    if(switching) return "Trocando destino...";
    if(pairing) return linked?"Pareando novo...":"Parear novo: procure T-Deck PDA";
    static std::string text;
    text=(ready()?(dragging?"Arrastando: ":"Conectado: "):linked?"Pareando: ":"Aguardando: ")+targetName;
    return text.c_str();
}
void key(uint8_t usage, uint8_t modifier) {
    if (!ready()) return;
    uint8_t report[8] = {modifier, 0, usage, 0, 0, 0, 0, 0};
    keyboard->setValue(report, sizeof(report)); keyboard->notify();
    delay(8);
    uint8_t release[8] = {};
    keyboard->setValue(release, sizeof(release)); keyboard->notify();
}
void input(const InputEvent &event) {
    if (!ready()) return;
    if (event.action == Action::Move) { mouseReport(dragging ? 1 : 0, event.x * 3, event.y * 3); return; }
    if (event.action == Action::Activate && !event.character) {
        if (dragging) { dragging = false; mouseReport(0); }
        else { mouseReport(1); delay(8); mouseReport(0); }
        return;
    }
    uint8_t modifier, usage;
    if (!hidKey(event.character, modifier, usage)) return;
    key(usage, modifier);
}
void command(RemoteCommand cmd) {
    if (!ready()) return;
    switch (cmd) {
    case RemoteCommand::RightClick:
        mouseReport((dragging ? 1 : 0) | 2); delay(8); mouseReport(dragging ? 1 : 0); break;
    case RemoteCommand::Drag: dragging = !dragging; mouseReport(dragging ? 1 : 0); break;
    case RemoteCommand::ScrollUp: mouseReport(dragging ? 1 : 0, 0, 0, 1); break;
    case RemoteCommand::ScrollDown: mouseReport(dragging ? 1 : 0, 0, 0, -1); break;
    }
}
}
BluetoothRemote bluetoothRemote() {
    BluetoothRemote result; result.enable = enable; result.status = status;
    result.input = input; result.command = command;
    result.key = key; result.connected = ready;
    result.peers=peers; result.selectPeer=selectPeer; result.pairNew=pairNew;
    result.renamePeer=renamePeer; result.forgetPeer=forgetPeer;
    return result;
}
void bluetoothPoll() {
    if(!active) {
        if(linked) server->disconnect(connection);
        return;
    }
    if(pairedPending && ready()) {
        const auto id=peer();
        if(NimBLEDevice::isBonded(addressOf(id))) {
            pairedPending=false; setTarget(id); targetName=peerName(id); pairing=false;
            if(!remember(id)) advertiseError="Destino salvo apenas nesta sessão";
        }
    }
    if(pairing && !linked && uint32_t(millis()-pairingAt)>=120000) {
        const auto saved=settingsReady?findPeer(peerSettings.getString("target","").c_str()):0;
        const auto next=saved?saved:firstPeer();
        if(next) changeDestination(next,false);
        else { pairingAt=millis(); } // First pairing has no previous target to restore.
    }
    if(advertiseAgain && !linked && uint32_t(millis()-lastAdvertise)>=250) {
        advertiseAgain=false; lastAdvertise=millis(); dragging=false;
        auto *advertising=server->getAdvertising(); advertising->stop();
        bool configured=true;
        if(pairing) advertising->setScanFilter(false,false);
        else {
            const auto id=target(); const auto address=addressOf(id);
            ble_addr_t allowed={}; allowed.type=address.getType(); memcpy(allowed.val,address.getNative(),6);
            configured=id && NimBLEDevice::isBonded(address) && ble_gap_wl_set(&allowed,1)==0;
            advertising->setScanFilter(false,true);
        }
        // On failure keep the filter closed; never fall back to accepting any host.
        switching=false;
        if(!configured || !advertising->start()) {
            switching=true; advertiseAgain=true; advertiseError="Falha Bluetooth; tentando novamente";
        } else advertiseError.clear();
    }
}
void bluetoothReport() {
    BtTrace copy[24]; uint32_t count=0;
    portENTER_CRITICAL(&peerMux);
    count=traceCount<24?traceCount:24;
    const uint32_t first=traceCount-count;
    for(uint32_t i=0;i<count;++i) copy[i]=traces[(first+i)%24];
    portEXIT_CRITICAL(&peerMux);
    Serial.printf("BT POLICY ACTIVE=%d LINK=%d AUTH=%d SWITCH=%d PAIR=%d TARGET=%s\n",bool(active),bool(linked),bool(authenticated),bool(switching),bool(pairing),target()?peerKey(target()).c_str():"-");
    for(uint32_t i=0;i<count;++i) {
        const auto &event=copy[i];
        Serial.printf("BT EVENT MS=%lu KIND=%u HANDLE=%u VALUE=%d ID=%s\n",(unsigned long)event.at,unsigned(event.kind),unsigned(event.handle),event.value,event.id?peerKey(event.id).c_str():"-");
    }
    Serial.printf("BT LOG COUNT=%u\n",unsigned(count));
}
} }
