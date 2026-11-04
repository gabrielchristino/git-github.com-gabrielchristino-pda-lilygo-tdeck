#include "pda/model.h"
#include <algorithm>
#include <cstdlib>
#include <ctype.h>

namespace pda {
const std::string &Sheet::get(int row, int column) const {
    static const std::string empty;
    for (const auto &cell : cells)
        if (cell.row == row && cell.column == column) return cell.text;
    return empty;
}
bool Sheet::set(int row, int column, const std::string &value) {
    if (row < 0 || row >= limits::rows || column < 0 || column >= limits::columns ||
        value.size() > limits::cellBytes) return false;
    for (auto it = cells.begin(); it != cells.end(); ++it) {
        if (it->row == row && it->column == column) {
            if (value.empty()) cells.erase(it); else it->text = value;
            return true;
        }
    }
    if (value.empty()) return true;
    if (cells.size() >= limits::cells) return false;
    Cell cell; cell.row = uint16_t(row); cell.column = uint8_t(column); cell.text = value;
    cells.push_back(cell);
    return true;
}
bool Sketch::pixel(int x, int y) const {
    if (x < 0 || y < 0 || x >= limits::sketchWidth || y >= limits::sketchHeight) return false;
    const size_t bit = size_t(y) * limits::sketchWidth + x;
    return pixels.size() == limits::sketchBytes && (pixels[bit / 8] & (0x80 >> (bit % 8)));
}
void Sketch::line(int x0, int y0, int x1, int y1, bool ink, int radius) {
    if (pixels.size() != limits::sketchBytes) pixels.assign(limits::sketchBytes, 0);
    const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    do {
        for (int yy = y0 - radius; yy <= y0 + radius; ++yy)
            for (int xx = x0 - radius; xx <= x0 + radius; ++xx) {
                if (xx < 0 || yy < 0 || xx >= limits::sketchWidth || yy >= limits::sketchHeight) continue;
                const size_t bit = size_t(yy) * limits::sketchWidth + xx;
                const uint8_t mask = uint8_t(0x80 >> (bit % 8));
                if (ink) pixels[bit / 8] |= mask; else pixels[bit / 8] &= uint8_t(~mask);
            }
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * error;
        if (e2 >= dy) { error += dy; x0 += sx; }
        if (e2 <= dx) { error += dx; y0 += sy; }
    } while (true);
}
uint32_t State::allocateId() {
    if (nextId == 0 || nextId == UINT32_MAX) return 0;
    return nextId++;
}
std::string lowerAscii(std::string value) {
    std::string result;
    for (size_t i = 0; i < value.size(); ++i) {
        const uint8_t c = uint8_t(value[i]);
        if (c == 0xC3 && i + 1 < value.size()) {
            const uint8_t accent = uint8_t(value[++i]) | 0x20;
            if (accent >= 0xA0 && accent <= 0xA5) result += 'a';
            else if (accent == 0xA7) result += 'c';
            else if (accent >= 0xA8 && accent <= 0xAB) result += 'e';
            else if (accent >= 0xAC && accent <= 0xAF) result += 'i';
            else if (accent >= 0xB2 && accent <= 0xB6) result += 'o';
            else if (accent >= 0xB9 && accent <= 0xBC) result += 'u';
            else { result += char(c); result += value[i]; }
        } else result += char(tolower(c));
    }
    return result;
}
std::string csvField(const std::string &value) {
    std::string result = "\"";
    for (char c : value) { if (c == '"') result += '"'; result += c; }
    return result + '"';
}
}
