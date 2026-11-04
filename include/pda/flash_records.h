#pragma once
#include <stdint.h>

namespace pda {
// Byte-addressable offsets keep immutable flash tables compact without relying
// on compiler-specific bitfields or unaligned 32-bit loads.
struct FlashOffset24 {
    uint8_t bytes[3];
    constexpr FlashOffset24(uint32_t value)
        : bytes{uint8_t(value), uint8_t(value >> 8), uint8_t(value >> 16)} {}
    constexpr operator uint32_t() const {
        return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]) << 16);
    }
};
static_assert(sizeof(FlashOffset24) == 3, "Flash offset must occupy exactly three bytes");
}
