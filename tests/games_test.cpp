#include "pda/games.h"
#include "pda/storage.h"
#include <cassert>
#include <algorithm>
#include <cstdio>
#include <vector>
#include <set>
using namespace pda;
using namespace pda::games;
#include "game_solutions.h"

static void mergeTests() {
    Save g; reset(Kind::Merge, g, 123);
    assert(valid(Kind::Merge, g)); assert(std::count(g.board.begin(), g.board.end(), 'a') == 14);
    g.board.assign(16, 'a'); g.board.replace(0, 4, "bbbb");
    const auto before = g;
    assert(move(Kind::Merge, g, -1, 0));
    assert(g.board[0] == 'c' && g.board[1] == 'c' && g.score == 8 && g.moves == 1);
    assert(std::count(g.board.begin(), g.board.end(), 'a') == 13);
    const auto after = g;
    assert(undo(g) && g.board == before.board && g.seed == before.seed && g.score == before.score);
    assert(move(Kind::Merge, g, -1, 0) && g.board == after.board && g.seed == after.seed);
    reset(Kind::Merge, g, 42); g.board.assign(16, 'a'); g.board.replace(0, 3, "bbc");
    assert(move(Kind::Merge, g, -1, 0) && g.board[0] == 'c' && g.board[1] == 'c' && g.score == 4);
    for (int p = 0; p < 16; ++p) g.board[p] = char('b' + (p / 4 + p % 4) % 2);
    const auto blocked = g; assert(!move(Kind::Merge, g, -1, 0));
    assert(g.board == blocked.board && g.seed == blocked.seed && g.moves == blocked.moves);
    reset(Kind::Merge, g, 9); g.board.assign(16, 'a'); g.board.replace(0, 2, "kk");
    assert(move(Kind::Merge, g, -1, 0) && g.board[0] == 'l' && g.status == 1);
    assert(move(Kind::Merge, g, 1, 0)); // 2048 is a milestone; play can continue.
}
static void warehouseTests() {
    assert(levelCount(Kind::Warehouse)==24);
    for (unsigned level = 0; level < levelCount(Kind::Warehouse); ++level) {
        Save g; reset(Kind::Warehouse, g, 3, level); assert(valid(Kind::Warehouse, g));
        assert(g.board.size()==192);
        bool checkedUndo = false;
        for (const char *p = warehouseSolutions[level]; *p && !g.status; ++p) {
            const auto before = g;
            const int dx = *p == 'a' ? -1 : *p == 'd' ? 1 : 0;
            const int dy = *p == 'w' ? -1 : *p == 's' ? 1 : 0;
            assert(move(Kind::Warehouse, g, dx, dy)); assert(valid(Kind::Warehouse, g));
            if (!checkedUndo) {
                assert(undo(g) && g.board == before.board && g.moves == before.moves && g.cursor == before.cursor);
                assert(move(Kind::Warehouse, g, dx, dy)); checkedUndo = true;
                State state; state.games.slots[1]=g; std::vector<uint8_t> bytes;
                assert(encodeSnapshot(state,4,bytes)); State restored; uint64_t generation;
                assert(decodeSnapshot(bytes,restored,generation));
                const auto &saved=restored.games.slots[1];
                assert(saved.board==g.board&&saved.cursor==g.cursor&&saved.undoBoard==g.undoBoard&&saved.undoCursor==g.undoCursor);
            }
        }
        assert(g.status == 1 && g.board.find('$') == std::string::npos);
        assert(activate(Kind::Warehouse, g) && g.level == (level + 1) % levelCount(Kind::Warehouse));
    }
    for(unsigned level=0;level<12;++level) {
        Save g; reset(Kind::Warehouse,g,3,level);
        g.board=legacyWarehouseBoards[level]; g.cursor=uint8_t(g.board.find_first_of("@+"));
        assert(valid(Kind::Warehouse,g));
        for(const char *p=legacyWarehouseSolutions[level];*p&&!g.status;++p) {
            const int dx=*p=='a'?-1:*p=='d'?1:0,dy=*p=='w'?-1:*p=='s'?1:0;
            assert(move(Kind::Warehouse,g,dx,dy)&&valid(Kind::Warehouse,g));
        }
        assert(g.status==1&&g.board.size()==64);
        State state; state.games.slots[1]=g; std::vector<uint8_t> bytes;
        assert(encodeSnapshot(state,4,bytes)); State restored; uint64_t generation;
        assert(decodeSnapshot(bytes,restored,generation)&&restored.games.slots[1].board==g.board);
        assert(activate(Kind::Warehouse,g)&&g.board.size()==192&&g.level==level+1);
    }
}
static void mineTests() {
    for(unsigned size:{81u,208u}) for (unsigned cursor = 0; cursor < size; ++cursor) {
        Save g; reset(Kind::Mines, g, 100 + cursor);
        if(size==81) { g.board.assign(81,'0'); g.marks.assign(81,'h'); }
        const unsigned width=mineWidth(g),count=mineCount(g); g.cursor = uint8_t(cursor);
        assert(special(Kind::Mines, g) && !activate(Kind::Mines, g) && !g.moves);
        assert(valid(Kind::Mines, g)); assert(special(Kind::Mines, g));
        assert(activate(Kind::Mines, g) && g.status != 2);
        assert(g.board[cursor] == '0' && std::count(g.board.begin(), g.board.end(), '*') == int(count));
        for (unsigned p = 0; p < size; ++p) if (g.board[p] != '*') {
            unsigned adjacent = 0;
            for (unsigned q = 0; q < size; ++q)
                if (q != p && abs(int(q / width) - int(p / width)) <= 1 && abs(int(q % width) - int(p % width)) <= 1 && g.board[q] == '*') ++adjacent;
            assert(g.board[p] == char('0' + adjacent));
        }
        assert(valid(Kind::Mines, g));
        if (cursor == 0) {
            auto lost = g; lost.cursor = uint8_t(lost.board.find('*'));
            assert(activate(Kind::Mines, lost) && lost.status == 2 && valid(Kind::Mines, lost));
            assert(!activate(Kind::Mines, lost));
            for (unsigned p = 0; p < size && !g.status; ++p) if (g.board[p] != '*') { g.cursor = uint8_t(p); activate(Kind::Mines, g); }
            assert(g.status == 1 && g.score == size-count && flags(g) == count && valid(Kind::Mines, g));
            State state; state.games.slots[2]=g; std::vector<uint8_t> bytes;
            assert(encodeSnapshot(state,3,bytes)); State restored; uint64_t generation;
            assert(decodeSnapshot(bytes,restored,generation)&&restored.games.slots[2].board==g.board&&restored.games.slots[2].marks==g.marks);
            auto edge=g; edge.cursor=uint8_t(size-1);
            assert(!move(Kind::Mines,edge,1,0)&&!move(Kind::Mines,edge,0,1));
        }
    }
}
static unsigned solutions(std::string &board, unsigned maximum = 2) {
    int best = -1; std::vector<char> options;
    for (unsigned p = 0; p < 81; ++p) if (board[p] == '0') {
        std::vector<char> available;
        for (char c = '1'; c <= '9'; ++c) {
            bool used = false;
            for (unsigned q = 0; q < 81; ++q) if (board[q] == c &&
                (p / 9 == q / 9 || p % 9 == q % 9 || (p / 27 == q / 27 && p % 9 / 3 == q % 9 / 3))) { used = true; break; }
            if (!used) available.push_back(c);
        }
        if (available.empty()) return 0;
        if (best < 0 || available.size() < options.size()) { best = int(p); options = available; if (options.size() == 1) break; }
    }
    if (best < 0) return 1;
    unsigned count = 0;
    for (char c : options) { board[size_t(best)] = c; count += solutions(board, maximum - count); if (count >= maximum) break; }
    board[size_t(best)] = '0'; return count;
}
static void sudokuTests() {
    for (unsigned level = 0; level < levelCount(Kind::Sudoku); ++level) {
        Save g; reset(Kind::Sudoku, g, 1, level); assert(valid(Kind::Sudoku, g));
        auto puzzle = g.board; assert(solutions(puzzle) == 1);
        g.cursor = uint8_t(g.marks.find_first_not_of('0')); assert(!digit(g, 0));
        g.cursor = uint8_t(g.marks.find('0')); const auto before = g;
        assert(digit(g, 1)); assert(undo(g) && g.board == before.board);
        unsigned hints = 0;
        for (unsigned p = 0; p < 81; ++p) if (g.marks[p] == '0') { g.cursor = uint8_t(p); assert(special(Kind::Sudoku, g)); ++hints; }
        assert(g.status == 1 && g.score == hints && valid(Kind::Sudoku, g));
        for (unsigned p = 0; p < 81; ++p) assert(!conflict(g, p));
        assert(activate(Kind::Sudoku, g) && g.level == (level + 1) % levelCount(Kind::Sudoku));
    }
}
static void hangmanTests() {
    std::set<std::string> words;
    assert(levelCount(Kind::Hangman) == 384);
    assert(std::string(wordAt(0))=="abelha"&&std::string(wordAt(5))=="camaleão"&&std::string(wordAt(127))=="violão");
    bool sawAccent = false;
    for (unsigned level = 0; level < levelCount(Kind::Hangman); ++level) {
        Save g; reset(Kind::Hangman, g, 1, level);
        assert(valid(Kind::Hangman, g) && !g.board.empty() && words.insert(g.board).second);
        const auto letters = lowerAscii(g.board);
        assert(letters.find_first_not_of("abcdefghijklmnopqrstuvwxyz") == std::string::npos);
        assert(hangmanDisplay(g).find_first_not_of("_ ") == std::string::npos);
        for (char c : letters) {
            if (g.marks.find(c) != std::string::npos) continue;
            assert(guess(g, char(c - 'a' + 'A')));
            const auto moves = g.moves; assert(!guess(g, c) && g.moves == moves);
            assert(valid(Kind::Hangman, g));
        }
        assert(g.status == 1 && g.score == 0 && g.best == 1);
        auto shown = hangmanDisplay(g); shown.erase(std::remove(shown.begin(), shown.end(), ' '), shown.end());
        assert(shown == g.board);
        sawAccent = sawAccent || shown != letters;
        assert(!guess(g, 'z')); // Completed rounds cannot change or count twice.
        reset(Kind::Hangman, g, 2, level); assert(g.best == 1);
        for (char c = 'a'; c <= 'z' && !g.status; ++c)
            if (letters.find(c) == std::string::npos) { assert(guess(g, c)); assert(valid(Kind::Hangman, g)); }
        assert(g.status == 2 && g.score == 6 && g.moves == 6);
        const auto lost = g; assert(!guess(g, letters[0]) && g.marks == lost.marks);
        assert(hangmanDisplay(g, true).find('_') == std::string::npos);
    }
    assert(sawAccent);
    Save g; reset(Kind::Hangman, g, 1); const auto original = g;
    assert(!guess(g, '?') && !guess(g, ' ') && g.moves == original.moves);
    assert(!move(Kind::Hangman, g, 1, 0) && !special(Kind::Hangman, g));
    guess(g, 'a'); auto invalid = g; invalid.marks += 'a'; ++invalid.moves;
    assert(!valid(Kind::Hangman, invalid));
}
static void put32(std::vector<uint8_t> &bytes, size_t at, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[at + i] = uint8_t(value >> (i * 8));
}
static void persistenceTests() {
    State state; Note note; note.id = state.allocateId(); note.text = "Nota antiga com ç"; state.notes.push_back(note);
    std::vector<uint8_t> legacy; assert(encodeSnapshot(state, 35, legacy));
    // Empty games extension occupies 511 bytes in schema 4; remove it to make
    // an actual schema-1 fixture with the old payload, header length and CRC.
    legacy.erase(legacy.end() - 4 - 511, legacy.end() - 4);
    put32(legacy, 8, 1); put32(legacy, 20, uint32_t(legacy.size() - 28));
    put32(legacy, legacy.size() - 4, crc32(legacy.data(), legacy.size() - 4));
    State migrated; uint64_t generation = 0;
    assert(decodeSnapshot(legacy, migrated, generation) && generation == 35 && migrated.notes[0].text == note.text);
    for (unsigned i = 0; i < count; ++i) {
        auto &g = migrated.games.slots[i]; reset(Kind(i), g, 40 + i);
        if (i == 0 || i == 1) move(Kind(i), g, 1, 0); else activate(Kind(i), g);
    }
    // Schema 2 contains four existing game saves and no Hangman. Preserve them.
    State fourGames = migrated;
    reset(Kind::Warehouse,fourGames.games.slots[1],41);
    fourGames.games.slots[1].board=legacyWarehouseBoards[0];
    fourGames.games.slots[1].cursor=uint8_t(fourGames.games.slots[1].board.find_first_of("@+"));
    reset(Kind::Mines,fourGames.games.slots[2],42);
    fourGames.games.slots[2].board.assign(81,'0'); fourGames.games.slots[2].marks.assign(81,'h');
    for(unsigned i=4;i<count;++i) fourGames.games.slots[i]=Save();
    std::vector<uint8_t> schema2; assert(encodeSnapshot(fourGames, 35, schema2));
    schema2.erase(schema2.end() - 4 - 306, schema2.end() - 4);
    put32(schema2, 8, 2); put32(schema2, 20, uint32_t(schema2.size() - 28));
    put32(schema2, schema2.size() - 4, crc32(schema2.data(), schema2.size() - 4));
    State from2; assert(decodeSnapshot(schema2, from2, generation) && from2.notes[0].text == note.text);
    for (unsigned i = 0; i < 4; ++i) {
        assert(from2.games.slots[i].board == fourGames.games.slots[i].board);
        assert(from2.games.slots[i].marks == fourGames.games.slots[i].marks);
    }
    assert(!from2.games.slots[4].started);
    State fiveGames=migrated; fiveGames.games.slots[1]=fourGames.games.slots[1]; fiveGames.games.slots[2]=fourGames.games.slots[2];
    for(unsigned i=5;i<count;++i) fiveGames.games.slots[i]=Save();
    std::vector<uint8_t> schema3; assert(encodeSnapshot(fiveGames,35,schema3));
    schema3.erase(schema3.end()-4-255,schema3.end()-4);
    put32(schema3,8,3); put32(schema3,20,uint32_t(schema3.size()-28));
    put32(schema3,schema3.size()-4,crc32(schema3.data(),schema3.size()-4));
    State from3; assert(decodeSnapshot(schema3,from3,generation));
    for(unsigned i=0;i<5;++i) assert(from3.games.slots[i].board==fiveGames.games.slots[i].board&&from3.games.slots[i].marks==fiveGames.games.slots[i].marks);
    assert(!from3.games.slots[5].started);
    guess(migrated.games.slots[4], 'a'); guess(migrated.games.slots[4], 'z');
    migrated.games.selected = 3; migrated.session.app = 10; migrated.session.home = false; migrated.session.view = 8; migrated.session.document = 4;
    std::vector<uint8_t> bytes; assert(encodeSnapshot(migrated, 36, bytes)); State restored;
    assert(decodeSnapshot(bytes, restored, generation) && generation == 36 && restored.notes[0].text == note.text);
    assert(restored.session.app == 10 && restored.session.document == 4 && restored.games.selected == 3);
    for (unsigned i = 0; i < count; ++i) {
        assert(restored.games.slots[i].board == migrated.games.slots[i].board);
        assert(restored.games.slots[i].marks == migrated.games.slots[i].marks);
        assert(restored.games.slots[i].undoBoard == migrated.games.slots[i].undoBoard);
    }
    auto bad = migrated; bad.games.slots[2].cursor = 255; assert(!encodeSnapshot(bad, 37, bytes));
    put32(bytes, 8, 5); put32(bytes, bytes.size() - 4, crc32(bytes.data(), bytes.size() - 4));
    assert(!decodeSnapshot(bytes, restored, generation));
}
int main() {
    extern void moreGameTests(); moreGameTests();
    mergeTests(); warehouseTests(); mineTests(); sudokuTests(); hangmanTests(); persistenceTests();
    puts("games: 2048, 24 Sokobans, mines, 16 unique Sudokus, 384 Portuguese hangman words/accents/win/loss, schema-1/2 migration and game saves OK");
}
