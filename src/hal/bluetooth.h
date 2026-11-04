#pragma once
#include "pda/bluetooth.h"
namespace pda { namespace hal {
BluetoothRemote bluetoothRemote();
void bluetoothPoll();
void bluetoothReport();
} }
