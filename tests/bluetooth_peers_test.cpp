#include "hal/bluetooth.h"
#include <NimBLEHIDDevice.h>
#include <Preferences.h>
#include <cassert>
#include <cstdio>
uint32_t bleTestMillis=1000;
FakeSerial Serial;
std::map<std::string,std::string> bleTestSettings;
bool bleTestWriteFail=false;
namespace bleTest {
std::vector<ble_addr_t> bonds;
std::vector<std::string> events;
ble_addr_t allowed={};
bool whitelistFail=false;
}
NimBLEServer NimBLEDevice::server;
NimBLECharacteristic NimBLEHIDDevice::keyboard,NimBLEHIDDevice::mouse;
static ble_addr_t addr(uint64_t value,uint8_t type=0) { ble_addr_t a={}; a.type=type; std::memcpy(a.val,&value,6); return a; }
static ble_gap_conn_desc current={};
static void connectPeer(ble_addr_t address,bool authenticate=true) {
    current={}; current.conn_handle=42; current.peer_id_addr=address;
    NimBLEDevice::server.callbacks->onConnect(&NimBLEDevice::server,&current);
    if(authenticate) {
        current.sec_state.encrypted=current.sec_state.bonded=true;
        NimBLEDevice::server.callbacks->onAuthenticationComplete(&current);
    }
}
static void disconnectPeer() { NimBLEDevice::server.callbacks->onDisconnect(&NimBLEDevice::server); }
static void poll() { bleTestMillis+=300; pda::hal::bluetoothPoll(); }
int main() {
    const auto pc=addr(0x112233445566ULL), phone=addr(0xaabbccddeeffULL,1), tablet=addr(0x123456789abcULL,1);
    bleTest::bonds={pc,phone};
    auto remote=pda::hal::bluetoothRemote();
    remote.enable(true); poll();
    auto list=remote.peers(); const auto pcId=list[0].id, phoneId=list[1].id;
    assert(list[0].selected && sameAddress(bleTest::allowed,pc));
    connectPeer(pc); assert(remote.connected());
    remote.command(pda::RemoteCommand::Drag);
    bleTest::events.clear();
    assert(remote.selectPeer(phoneId));
    assert(!remote.connected());
    auto stop=std::find(bleTest::events.begin(),bleTest::events.end(),"disconnect");
    auto release=std::find(bleTest::events.begin(),bleTest::events.end(),"release/report");
    assert(release<stop);
    const auto reports=NimBLEHIDDevice::keyboard.reports.size();
    remote.key(4,0); assert(NimBLEHIDDevice::keyboard.reports.size()==reports);
    const int starts=NimBLEDevice::server.getAdvertising()->starts;
    poll(); assert(NimBLEDevice::server.getAdvertising()->starts==starts);
    // Late encryption completion from the old host must not reopen the HID path.
    NimBLEDevice::server.callbacks->onAuthenticationComplete(&current); assert(!remote.connected());
    disconnectPeer(); poll();
    assert(NimBLEDevice::server.getAdvertising()->filtered && sameAddress(bleTest::allowed,phone));
    connectPeer(pc); assert(!remote.connected()); disconnectPeer(); poll();
    connectPeer(phone); assert(remote.connected());
    assert(remote.renamePeer(pcId,"Computador") && remote.renamePeer(phoneId,"Celular"));
    bleTestWriteFail=true;
    assert(!remote.selectPeer(pcId) && remote.connected()); bleTestWriteFail=false;
    remote.enable(false); disconnectPeer();
    std::swap(bleTest::bonds[0],bleTest::bonds[1]);
    remote.enable(true); poll();
    assert(sameAddress(bleTest::allowed,phone)); // Stable identity, not list position.
    assert(remote.peers()[0].name=="Celular");
    connectPeer(phone);
    assert(remote.pairNew()); disconnectPeer(); poll();
    assert(!NimBLEDevice::server.getAdvertising()->filtered);
    connectPeer(pc); assert(!remote.connected()); disconnectPeer(); poll();
    connectPeer(tablet,false);
    current.sec_state.encrypted=current.sec_state.bonded=true;
    NimBLEDevice::server.callbacks->onAuthenticationComplete(&current); poll();
    assert(remote.connected() && remote.peers().size()==2); // Keys can be persisted after the security callback.
    bleTest::bonds.push_back(tablet); poll();
    assert(remote.connected() && !remote.pairNew() && bleTest::bonds.size()==3);
    list=remote.peers(); assert(list.back().selected && list.back().connected);
    assert(remote.forgetPeer(list.back().id)); disconnectPeer(); poll();
    assert(bleTest::bonds.size()==2 && sameAddress(bleTest::allowed,phone));
    assert(remote.pairNew()); poll(); bleTestMillis+=120001; poll();
    assert(NimBLEDevice::server.getAdvertising()->filtered && sameAddress(bleTest::allowed,phone));
    bleTest::whitelistFail=true;
    assert(remote.selectPeer(pcId)); const int blocked=NimBLEDevice::server.getAdvertising()->starts;
    poll(); assert(NimBLEDevice::server.getAdvertising()->starts==blocked && !remote.connected());
    bleTest::whitelistFail=false; poll();
    assert(sameAddress(bleTest::allowed,pc));
    assert(!remote.selectPeer("unknown") && !remote.forgetPeer("unknown"));
    remote.enable(false);
    std::puts("BLE peers: persistent identity selection, private address types, reports released before switch, late callbacks, pairing isolation/capacity/timeout and closed filter on failure OK");
}
