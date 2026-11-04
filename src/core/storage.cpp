#include "pda/storage.h"
#include "pda/datetime.h"
#include <algorithm>
#include <cstring>
#include <set>

namespace pda {
uint32_t crc32(const uint8_t *data, size_t length) {
    uint32_t value = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        value ^= data[i];
        for (int bit = 0; bit < 8; ++bit) value = (value >> 1) ^ (0xEDB88320u & (0u - (value & 1u)));
    }
    return ~value;
}
namespace {
struct Writer {
    std::vector<uint8_t> data;
    bool ok = true;
    void u8(uint8_t v) { data.push_back(v); }
    void u16(uint16_t v) { u8(uint8_t(v)); u8(uint8_t(v >> 8)); }
    void u32(uint32_t v) { for (int i = 0; i < 4; ++i) u8(uint8_t(v >> (i * 8))); }
    void u64(uint64_t v) { for (int i = 0; i < 8; ++i) u8(uint8_t(v >> (i * 8))); }
    void text(const std::string &v, size_t max) {
        if (v.size() > max) { ok = false; return; }
        u32(uint32_t(v.size())); data.insert(data.end(), v.begin(), v.end());
    }
    void count(size_t n, size_t max) { if (n > max) ok = false; u32(uint32_t(n)); }
};
struct Reader {
    const std::vector<uint8_t> &data;
    size_t pos = 0;
    bool ok = true;
    explicit Reader(const std::vector<uint8_t> &d) : data(d) {}
    uint8_t u8() { if (pos >= data.size()) { ok = false; return 0; } return data[pos++]; }
    uint16_t u16() { uint16_t v = u8(); return uint16_t(v | uint16_t(u8()) << 8); }
    uint32_t u32() { uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= uint32_t(u8()) << (i * 8); return v; }
    uint64_t u64() { uint64_t v = 0; for (int i = 0; i < 8; ++i) v |= uint64_t(u8()) << (i * 8); return v; }
    bool boolean() { const auto v = u8(); if (v > 1) ok = false; return v == 1; }
    size_t count(size_t max) { const uint32_t n = u32(); if (n > max) { ok = false; return 0; } return n; }
    std::string text(size_t max) {
        const size_t n = count(max);
        if (!ok || pos > data.size() || n > data.size() - pos) { ok = false; return {}; }
        std::string result(reinterpret_cast<const char *>(data.data() + pos), n);
        pos += n;
        if (result.find('\0') != std::string::npos) ok = false;
        return result;
    }
};
bool validate(const State &s) {
    const auto &p = s.preferences; const auto &v = s.session;
    if (!s.nextId || p.brightness < 1 || p.brightness > 16 || p.dimSeconds > 3600 || p.offSeconds > 7200 ||
        (p.offSeconds && p.dimSeconds && p.offSeconds < p.dimSeconds) || p.timezoneMinutes < -720 || p.timezoneMinutes > 840 ||
        v.app < -1 || v.app > 12 || v.view > 9 || v.field > 7 || v.form > 10 || v.selected < 0 || v.selected > 300 ||
        v.row < 0 || v.row >= limits::rows || v.column < 0 || v.column >= limits::columns ||
        v.x < 0 || v.x >= limits::sketchWidth || v.y < 0 || v.y >= limits::sketchHeight ||
        dateKey(fromDateKey(v.day)) != v.day || v.caret > limits::noteBytes ||
        s.stopwatchMs > 315360000000ULL || s.timerMs > 31536000000ULL) return false;
    std::set<uint32_t> ids;
    auto id = [&](uint32_t n) { return n && n < s.nextId && ids.insert(n).second; };
    for (const auto &x : s.notes) if (!id(x.id)) return false;
    for (const auto &x : s.contacts) if (!id(x.id)) return false;
    for (const auto &x : s.appointments)
        if (!id(x.id) || !validCivil(fromUtc(x.start, 0)) ||
            (x.end && (x.end < x.start || !validCivil(fromUtc(x.end, 0)))) ||
            x.reminderMinutes < -1 || x.reminderMinutes > 10080) return false;
    for (const auto &x : s.sheets) {
        if (!id(x.id)) return false;
        std::set<int> positions;
        for (const auto &c : x.cells)
            if (c.row >= limits::rows || c.column >= limits::columns || c.text.empty() ||
                !positions.insert(c.row * limits::columns + c.column).second) return false;
    }
    for (const auto &x : s.sketches) if (!id(x.id) || x.pixels.size() != limits::sketchBytes) return false;
    if (s.games.selected >= games::count) return false;
    for (unsigned i = 0; i < games::count; ++i) if (!games::valid(games::Kind(i), s.games.slots[i])) return false;
    return true;
}
void writeState(Writer &w, const State &s) {
    w.u32(s.nextId);
    const auto &p = s.preferences;
    w.u8(p.brightness); w.u16(p.dimSeconds); w.u16(p.offSeconds); w.u16(uint16_t(p.timezoneMinutes));
    w.u8(p.composeSymbols); w.u8(p.sound);
    const auto &v = s.session;
    w.u8(uint8_t(v.app)); w.u8(v.view); w.u8(v.field); w.u8(v.form); w.u8(v.home); w.u32(v.document); w.u32(v.caret);
    w.u16(uint16_t(v.selected)); w.u16(uint16_t(v.row)); w.u16(uint16_t(v.column));
    w.u16(uint16_t(v.x)); w.u16(uint16_t(v.y)); w.u32(v.day); w.u8(v.editing); w.u8(v.pen); w.u8(v.eraser);
    w.count(v.draft.size(), 8); for (const auto &text : v.draft) w.text(text, limits::noteBytes);
    w.count(s.notes.size(), limits::notes);
    for (const auto &x : s.notes) { w.u32(x.id); w.text(x.title, 256); w.text(x.text, limits::noteBytes); }
    w.count(s.contacts.size(), limits::contacts);
    for (const auto &x : s.contacts) {
        w.u32(x.id); w.text(x.name, 256); w.text(x.phone, 256); w.text(x.email, 256); w.text(x.detail, 1024);
    }
    w.count(s.appointments.size(), limits::events);
    for (const auto &x : s.appointments) {
        w.u32(x.id); w.text(x.title, 256); w.text(x.detail, 2048); w.u64(uint64_t(x.start)); w.u64(uint64_t(x.end));
        w.u16(uint16_t(x.reminderMinutes)); w.u8(x.alerted);
        w.count(x.draft.size(), 5); for (const auto &field : x.draft) w.text(field, 2048);
    }
    w.count(s.sheets.size(), limits::sheets);
    for (const auto &x : s.sheets) {
        w.u32(x.id); w.text(x.title, 256); w.count(x.cells.size(), limits::cells);
        for (const auto &c : x.cells) { w.u16(c.row); w.u8(c.column); w.text(c.text, limits::cellBytes); }
    }
    w.count(s.sketches.size(), limits::sketches);
    for (const auto &x : s.sketches) {
        w.u32(x.id); w.text(x.title, 256); w.count(x.pixels.size(), limits::sketchBytes);
        w.data.insert(w.data.end(), x.pixels.begin(), x.pixels.end());
    }
    w.text(s.calculator, 256); w.u64(uint64_t(s.lastKnownUtc)); w.u64(s.stopwatchMs); w.u64(s.timerMs);
    w.u64(uint64_t(s.timerDeadline)); w.u8(s.stopwatchRunning); w.u8(s.timerRunning);
    w.u8(s.games.selected);
    for (const auto &g : s.games.slots) {
        w.u8(g.started); w.u8(g.cursor); w.u8(g.status); w.u16(g.level);
        w.u32(g.seed); w.u32(g.score); w.u32(g.best); w.u32(g.moves);
        w.text(g.board, 512); w.text(g.marks, 512); w.text(g.undoBoard, 512); w.text(g.undoMarks, 512);
        w.u32(g.undoSeed); w.u32(g.undoScore); w.u32(g.undoMoves); w.u8(g.undoCursor); w.u8(g.undoStatus);
    }
}
bool readState(Reader &r, State &s, uint32_t schema) {
    s.nextId = r.u32(); auto &p = s.preferences;
    p.brightness = r.u8(); p.dimSeconds = r.u16(); p.offSeconds = r.u16(); p.timezoneMinutes = int16_t(r.u16());
    p.composeSymbols = r.boolean(); p.sound = r.boolean();
    auto &v = s.session;
    v.app = int8_t(r.u8()); v.view = r.u8(); v.field = r.u8(); v.form = r.u8(); v.home = r.boolean(); v.document = r.u32(); v.caret = r.u32();
    v.selected = int16_t(r.u16()); v.row = int16_t(r.u16()); v.column = int16_t(r.u16());
    v.x = int16_t(r.u16()); v.y = int16_t(r.u16()); v.day = r.u32();
    v.editing = r.boolean(); v.pen = r.boolean(); v.eraser = r.boolean();
    size_t count = r.count(8); for (size_t i = 0; i < count; ++i) v.draft.push_back(r.text(limits::noteBytes));
    count = r.count(limits::notes);
    for (size_t i = 0; i < count && r.ok; ++i) {
        Note x; x.id = r.u32(); x.title = r.text(256); x.text = r.text(limits::noteBytes); s.notes.push_back(std::move(x));
    }
    count = r.count(limits::contacts);
    for (size_t i = 0; i < count && r.ok; ++i) {
        Contact x; x.id = r.u32(); x.name = r.text(256); x.phone = r.text(256); x.email = r.text(256); x.detail = r.text(1024);
        s.contacts.push_back(std::move(x));
    }
    count = r.count(limits::events);
    for (size_t i = 0; i < count && r.ok; ++i) {
        Appointment x; x.id = r.u32(); x.title = r.text(256); x.detail = r.text(2048);
        x.start = int64_t(r.u64()); x.end = int64_t(r.u64()); x.reminderMinutes = int16_t(r.u16()); x.alerted = r.boolean();
        const size_t drafts = r.count(5); for (size_t j = 0; j < drafts; ++j) x.draft.push_back(r.text(2048));
        s.appointments.push_back(std::move(x));
    }
    count = r.count(limits::sheets);
    for (size_t i = 0; i < count && r.ok; ++i) {
        Sheet x; x.id = r.u32(); x.title = r.text(256); const size_t cells = r.count(limits::cells);
        for (size_t j = 0; j < cells && r.ok; ++j) {
            Cell c; c.row = r.u16(); c.column = r.u8(); c.text = r.text(limits::cellBytes); x.cells.push_back(std::move(c));
        }
        s.sheets.push_back(std::move(x));
    }
    count = r.count(limits::sketches);
    for (size_t i = 0; i < count && r.ok; ++i) {
        Sketch x; x.id = r.u32(); x.title = r.text(256); const size_t n = r.count(limits::sketchBytes);
        if (n != limits::sketchBytes || r.pos > r.data.size() || n > r.data.size() - r.pos) { r.ok = false; break; }
        x.pixels.assign(r.data.begin() + r.pos, r.data.begin() + r.pos + n); r.pos += n;
        s.sketches.push_back(std::move(x));
    }
    s.calculator = r.text(256); s.lastKnownUtc = int64_t(r.u64()); s.stopwatchMs = r.u64(); s.timerMs = r.u64();
    s.timerDeadline = int64_t(r.u64()); s.stopwatchRunning = r.boolean(); s.timerRunning = r.boolean();
    if (schema >= 2) {
        s.games.selected = r.u8();
        const unsigned storedGames = schema >= 4 ? games::count : schema == 3 ? 5 : 4;
        for (unsigned i = 0; i < storedGames; ++i) {
            auto &g = s.games.slots[i];
            g.started = r.boolean(); g.cursor = r.u8(); g.status = r.u8(); g.level = r.u16();
            g.seed = r.u32(); g.score = r.u32(); g.best = r.u32(); g.moves = r.u32();
            const size_t maxText = schema >= 4 ? 512 : 81;
            g.board = r.text(maxText); g.marks = r.text(maxText); g.undoBoard = r.text(maxText); g.undoMarks = r.text(maxText);
            g.undoSeed = r.u32(); g.undoScore = r.u32(); g.undoMoves = r.u32(); g.undoCursor = r.u8(); g.undoStatus = r.u8();
            // The original dungeon left undoSeed at its unused default of 1.
            if(i==9 && g.board.size()==99 && g.undoBoard.empty() && g.undoSeed==1) g.undoSeed=0;
        }
    }
    return r.ok && validate(s);
}
}
bool encodeSnapshot(const State &state, uint64_t generation, std::vector<uint8_t> &data) {
    if (!validate(state)) return false;
    Writer payload; writeState(payload, state);
    if (!payload.ok || payload.data.size() > limits::snapshotBytes - 28) return false;
    Writer w;
    const char magic[] = "PDASNAP1";
    w.data.insert(w.data.end(), magic, magic + 8); w.u32(4); w.u64(generation); w.u32(uint32_t(payload.data.size()));
    w.data.insert(w.data.end(), payload.data.begin(), payload.data.end());
    const uint32_t checksum = crc32(w.data.data(), w.data.size()); w.u32(checksum);
    data = std::move(w.data);
    return true;
}
bool decodeSnapshot(const std::vector<uint8_t> &data, State &state, uint64_t &generation) {
    if (data.size() < 28 || data.size() > limits::snapshotBytes || memcmp(data.data(), "PDASNAP1", 8)) return false;
    Reader r(data); r.pos = data.size() - 4;
    if (r.u32() != crc32(data.data(), data.size() - 4)) return false;
    r.pos = 8;
    const uint32_t schema = r.u32();
    if (schema < 1 || schema > 4) return false;
    const uint64_t gen = r.u64(); const size_t size = r.u32();
    if (size != data.size() - 28 || !gen) return false;
    State candidate;
    if (!readState(r, candidate, schema) || r.pos != data.size() - 4) return false;
    state = std::move(candidate); generation = gen;
    return true;
}
LoadResult Store::begin(StorageBackend *backend, State &state) {
    backend_ = backend; active_ = -1; generation_ = 0; protected_ = false; error_.clear();
    if (!backend || !backend->ready()) { error_ = "Memoria indisponivel. Abra Ajustes."; return LoadResult::Unavailable; }
    bool invalid = false, any = false;
    for (int slot = 0; slot < 2; ++slot) {
        if (!backend->exists(slot)) continue;
        any = true;
        std::vector<uint8_t> bytes; State candidate; uint64_t generation = 0;
        if (!backend->read(slot, bytes) || !decodeSnapshot(bytes, candidate, generation)) { invalid = true; continue; }
        if (active_ < 0 || generation > generation_) { state = std::move(candidate); generation_ = generation; active_ = slot; }
    }
    if (active_ >= 0) {
        if (invalid) { error_ = "Recuperado da ultima copia valida."; return LoadResult::Recovered; }
        return LoadResult::Loaded;
    }
    if (any) { protected_ = true; error_ = "Dados invalidos. Memoria protegida contra sobrescrita."; return LoadResult::Corrupt; }
    return LoadResult::Fresh;
}
bool Store::ready() const { return backend_ && backend_->ready() && !protected_; }
const char *Store::description() const { return backend_ ? backend_->description() : "Sem backend"; }
bool Store::initialize(State &state) {
    if (!backend_ || !backend_->initialize()) { error_ = "Falha ao preparar memoria."; return false; }
    active_ = -1; generation_ = 0; protected_ = false; error_.clear();
    return save(state);
}
bool Store::save(const State &state) {
    if (!ready()) { if (error_.empty()) error_ = "Memoria indisponivel."; return false; }
    if (generation_ == UINT64_MAX) { error_ = "Limite de versoes atingido."; return false; }
    std::vector<uint8_t> bytes;
    if (!encodeSnapshot(state, generation_ + 1, bytes)) { error_ = "Limite de dados ou estado invalido."; return false; }
    const int target = active_ == 0 ? 1 : 0;
    if (!backend_->write(target, bytes)) { error_ = "Falha ao gravar. Dados continuam na RAM."; return false; }
    std::vector<uint8_t> verified;
    if (!backend_->read(target, verified) || bytes != verified) { error_ = "Falha ao verificar gravacao. Tente salvar."; return false; }
    active_ = target; ++generation_; error_.clear();
    return true;
}
}
