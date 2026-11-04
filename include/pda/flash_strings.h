#pragma once
#include "pda/flash_records.h"
#include <stddef.h>
#include <string>

namespace pda {
// Sorted names share byte prefixes within blocks of 16. Random lookup decodes
// at most one block into a caller-owned string, with no global mutable buffer.
struct FlashStrings {
    const uint8_t *data;
    const FlashOffset24 *blocks;
    size_t bytes, count;
    class Cursor {
    public:
        explicit Cursor(const FlashStrings &table) : table_(table), last_(table.count) {}
        // Sequential searches reuse the preceding decoded name. Each caller
        // owns its cursor; render callbacks and nested lookups cannot clobber it.
        const std::string &get(size_t index) {
            if(index>=table_.count) return invalid();
            if(index==last_) return value_;
            size_t start=index;
            if(last_>=table_.count || index!=last_+1 || index%16==0) {
                at_=table_.blocks[index/16]; value_.clear(); start=index-index%16;
            }
            for(size_t i=start;i<=index;++i) {
                if(at_>=table_.bytes) return invalid();
                const size_t prefix=table_.data[at_++];
                if(prefix>value_.size()) return invalid();
                size_t end=at_;
                while(end<table_.bytes && table_.data[end]) ++end;
                if(end==table_.bytes) return invalid();
                value_.resize(prefix);
                value_.append(reinterpret_cast<const char *>(table_.data+at_),end-at_);
                at_=end+1;
            }
            last_=index;
            return value_;
        }
    private:
        const std::string &invalid() { last_=table_.count; value_.clear(); return value_; }
        const FlashStrings &table_;
        size_t at_=0, last_;
        std::string value_;
    };
    std::string get(size_t index) const { return Cursor(*this).get(index); }
};
}
