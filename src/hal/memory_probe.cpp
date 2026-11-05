#include "memory_probe.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include <limits.h>

namespace pda { namespace hal {
namespace {
constexpr uint32_t internalCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
constexpr uint32_t externalCaps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
constexpr uint32_t reserveLimit = 5 * 1024 * 1024;
constexpr uint32_t externalMargin = 1536 * 1024;
constexpr uint32_t internalMargin = 64 * 1024;
uint32_t *buffer = nullptr;
uint32_t bufferBytes = 0, cursor = 0, expiresAt = 0;
enum class Phase { Empty, Write, Verify, Held, Failed };
Phase phase = Phase::Empty;
uint32_t windowAt = 0, previousLoop = 0, largestGap = 0, lastSample = 0, samples = 0;
uint32_t internalLow = UINT_MAX, externalLow = UINT_MAX;
uint32_t internalBlockLow = UINT_MAX, externalBlockLow = UINT_MAX;
const char *phaseName() {
    switch (phase) {
    case Phase::Write: return "WRITE";
    case Phase::Verify: return "VERIFY";
    case Phase::Held: return "PASS";
    case Phase::Failed: return "FAIL";
    default: return "EMPTY";
    }
}
uint32_t pattern(uint32_t index) { return (index * 2654435761u) ^ 0xA5C35A7Du; }
void sample() {
    internalLow = std::min(internalLow, uint32_t(heap_caps_get_free_size(internalCaps)));
    externalLow = std::min(externalLow, uint32_t(heap_caps_get_free_size(externalCaps)));
    internalBlockLow = std::min(internalBlockLow, uint32_t(heap_caps_get_largest_free_block(internalCaps)));
    externalBlockLow = std::min(externalBlockLow, uint32_t(heap_caps_get_largest_free_block(externalCaps)));
    ++samples;
}
}
void memoryResetWindow() {
    windowAt = previousLoop = millis(); largestGap = samples = 0;
    internalLow = externalLow = internalBlockLow = externalBlockLow = UINT_MAX;
    sample();
}
void memoryReport() {
    sample();
    Serial.printf("MEM INTERNAL TOTAL=%u FREE=%u LARGEST=%u BOOT_LOW=%u WINDOW_LOW=%u BLOCK_LOW=%u\n",
        unsigned(heap_caps_get_total_size(internalCaps)), unsigned(heap_caps_get_free_size(internalCaps)),
        unsigned(heap_caps_get_largest_free_block(internalCaps)), unsigned(heap_caps_get_minimum_free_size(internalCaps)),
        unsigned(internalLow), unsigned(internalBlockLow));
    Serial.printf("MEM PSRAM TOTAL=%u FREE=%u LARGEST=%u BOOT_LOW=%u WINDOW_LOW=%u BLOCK_LOW=%u\n",
        unsigned(heap_caps_get_total_size(externalCaps)), unsigned(heap_caps_get_free_size(externalCaps)),
        unsigned(heap_caps_get_largest_free_block(externalCaps)), unsigned(heap_caps_get_minimum_free_size(externalCaps)),
        unsigned(externalLow), unsigned(externalBlockLow));
    Serial.printf("MEM PROBE BYTES=%u STATE=%s WINDOW_MS=%u SAMPLES=%u LOOP_MAX_MS=%u STACK_FREE_BYTES=%u\n",
        unsigned(bufferBytes), phaseName(), unsigned(millis() - windowAt), unsigned(samples), unsigned(largestGap),
        unsigned(uxTaskGetStackHighWaterMark(nullptr)));
    Serial.println("MEM DONE");
}
void memoryRelease() {
    heap_caps_free(buffer); buffer = nullptr; bufferBytes = cursor = expiresAt = 0; phase = Phase::Empty;
}
bool memoryReserve(uint32_t bytes) {
    // Explicit PSRAM allocation; never fall back to scarce internal/DMA memory.
    if (buffer || !bytes || bytes % 4 || bytes > reserveLimit ||
        heap_caps_get_free_size(externalCaps) < size_t(bytes) + externalMargin ||
        heap_caps_get_free_size(internalCaps) < internalMargin) return false;
    buffer = static_cast<uint32_t *>(heap_caps_malloc(bytes, externalCaps));
    if (!buffer) return false;
    bufferBytes = bytes; cursor = 0; phase = Phase::Write; expiresAt = millis() + 120000;
    return true;
}
void memoryVerify() {
    if (!buffer || phase != Phase::Held) { Serial.println("MEM VERIFY ERROR"); return; }
    cursor = 0; phase = Phase::Verify; Serial.println("MEM VERIFY START");
}
void memoryCheck() {
    Serial.printf("MEM CHECK OK=%d\n", heap_caps_check_integrity_all(true));
}
void memoryProbeTick() {
    const uint32_t now = millis();
    if (previousLoop) largestGap = std::max(largestGap, uint32_t(now - previousLoop));
    previousLoop = now;
    if (uint32_t(now - lastSample) >= 100) { lastSample = now; sample(); }
    if (buffer && int32_t(now - expiresAt) >= 0) {
        memoryRelease(); Serial.println("MEM RELEASE TIMEOUT"); return;
    }
    if (phase != Phase::Write && phase != Phase::Verify) return;
    const uint32_t end = std::min(bufferBytes / 4, cursor + 8192);
    for (; cursor < end; ++cursor) {
        if (phase == Phase::Write) buffer[cursor] = pattern(cursor);
        else if (buffer[cursor] != pattern(cursor)) {
            phase = Phase::Failed;
            Serial.printf("MEM BUFFER FAIL WORD=%u\n", unsigned(cursor)); return;
        }
    }
    if (cursor == bufferBytes / 4) {
        cursor = 0;
        if (phase == Phase::Write) phase = Phase::Verify;
        else { phase = Phase::Held; Serial.printf("MEM BUFFER PASS BYTES=%u\n", unsigned(bufferBytes)); }
    }
}
} }
