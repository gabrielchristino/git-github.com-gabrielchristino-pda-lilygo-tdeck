#pragma once
#include "NimBLEDevice.h"
inline int ble_store_util_bonded_peers(ble_addr_t *addresses,int *count,int maximum) {
    if(int(bleTest::bonds.size())>maximum) return -1;
    *count=int(bleTest::bonds.size());
    std::copy(bleTest::bonds.begin(),bleTest::bonds.end(),addresses); return 0;
}
