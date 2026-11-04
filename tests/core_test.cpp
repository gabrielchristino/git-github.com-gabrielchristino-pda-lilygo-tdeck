#include "pda/storage.h"
#include "pda/system.h"
#include "pda/datetime.h"
#include "pda/expression.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
using namespace pda;

class MemoryStorage : public StorageBackend {
public:
    std::vector<uint8_t> slots[2];
    bool online = true, fail = false, truncate = false;
    bool ready() const override { return online; }
    bool exists(int i) const override { return !slots[i].empty(); }
    bool read(int i, std::vector<uint8_t> &data) override { data = slots[i]; return !data.empty(); }
    bool write(int i, const std::vector<uint8_t> &data) override {
        if (fail) return false;
        slots[i] = data;
        if (truncate) { slots[i].resize(data.size() / 2); return false; }
        return true;
    }
    bool initialize() override { slots[0].clear(); slots[1].clear(); online = true; return true; }
    const char *description() const override { return "memory"; }
};
class FakePlatform : public Platform {
public:
    int64_t epoch = 0;
    int alarms = 0;
    int64_t utc() const override { return epoch; }
    bool setUtc(int64_t value) override { epoch = value; return true; }
    void alert() override { ++alarms; }
};
static void equals(const Value &v, double number) { assert(v.kind == ValueKind::Number); assert(std::abs(v.number - number) < 1e-8); }
static void expressionTests() {
    Evaluator calc;
    equals(calc.expression("2+3*4"), 14); equals(calc.expression("-(2+3)/2"), -2.5);
    equals(calc.expression("1e2 + .5"), 100.5);
    assert(calc.expression("1/0").error == "#DIV/0!");
    assert(calc.expression("1e309").error == "#NUM!");
    assert(calc.expression("1+)").error == "#SYNTAX!");
    assert(calc.expression("2 garbage").error == "#SYNTAX!");
    assert(calc.expression("2**4").kind == ValueKind::Error);
    assert(calc.expression(std::string(260, '1')).error == "#LIMIT!");
    Sheet s; s.id = 1;
    assert(s.set(0, 0, "2")); assert(s.set(1, 0, "3")); assert(s.set(2, 0, "texto"));
    assert(s.set(0, 1, "=A1+A2*2"));
    equals(Evaluator(&s).cell(0, 1), 8);
    equals(Evaluator(&s).expression("=SUM(A1:A5;B1;2)"), 15);
    equals(Evaluator(&s).expression("=AVG(A1:A5)"), 2.5);
    equals(Evaluator(&s).expression("=COUNT(A1:A5)"), 2);
    equals(Evaluator(&s).expression("=MIN(A1:A5)"), 2);
    equals(Evaluator(&s).expression("=MAX(A1:A5)"), 3);
    equals(Evaluator(&s).expression("=SUM(A5:A1)"), 5);
    equals(Evaluator(&s).expression("=SUM(Z100:Z100)"), 0);
    assert(Evaluator(&s).expression("=AVG(Z100:Z100)").error == "#EMPTY!");
    assert(Evaluator(&s).expression("=A3+2").error == "#VALUE!");
    assert(Evaluator(&s).expression("=AA1").error == "#REF!");
    assert(Evaluator(&s).expression("=A0").error == "#REF!");
    assert(Evaluator(&s).expression("=A101").error == "#REF!");
    assert(Evaluator(&s).expression("=FOO(A1)").error == "#NAME?");
    s.set(0, 0, "5"); equals(Evaluator(&s).cell(0, 1), 11);
    s.set(0, 0, "=B1"); assert(Evaluator(&s).cell(0, 1).error == "#CYCLE!");
    assert(Evaluator(&s).expression("=COUNT(A1:B1)").error == "#CYCLE!");
    s.set(0, 0, "=A1"); assert(Evaluator(&s).cell(0, 0).error == "#CYCLE!");
    s.cells.clear();
    for (int i = 0; i < 40; ++i) s.set(i, 0, "=A" + std::to_string(i + 2));
    assert(Evaluator(&s).cell(0, 0).error == "#LIMIT!");
    assert(!s.set(100, 0, "1")); assert(!s.set(0, 26, "1"));
    s.cells.clear(); s.set(0, 0, "1"); s.set(0, 0, ""); assert(s.cells.empty());
    for (int i = 0; i < 512; ++i) assert(s.set(i / 26, i % 26, "1"));
    assert(!s.set(99, 25, "2")); assert(s.set(0, 0, "2"));
}
static State sample() {
    State s;
    Note note; note.id = s.allocateId(); note.title = "Anotações"; note.text = "Olá\nTexto com ç e ã"; s.notes.push_back(note);
    Contact c; c.id = s.allocateId(); c.name = "Pessoa"; c.phone = "+55 11 1234"; c.email = "a@example.com"; s.contacts.push_back(c);
    Appointment a; a.id = s.allocateId(); a.title = "Compromisso"; a.start = 1792425600; a.end = a.start + 3600; a.reminderMinutes = 5; s.appointments.push_back(a);
    Sketch d; d.id = s.allocateId(); d.title = "Rascunho"; d.line(0, 0, 287, 127, true); s.sketches.push_back(d);
    Sheet sheet; sheet.id = s.allocateId(); sheet.title = "Contas"; sheet.set(0, 0, "5"); sheet.set(0, 1, "=A1*2"); s.sheets.push_back(sheet);
    s.session.app = 2; s.session.view = 1; s.session.document = note.id; s.session.editing = true;
    s.session.caret = 7; s.session.draft.push_back("Edicao incompleta");
    return s;
}
static void persistenceTests() {
    State s = sample(); std::vector<uint8_t> bytes; assert(encodeSnapshot(s, 42, bytes));
    State restored; uint64_t gen = 0; assert(decodeSnapshot(bytes, restored, gen));
    assert(gen == 42 && restored.notes[0].text == s.notes[0].text);
    assert(restored.session.editing && restored.session.caret == 7 && restored.session.draft == s.session.draft);
    assert(restored.sketches[0].pixels == s.sketches[0].pixels);
    equals(Evaluator(&restored.sheets[0]).cell(0, 1), 10);
    for (size_t index : {size_t(10), size_t(15), size_t(25), bytes.size() - 1}) {
        auto broken = bytes; broken[index] ^= 1; assert(!decodeSnapshot(broken, restored, gen));
    }
    auto truncated = bytes; truncated.resize(bytes.size() - 1); assert(!decodeSnapshot(truncated, restored, gen));
    MemoryStorage memory; Store store; State state;
    assert(store.begin(&memory, state) == LoadResult::Fresh); state = sample(); assert(store.save(state));
    state.notes[0].text = "segunda versao"; assert(store.save(state));
    state.notes[0].text = "nao terminou"; memory.truncate = true; assert(!store.save(state));
    Store reboot; State recovery;
    assert(reboot.begin(&memory, recovery) == LoadResult::Recovered);
    assert(recovery.notes[0].text == "segunda versao");
    memory.truncate = false; recovery.notes[0].text = "retomada"; assert(reboot.save(recovery));
    Store finalBoot; State finalState; assert(finalBoot.begin(&memory, finalState) == LoadResult::Loaded);
    assert(finalState.notes[0].text == "retomada");
    memory.fail = true; assert(!finalBoot.save(finalState));
    memory.slots[0] = {1, 2, 3}; memory.slots[1] = {4, 5};
    Store corrupt; State empty; assert(corrupt.begin(&memory, empty) == LoadResult::Corrupt);
    assert(!corrupt.ready() && !corrupt.save(empty));
    memory.fail = false; assert(corrupt.initialize(empty)); assert(corrupt.ready());
    auto invalid = sample(); invalid.notes.push_back(invalid.notes[0]); assert(!encodeSnapshot(invalid, 1, bytes));
}
static void dateAndTimerTests() {
    CivilTime d; assert(parseCivil("29/02/2028 12:34", d));
    assert(!parseCivil("29/02/2026 12:34", d)); assert(!parseCivil("01/01/2026 25:00", d));
    assert(!parseCivil("01/01/2026 10:00x", d));
    assert(formatCivil(toUtc(d, -180), -180) == "29/02/2028 12:34");
    d.hour = d.minute = 0; assert(dateKey(addDays(d, 1)) == 20280301);
    assert(weekDay(fromDateKey(20261019)) == 1);
    FakePlatform platform; MemoryStorage memory; System system;
    system.begin(&memory, &platform, UINT32_MAX - 500);
    system.state = sample(); system.changed();
    system.tick(300); assert(!system.dirty()); // autosave across rollover
    system.startTimer(1000); system.tick(800); assert(system.state.timerMs == 500);
    system.tick(1300); assert(!system.state.timerRunning && platform.alarms == 1);
    system.dismissNotification();
    system.toggleStopwatch(); system.tick(2300); assert(system.state.stopwatchMs == 1000);
    assert(system.setClock(1792425600));
    system.startTimer(10000); assert(system.save());
    System warm; warm.begin(&memory, &platform, 100); assert(warm.state.timerRunning && !warm.state.stopwatchRunning);
    platform.epoch = 0;
    System cold; cold.begin(&memory, &platform, 100); assert(!cold.clockValid() && !cold.state.timerRunning);
    assert(cold.state.timerMs > 0 && cold.state.notes[0].text == system.state.notes[0].text);
    platform.epoch = system.state.appointments[0].start - 300;
    cold.tick(1100); assert(cold.state.appointments[0].alerted);
    const int previous = platform.alarms; cold.tick(2100); assert(platform.alarms == previous);
}
int main() {
    expressionTests(); persistenceTests(); dateAndTimerTests();
    puts("core: formulas, cycles, limits, UTF-8, CRC, interrupted writes, recovery, dates, timers and autosave OK");
}
