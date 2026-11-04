#pragma once
#include "pda/games.h"
#include <vector>
namespace pda { namespace games {
bool extra(Kind kind);
const char *extraName(Kind kind);
const char *extraHelp(Kind kind);
unsigned extraLevels(Kind kind);
void extraReset(Kind kind, Save &save, uint32_t seed, unsigned level);
bool extraValid(Kind kind, const Save &save);
bool extraMove(Kind kind, Save &save, int dx, int dy);
bool extraActivate(Kind kind, Save &save);
bool extraSpecial(Kind kind, Save &save);
bool extraSpace(Kind kind, Save &save);
bool codeDigit(Save &save, unsigned digit);
// Old 11x9 adventures remain playable; new adventures use 19x13 (247 cells).
unsigned dungeonWidth(const Save &save);
unsigned dungeonHeight(const Save &save);
bool dungeonEnemy(char cell);
bool dungeonAction(Save &save, char key);
unsigned dungeonSupply(const Save &save, unsigned item);
const char *dungeonMessage(const Save &save);
bool snakeStep(Save &save);
bool snakePause(Save &save);
std::vector<unsigned> snakeBody(const Save &save);
std::string searchWord(const Save &save, unsigned index);
std::vector<unsigned> searchPath(unsigned start, unsigned end);
unsigned searchWidth(const Save &save);
unsigned searchHeight(const Save &save);
unsigned searchCount(const Save &save);
std::vector<unsigned> searchPath(const Save &save, unsigned start, unsigned end);
} }
