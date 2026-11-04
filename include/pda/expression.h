#pragma once
#include "pda/model.h"
#include <map>
namespace pda {
enum class ValueKind { Number, Blank, Text, Error };
struct Value {
    ValueKind kind = ValueKind::Number;
    double number = 0;
    std::string error;
    static Value fail(const char *message);
    std::string display() const;
};
class Evaluator {
public:
    explicit Evaluator(const Sheet *sheet = nullptr) : sheet_(sheet) {}
    Value expression(const std::string &source);
    Value cell(int row, int column);
private:
    friend class ExpressionParser;
    Value evaluateCell(int row, int column, int depth);
    Value parse(const std::string &source, int depth);
    const Sheet *sheet_;
    int operations_ = 0;
    std::map<int, Value> cache_;
    std::map<int, bool> active_;
};
}
