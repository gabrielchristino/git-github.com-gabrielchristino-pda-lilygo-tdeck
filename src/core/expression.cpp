#include "pda/expression.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctype.h>
#include <stdio.h>

namespace pda {
Value Value::fail(const char *message) { Value v; v.kind = ValueKind::Error; v.error = message; return v; }
std::string Value::display() const {
    if (kind == ValueKind::Error) return error;
    if (kind == ValueKind::Blank || kind == ValueKind::Text) return {};
    char buffer[48]; snprintf(buffer, sizeof(buffer), "%.10g", number == 0 ? 0 : number); return buffer;
}
static bool numeric(const std::string &text, double &value) {
    if (text.empty()) return false;
    const char *start = text.c_str();
    while (isspace(static_cast<unsigned char>(*start))) ++start;
    if (!*start) return false;
    char *end = nullptr; value = strtod(start, &end);
    if (end == start) return false;
    while (isspace(static_cast<unsigned char>(*end))) ++end;
    return !*end;
}
class ExpressionParser {
public:
    ExpressionParser(Evaluator &owner, const std::string &source, int depth)
        : owner_(owner), source_(source), depth_(depth) {}
    Value run() {
        if (source_.size() > limits::cellBytes) return Value::fail("#LIMIT!");
        if (peek() == '=') ++position_;
        Value v = sum(0);
        if (v.kind == ValueKind::Error) return v;
        if (peek()) return Value::fail("#SYNTAX!");
        if (v.kind == ValueKind::Text) return Value::fail("#VALUE!");
        if (!std::isfinite(v.number)) return Value::fail("#NUM!");
        return v;
    }
private:
    char peek() {
        while (position_ < source_.size() && isspace(static_cast<unsigned char>(source_[position_]))) ++position_;
        return position_ < source_.size() ? source_[position_] : 0;
    }
    bool take(char c) { if (peek() != c) return false; ++position_; return true; }
    bool budget() { return ++owner_.operations_ <= 8192; }
    Value arithmetic(Value a, Value b, char operation) {
        if (a.kind == ValueKind::Error) return a;
        if (b.kind == ValueKind::Error) return b;
        if (a.kind == ValueKind::Text || b.kind == ValueKind::Text) return Value::fail("#VALUE!");
        Value v;
        if (operation == '+') v.number = a.number + b.number;
        else if (operation == '-') v.number = a.number - b.number;
        else if (operation == '*') v.number = a.number * b.number;
        else { if (b.number == 0) return Value::fail("#DIV/0!"); v.number = a.number / b.number; }
        return std::isfinite(v.number) ? v : Value::fail("#NUM!");
    }
    Value sum(int level) {
        Value v = product(level + 1);
        while (peek() == '+' || peek() == '-') {
            const char op = source_[position_++]; Value other = product(level + 1);
            v = arithmetic(v, other, op);
            if (v.kind == ValueKind::Error) return v;
        }
        return v;
    }
    Value product(int level) {
        Value v = atom(level + 1);
        while (peek() == '*' || peek() == '/') {
            const char op = source_[position_++]; Value other = atom(level + 1);
            v = arithmetic(v, other, op);
            if (v.kind == ValueKind::Error) return v;
        }
        return v;
    }
    bool reference(int &row, int &column) {
        peek();
        const size_t begin = position_;
        if (position_ >= source_.size() || !isalpha(static_cast<unsigned char>(source_[position_]))) return false;
        column = 0;
        while (position_ < source_.size() && isalpha(static_cast<unsigned char>(source_[position_]))) {
            column = std::min(10000, column * 26 + toupper(static_cast<unsigned char>(source_[position_++])) - 'A' + 1);
        }
        if (position_ >= source_.size() || !isdigit(static_cast<unsigned char>(source_[position_]))) {
            position_ = begin; return false;
        }
        row = 0;
        while (position_ < source_.size() && isdigit(static_cast<unsigned char>(source_[position_])))
            row = std::min(10000, row * 10 + source_[position_++] - '0');
        --row; --column;
        return true;
    }
    Value function(const std::string &name, int level) {
        if (name != "SUM" && name != "AVG" && name != "MIN" && name != "MAX" && name != "COUNT")
            return Value::fail("#NAME?");
        double total = 0, minimum = 0, maximum = 0;
        int count = 0;
        Value error;
        auto collect = [&](const Value &value) {
            if (!budget()) { error = Value::fail("#LIMIT!"); return; }
            if (value.kind == ValueKind::Error) { error = value; return; }
            if (value.kind != ValueKind::Number) return;
            total += value.number;
            if (!count) minimum = maximum = value.number;
            else { minimum = std::min(minimum, value.number); maximum = std::max(maximum, value.number); }
            ++count;
        };
        if (!take(')')) {
            do {
                const size_t saved = position_;
                int r0 = 0, c0 = 0, r1 = 0, c1 = 0;
                if (reference(r0, c0) && take(':')) {
                    if (!reference(r1, c1) || r0 < 0 || c0 < 0 || r1 < 0 || c1 < 0 ||
                        r0 >= limits::rows || r1 >= limits::rows || c0 >= limits::columns || c1 >= limits::columns)
                        return Value::fail("#REF!");
                    for (int r = std::min(r0, r1); r <= std::max(r0, r1); ++r)
                        for (int c = std::min(c0, c1); c <= std::max(c0, c1); ++c) {
                            collect(owner_.evaluateCell(r, c, depth_ + 1));
                            if (error.kind == ValueKind::Error) return error;
                        }
                } else {
                    position_ = saved; collect(sum(level + 1));
                    if (error.kind == ValueKind::Error) return error;
                }
            } while (take(';'));
            if (!take(')')) return Value::fail("#SYNTAX!");
        }
        Value result;
        if (name == "COUNT") result.number = count;
        else if (name == "SUM") result.number = total;
        else if (!count) return Value::fail("#EMPTY!");
        else if (name == "AVG") result.number = total / count;
        else if (name == "MIN") result.number = minimum;
        else result.number = maximum;
        return std::isfinite(result.number) ? result : Value::fail("#NUM!");
    }
    Value atom(int level) {
        if (level > 64 || !budget()) return Value::fail("#LIMIT!");
        const char next = peek();
        if (next == '+' || next == '-') {
            ++position_; Value v = atom(level + 1);
            if (v.kind == ValueKind::Text) return Value::fail("#VALUE!");
            if (next == '-') v.number = -v.number;
            return v;
        }
        if (take('(')) {
            Value v = sum(level + 1);
            if (v.kind == ValueKind::Error) return v;
            return take(')') ? v : Value::fail("#SYNTAX!");
        }
        if (isdigit(static_cast<unsigned char>(next)) || next == '.') {
            char *end = nullptr; Value v;
            v.number = strtod(source_.c_str() + position_, &end);
            if (end == source_.c_str() + position_) return Value::fail("#SYNTAX!");
            position_ = size_t(end - source_.c_str());
            return std::isfinite(v.number) ? v : Value::fail("#NUM!");
        }
        int row = 0, column = 0;
        if (reference(row, column)) return owner_.evaluateCell(row, column, depth_ + 1);
        std::string name;
        while (isalpha(static_cast<unsigned char>(peek()))) name += char(toupper(source_[position_++]));
        if (!name.empty() && take('(')) return function(name, level + 1);
        return Value::fail("#SYNTAX!");
    }
    Evaluator &owner_;
    const std::string &source_;
    int depth_;
    size_t position_ = 0;
};
Value Evaluator::parse(const std::string &source, int depth) { return ExpressionParser(*this, source, depth).run(); }
Value Evaluator::expression(const std::string &source) { operations_ = 0; cache_.clear(); active_.clear(); return parse(source, 0); }
Value Evaluator::cell(int row, int column) { operations_ = 0; cache_.clear(); active_.clear(); return evaluateCell(row, column, 0); }
Value Evaluator::evaluateCell(int row, int column, int depth) {
    if (!sheet_ || row < 0 || column < 0 || row >= limits::rows || column >= limits::columns) return Value::fail("#REF!");
    if (depth > 32 || ++operations_ > 8192) return Value::fail("#LIMIT!");
    const int index = row * limits::columns + column;
    if (active_[index]) return Value::fail("#CYCLE!");
    const auto cached = cache_.find(index); if (cached != cache_.end()) return cached->second;
    const auto &text = sheet_->get(row, column);
    Value result;
    if (text.empty()) result.kind = ValueKind::Blank;
    else if (text[0] == '=') { active_[index] = true; result = parse(text, depth); active_[index] = false; }
    else if (numeric(text, result.number)) { if (!std::isfinite(result.number)) result = Value::fail("#NUM!"); }
    else result.kind = ValueKind::Text;
    cache_[index] = result;
    return result;
}
}
