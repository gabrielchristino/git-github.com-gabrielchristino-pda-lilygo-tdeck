#pragma once
#include "pda/input.h"
#include <string>
#include <vector>

namespace pda {
enum class RemoteCommand : uint8_t { RightClick, Drag, ScrollUp, ScrollDown };
struct BluetoothPeer {
    std::string id, name;
    bool selected = false, connected = false;
};
struct BluetoothRemote {
    void (*enable)(bool) = nullptr;
    const char *(*status)() = nullptr;
    void (*input)(const InputEvent &) = nullptr;
    void (*command)(RemoteCommand) = nullptr;
    // Tap one raw HID key/chord, always followed by a complete release.
    void (*key)(uint8_t usage, uint8_t modifiers) = nullptr;
    bool (*connected)() = nullptr;
    std::vector<BluetoothPeer> (*peers)() = nullptr;
    bool (*selectPeer)(const std::string &id) = nullptr;
    bool (*pairNew)() = nullptr;
    bool (*renamePeer)(const std::string &id, const std::string &name) = nullptr;
    bool (*forgetPeer)(const std::string &id) = nullptr;
};
// Factory keyboard supplies characters, not physical key down/up events.
// HID usages assume the host's English (US) keyboard layout.
bool hidKey(uint8_t character, uint8_t &modifier, uint8_t &usage);
}
