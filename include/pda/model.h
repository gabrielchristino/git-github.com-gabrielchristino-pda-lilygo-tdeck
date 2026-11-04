#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>
#include "pda/games.h"

namespace pda {
namespace limits {
constexpr size_t notes = 32, contacts = 128, events = 128, sheets = 4, sketches = 8;
constexpr size_t noteBytes = 16384, cells = 512, cellBytes = 256;
constexpr size_t snapshotBytes = 1400000;
constexpr int columns = 26, rows = 100, sketchWidth = 288, sketchHeight = 128;
constexpr size_t sketchBytes = sketchWidth * sketchHeight / 8;
}
struct Note { uint32_t id = 0; std::string title, text; };
struct Contact { uint32_t id = 0; std::string name, phone, email, detail; };
struct Appointment {
    uint32_t id = 0;
    std::string title, detail;
    int64_t start = 0, end = 0;
    int16_t reminderMinutes = -1;
    bool alerted = false;
    std::vector<std::string> draft;
};
struct Cell { uint16_t row = 0; uint8_t column = 0; std::string text; };
struct Sheet {
    uint32_t id = 0;
    std::string title;
    std::vector<Cell> cells;
    const std::string &get(int row, int column) const;
    bool set(int row, int column, const std::string &value);
};
struct Sketch {
    uint32_t id = 0;
    std::string title;
    std::vector<uint8_t> pixels = std::vector<uint8_t>(limits::sketchBytes, 0);
    bool pixel(int x, int y) const;
    void line(int x0, int y0, int x1, int y1, bool ink, int radius = 0);
};
struct Preferences {
    uint8_t brightness = 12;
    uint16_t dimSeconds = 30, offSeconds = 120;
    int16_t timezoneMinutes = -180;
    bool composeSymbols = true, sound = true;
};
// Views 0=list, 1=editor, 2=calendar, 3=canvas, 4=grid, 5=clock, 6=calculator,
// 7=settings, 8=games, 9=vector map. Dialogs are deliberately transient.
struct Session {
    int8_t app = -1;
    uint8_t view = 0, field = 0, form = 0;
    uint32_t document = 0, caret = 0;
    int16_t selected = 0, row = 0, column = 0, x = 144, y = 64;
    uint32_t day = 20000101;
    bool home = true, editing = false, pen = false, eraser = false;
    std::vector<std::string> draft;
};
struct State {
    uint32_t nextId = 1;
    Preferences preferences;
    Session session;
    std::vector<Note> notes;
    std::vector<Contact> contacts;
    std::vector<Appointment> appointments;
    std::vector<Sheet> sheets;
    std::vector<Sketch> sketches;
    std::string calculator;
    int64_t lastKnownUtc = 0;
    uint64_t stopwatchMs = 0, timerMs = 0;
    int64_t timerDeadline = 0;
    bool stopwatchRunning = false, timerRunning = false;
    games::Collection games;
    uint32_t allocateId();
};
template<class T> T *findById(std::vector<T> &items, uint32_t id) {
    for (auto &item : items) if (item.id == id) return &item;
    return nullptr;
}
template<class T> const T *findById(const std::vector<T> &items, uint32_t id) {
    for (const auto &item : items) if (item.id == id) return &item;
    return nullptr;
}
std::string lowerAscii(std::string value);
std::string csvField(const std::string &value);
}
