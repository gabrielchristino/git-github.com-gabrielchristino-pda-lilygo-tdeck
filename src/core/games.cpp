#include "pda/games.h"
#include "pda/model.h"
#include "pda/more_games.h"
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdlib>

namespace pda { namespace games {
namespace {
#include "game_levels.h"
#include "hangman_words.h"
uint8_t hangmanStatus(const Save &g) {
    const auto word = lowerAscii(g.board);
    const bool complete = word.find_first_not_of(g.marks) == std::string::npos;
    return complete ? 1 : g.score >= 6 ? 2 : 0;
}
uint32_t random(Save &g) {
    uint32_t n = g.seed ? g.seed : 1;
    n ^= n << 13; n ^= n >> 17; n ^= n << 5; g.seed = n; return n;
}
void remember(Save &g) {
    g.undoBoard = g.board; g.undoMarks = g.marks;
    g.undoSeed = g.seed; g.undoScore = g.score; g.undoMoves = g.moves;
    g.undoCursor = g.cursor; g.undoStatus = g.status;
}
void spawn(Save &g) {
    std::vector<unsigned> empty;
    for (unsigned i = 0; i < 16; ++i) if (g.board[i] == 'a') empty.push_back(i);
    if (!empty.empty()) {
        const unsigned p = empty[random(g) % empty.size()];
        g.board[p] = random(g) % 10 ? 'b' : 'c';
    }
}
bool canMerge(const Save &g) {
    for (int i = 0; i < 16; ++i) {
        if (g.board[i] == 'a') return true;
        if (g.board[i] >= 'a' + 30) continue;
        if (i % 4 < 3 && g.board[i] == g.board[i + 1]) return true;
        if (i < 12 && g.board[i] == g.board[i + 4]) return true;
    }
    return false;
}
bool box(char c) { return c == '$' || c == '*'; }
bool goal(char c) { return c == '.' || c == '*' || c == '+'; }
char floor(char c) { return goal(c) ? '.' : ' '; }
bool walkable(char c) { return c == ' ' || c == '.'; }
std::vector<int> neighbors(const Save &g,int p) {
    const int width=int(mineWidth(g)),height=int(mineHeight(g));
    std::vector<int> result;
    for (int y = std::max(0, p / width - 1); y <= std::min(height-1, p / width + 1); ++y)
        for (int x = std::max(0, p % width - 1); x <= std::min(width-1, p % width + 1); ++x)
            if (y * width + x != p) result.push_back(y * width + x);
    return result;
}
void placeMines(Save &g) {
    const int width=int(mineWidth(g));
    std::vector<int> candidates;
    for (int p = 0; p < int(g.board.size()); ++p)
        if (std::abs(p / width - g.cursor / width) > 1 || std::abs(p % width - g.cursor % width) > 1) candidates.push_back(p);
    for (unsigned n = 0; n < mineCount(g); ++n) {
        const unsigned pick = n + random(g) % (candidates.size() - n);
        std::swap(candidates[n], candidates[pick]); g.board[candidates[n]] = '*';
    }
    for (int p = 0; p < int(g.board.size()); ++p) {
        if (g.board[p] == '*') continue;
        int n = 0; for (int q : neighbors(g,p)) if (g.board[q] == '*') ++n;
        g.board[p] = char('0' + n);
    }
}
bool digitBoard(const std::string &board, size_t length) {
    return board.size() == length && board.find_first_not_of("0123456789") == std::string::npos;
}
bool warehouseValid(const Save &g) {
    if (g.level >= levelCount(Kind::Warehouse) || !g.marks.empty()) return false;
    const bool legacy=g.board.size()==64;
    if(legacy&&g.level>=sizeof(legacyWarehouseLevels)/sizeof(*legacyWarehouseLevels)) return false;
    const char *layout=legacy?legacyWarehouseLevels[g.level]:warehouseLevels[g.level];
    if(g.board.size()!=std::strlen(layout)) return false;
    unsigned players = 0, boxes = 0, goals = 0;
    for (unsigned i = 0; i < g.board.size(); ++i) {
        const char c = g.board[i], original = layout[i];
        if (std::strchr("# .$*@+", c) == nullptr || (c == '#') != (original == '#') || goal(c) != goal(original)) return false;
        if (c == '@' || c == '+') ++players;
        if (box(c)) ++boxes;
        if (goal(c)) ++goals;
    }
    return players == 1 && boxes == goals && g.cursor < g.board.size() && (g.board[g.cursor] == '@' || g.board[g.cursor] == '+');
}
}
const char *name(Kind k) {
    if (extra(k)) return extraName(k);
    switch (k) {
    case Kind::Merge: return "2048";
    case Kind::Warehouse: return "Sokoban";
    case Kind::Mines: return "Campo Minado";
    case Kind::Sudoku: return "Sudoku";
    case Kind::Hangman: return "Forca";
    default: break;
    }
    return "Jogos";
}
const char *help(Kind k) {
    if (extra(k)) return extraHelp(k);
    switch (k) {
    case Kind::Merge: return "Junte números iguais até chegar a 2048. WASD desliza o tabuleiro; U desfaz uma jogada. N pede nova partida. M volta aos jogos. A partida é salva automaticamente.";
    case Kind::Warehouse: return "Leve O aos alvos +; @ é você. Caixa verde está no alvo. WASD anda/empurra; U desfaz. Caixas não podem ser puxadas. Enter após vencer avança. N reinicia; M: jogos. Ajuda > Fases permite escolher entre 24 fases de 16x12.";
    case Kind::Mines: return "Abra as casas sem minas. Novas grades: 16x13, 40 minas. WASD move; Enter ou espaço abre; F marca bandeira. N pede nova partida; M volta aos jogos. A primeira abertura e seus vizinhos são seguros.";
    case Kind::Sudoku: return "Complete linhas, colunas e blocos com 1 a 9. WASD move; números preenchem; Enter alterna 1-9/vazio. Backspace apaga. H dá dica; U desfaz. N troca puzzle; M volta aos jogos.";
    case Kind::Hangman: return "Digite letras para adivinhar a palavra. A vale para á, ã e â; C vale para ç. Você tem 6 erros. Letras repetidas não contam. Enter: jogos. Backspace: nova palavra. Espaço após terminar: próxima.";
    default: break;
    }
    return "";
}
unsigned levelCount(Kind k) {
    if (extra(k)) return extraLevels(k);
    return k == Kind::Warehouse ? sizeof(warehouseLevels) / sizeof(*warehouseLevels) :
        k == Kind::Sudoku ? sizeof(sudokuPuzzles) / sizeof(*sudokuPuzzles) :
        k == Kind::Hangman ? sizeof(hangmanWords) / sizeof(*hangmanWords) : 1;
}
void reset(Kind k, Save &g, uint32_t seed, unsigned level) {
    if (extra(k)) { extraReset(k, g, seed, level); return; }
    const uint32_t best = g.best;
    g = Save(); g.started = true; g.seed = seed ? seed : 1; g.best = best;
    g.level = uint16_t(level % levelCount(k));
    if (k == Kind::Merge) { g.board.assign(16, 'a'); spawn(g); spawn(g); }
    else if (k == Kind::Warehouse) {
        g.board = warehouseLevels[g.level];
        g.cursor = uint8_t(g.board.find_first_of("@+"));
    } else if (k == Kind::Mines) { g.board.assign(208, '0'); g.marks.assign(208, 'h'); }
    else if (k == Kind::Hangman) g.board = hangmanWords[g.level].word;
    else { g.board = sudokuPuzzles[g.level]; g.marks = g.board; }
}
bool valid(Kind k, const Save &g) {
    if (!g.started) return g.board.empty() && g.marks.empty() && g.undoBoard.empty() && g.undoMarks.empty();
    if (extra(k)) return extraValid(k, g);
    if (!g.seed || g.status > 2 || g.level >= levelCount(k) || g.moves > 1000000000u) return false;
    if (k == Kind::Merge) {
        if (g.board.size() != 16 || !g.marks.empty() || g.cursor != 0) return false;
        for (char c : g.board) if (c < 'a' || c > 'a' + 30) return false;
    } else if (k == Kind::Warehouse) {
        if (!warehouseValid(g)) return false;
    } else if (k == Kind::Mines) {
        if ((g.board.size()!=81&&g.board.size()!=208) || g.marks.size()!=g.board.size() || g.cursor>=g.board.size() ||
            g.board.find_first_not_of("012345678*") != std::string::npos ||
            g.marks.find_first_not_of("hrf") != std::string::npos || !g.undoBoard.empty() || !g.undoMarks.empty()) return false;
        const auto mines = std::count(g.board.begin(), g.board.end(), '*');
        if ((!g.moves && (mines || g.status)) || (g.moves && mines != int(mineCount(g)))) return false;
    } else if (k == Kind::Hangman) {
        if (g.board != hangmanWords[g.level].word || g.cursor || g.score > 6 || g.moves != g.marks.size() ||
            g.marks.size() > 26 || g.marks.find_first_not_of("abcdefghijklmnopqrstuvwxyz") != std::string::npos ||
            !g.undoBoard.empty() || !g.undoMarks.empty()) return false;
        uint32_t misses = 0; const auto word = lowerAscii(g.board);
        for (size_t i = 0; i < g.marks.size(); ++i) {
            if (g.marks.find(g.marks[i]) != i) return false;
            if (word.find(g.marks[i]) == std::string::npos) ++misses;
        }
        if (misses != g.score || hangmanStatus(g) != g.status) return false;
    } else {
        if (!digitBoard(g.board, 81) || g.marks != sudokuPuzzles[g.level] || g.cursor >= 81) return false;
        for (unsigned i = 0; i < 81; ++i) if (g.marks[i] != '0' && g.board[i] != g.marks[i]) return false;
    }
    if (!g.undoBoard.empty()) {
        Save previous = g;
        previous.board = g.undoBoard; previous.marks = g.undoMarks;
        previous.seed = g.undoSeed; previous.score = g.undoScore; previous.moves = g.undoMoves;
        previous.cursor = g.undoCursor; previous.status = g.undoStatus;
        previous.undoBoard.clear(); previous.undoMarks.clear();
        if (!valid(k, previous)) return false;
    } else if (!g.undoMarks.empty()) return false;
    return true;
}
bool undo(Save &g) {
    if (g.undoBoard.empty()) return false;
    g.board = std::move(g.undoBoard); g.marks = std::move(g.undoMarks);
    g.seed = g.undoSeed; g.score = g.undoScore; g.moves = g.undoMoves;
    g.cursor = g.undoCursor; g.status = g.undoStatus;
    g.undoBoard.clear(); g.undoMarks.clear(); return true;
}
bool move(Kind k, Save &g, int dx, int dy) {
    if (extra(k)) return extraMove(k, g, dx, dy);
    if (!g.started || k == Kind::Hangman || (!dx && !dy)) return false;
    dx = dx > 0 ? 1 : dx < 0 ? -1 : 0; dy = dy > 0 ? 1 : dy < 0 ? -1 : 0;
    if (k == Kind::Mines || k == Kind::Sudoku) {
        const int width=k==Kind::Mines?int(mineWidth(g)):9,height=k==Kind::Mines?int(mineHeight(g)):9;
        const int x = std::max(0, std::min(width-1, int(g.cursor % width) + dx));
        const int y = std::max(0, std::min(height-1, int(g.cursor / width) + dy));
        const int cursor = y * width + x; if (cursor == g.cursor) return false;
        g.cursor = uint8_t(cursor); return true;
    }
    if (g.status == 2 || (k == Kind::Warehouse && g.status == 1)) return false;
    if (k == Kind::Warehouse) {
        const int step = dy ? dy * int(warehouseWidth(g)) : dx;
        const int p = g.cursor + step;
        if (p < 0 || p >= int(g.board.size()) || g.board[p] == '#') return false;
        if (box(g.board[p])) {
            const int q = p + step;
            if (q < 0 || q >= int(g.board.size()) || !walkable(g.board[q])) return false;
            remember(g); g.board[q] = goal(g.board[q]) ? '*' : '$';
        } else { if (!walkable(g.board[p])) return false; remember(g); }
        g.board[g.cursor] = floor(g.board[g.cursor]);
        g.board[p] = goal(g.board[p]) ? '+' : '@'; g.cursor = uint8_t(p); ++g.moves;
        g.status = g.board.find('$') == std::string::npos ? 1 : 0;
        if (g.status == 1) g.best = std::max(g.best, uint32_t(g.level + 1));
        return true;
    }
    std::string board(16, 'a'); uint32_t score = g.score;
    for (int line = 0; line < 4; ++line) {
        std::vector<char> values;
        auto index = [&](int offset) { return dy ? (dy < 0 ? offset : 3 - offset) * 4 + line : line * 4 + (dx < 0 ? offset : 3 - offset); };
        for (int p = 0; p < 4; ++p) if (g.board[index(p)] != 'a') values.push_back(g.board[index(p)]);
        int dest = 0;
        for (size_t p = 0; p < values.size(); ++p) {
            char value = values[p];
            if (p + 1 < values.size() && values[p + 1] == value && value < 'a' + 30) {
                ++value; ++p;
                const uint64_t total = uint64_t(score) + (uint32_t(1) << unsigned(value - 'a'));
                score = uint32_t(std::min<uint64_t>(UINT32_MAX, total));
            }
            board[index(dest++)] = value;
        }
    }
    if (board == g.board) return false;
    remember(g); g.board = std::move(board); g.score = score; g.best = std::max(g.best, score); ++g.moves; spawn(g);
    g.status = !canMerge(g) ? 2 : *std::max_element(g.board.begin(), g.board.end()) >= 'a' + 11 ? 1 : 0;
    return true;
}
bool digit(Save &g, unsigned value) {
    if (!g.started || value > 9 || g.cursor >= 81 || g.marks[g.cursor] != '0' || g.status == 1) return false;
    const char c = char('0' + value); if (g.board[g.cursor] == c) return false;
    remember(g); g.board[g.cursor] = c; ++g.moves;
    g.status = g.board == sudokuAnswers[g.level] ? 1 : 0;
    if (g.status == 1) g.best = std::max(g.best, uint32_t(g.level + 1));
    return true;
}
bool activate(Kind k, Save &g) {
    if (extra(k)) return extraActivate(k, g);
    if (!g.started) return false;
    if (k == Kind::Warehouse && g.status == 1) { reset(k, g, g.seed, g.level + 1); return true; }
    if (k == Kind::Sudoku) {
        if (g.status == 1) { reset(k, g, g.seed, g.level + 1); return true; }
        return digit(g, unsigned(g.board[g.cursor] - '0' + 1) % 10);
    }
    if (k != Kind::Mines || g.status || g.marks[g.cursor] != 'h') return false;
    if (!g.moves) placeMines(g);
    ++g.moves;
    if (g.board[g.cursor] == '*') { g.marks[g.cursor] = 'r'; g.status = 2; return true; }
    std::vector<int> queue = {g.cursor}; g.marks[g.cursor] = 'r';
    for (size_t n = 0; n < queue.size(); ++n) {
        const int p = queue[n];
        if (g.board[p] != '0') continue;
        for (int q : neighbors(g,p)) if (g.marks[q] == 'h' && g.board[q] != '*') { g.marks[q] = 'r'; queue.push_back(q); }
    }
    g.score = 0;
    for (unsigned p = 0; p < g.board.size(); ++p) if (g.marks[p] == 'r' && g.board[p] != '*') ++g.score;
    if (g.score == g.board.size()-mineCount(g)) { g.status = 1; ++g.best; for (unsigned p = 0; p < g.board.size(); ++p) if (g.board[p] == '*') g.marks[p] = 'f'; }
    return true;
}
bool special(Kind k, Save &g) {
    if (extra(k)) return extraSpecial(k, g);
    if (!g.started || k == Kind::Hangman) return false;
    if (k == Kind::Merge || k == Kind::Warehouse) return undo(g);
    if (k == Kind::Mines) {
        if (g.status || g.marks[g.cursor] == 'r') return false;
        g.marks[g.cursor] = g.marks[g.cursor] == 'f' ? 'h' : 'f'; return true;
    }
    if (g.status || g.marks[g.cursor] != '0' || g.board[g.cursor] == sudokuAnswers[g.level][g.cursor]) return false;
    if (!digit(g, unsigned(sudokuAnswers[g.level][g.cursor] - '0'))) return false;
    ++g.score; return true;
}
bool conflict(const Save &g, unsigned cell) {
    if (cell >= 81 || g.board.size() != 81 || g.board[cell] == '0') return false;
    for (unsigned p = 0; p < 81; ++p) if (p != cell && g.board[p] == g.board[cell] &&
        (p / 9 == cell / 9 || p % 9 == cell % 9 || (p / 27 == cell / 27 && p % 9 / 3 == cell % 9 / 3))) return true;
    return false;
}
unsigned warehouseWidth(const Save &g) { return g.board.size()==64?8:16; }
unsigned warehouseHeight(const Save &g) { return unsigned(g.board.size())/warehouseWidth(g); }
unsigned mineWidth(const Save &g) { return g.board.size()==81?9:16; }
unsigned mineHeight(const Save &g) { return unsigned(g.board.size())/mineWidth(g); }
unsigned mineCount(const Save &g) { return g.board.size()==81?10:40; }
unsigned flags(const Save &g) { return unsigned(std::count(g.marks.begin(), g.marks.end(), 'f')); }
bool guess(Save &g, char letter) {
    if (letter >= 'A' && letter <= 'Z') letter = char(letter - 'A' + 'a');
    if (!g.started || g.status || letter < 'a' || letter > 'z' || g.marks.find(letter) != std::string::npos) return false;
    g.marks += letter; ++g.moves;
    if (lowerAscii(g.board).find(letter) == std::string::npos) ++g.score;
    g.status = hangmanStatus(g);
    if (g.status == 1 && g.best != UINT32_MAX) ++g.best;
    return true;
}
std::string hangmanDisplay(const Save &g, bool reveal) {
    std::string display;
    for (size_t pos = 0; pos < g.board.size();) {
        const size_t length = (uint8_t(g.board[pos]) & 0xE0) == 0xC0 ? 2 : 1;
        const auto glyph = g.board.substr(pos, length); const auto letter = lowerAscii(glyph);
        if (!display.empty()) display += ' ';
        display += reveal || g.marks.find(letter) != std::string::npos ? glyph : "_";
        pos += length;
    }
    return display;
}
const char *hangmanCategory(const Save &g) { return g.level < levelCount(Kind::Hangman) ? hangmanWords[g.level].category : ""; }
const char *wordAt(unsigned index) { return index < levelCount(Kind::Hangman) ? hangmanWords[index].word : ""; }
const char *wordCategory(unsigned index) { return index < levelCount(Kind::Hangman) ? hangmanWords[index].category : ""; }
} }
