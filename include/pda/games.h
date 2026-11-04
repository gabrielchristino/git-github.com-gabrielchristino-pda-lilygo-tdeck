#pragma once
#include <stdint.h>
#include <string>

namespace pda { namespace games {
enum class Kind : uint8_t { Merge, Warehouse, Mines, Sudoku, Hangman, Naval, Snake, Code, WordSearch, Dungeon };
constexpr unsigned count = 10;
struct Save {
    bool started = false;
    uint8_t cursor = 0, status = 0; // playing, won, over
    uint16_t level = 0;
    uint32_t seed = 1, score = 0, best = 0, moves = 0;
    std::string board, marks;
    std::string undoBoard, undoMarks;
    uint32_t undoSeed = 1, undoScore = 0, undoMoves = 0;
    uint8_t undoCursor = 0, undoStatus = 0;
};
struct Collection { uint8_t selected = 0; Save slots[count]; };
const char *name(Kind kind);
const char *help(Kind kind);
unsigned levelCount(Kind kind);
void reset(Kind kind, Save &save, uint32_t seed, unsigned level = 0);
bool valid(Kind kind, const Save &save);
bool move(Kind kind, Save &save, int dx, int dy);
bool activate(Kind kind, Save &save);
bool digit(Save &save, unsigned digit);
bool special(Kind kind, Save &save); // undo / flag / hint
bool undo(Save &save);
bool conflict(const Save &save, unsigned cell);
unsigned flags(const Save &save);
unsigned mineWidth(const Save &save);
unsigned mineHeight(const Save &save);
unsigned mineCount(const Save &save);
unsigned warehouseWidth(const Save &save);
unsigned warehouseHeight(const Save &save);
bool guess(Save &save, char letter);
std::string hangmanDisplay(const Save &save, bool reveal = false);
const char *hangmanCategory(const Save &save);
const char *wordAt(unsigned index);
const char *wordCategory(unsigned index);
} }
