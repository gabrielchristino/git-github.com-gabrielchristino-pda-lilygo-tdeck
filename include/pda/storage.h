#pragma once
#include "pda/model.h"

namespace pda {
class StorageBackend {
public:
    virtual ~StorageBackend() = default;
    virtual bool ready() const = 0;
    virtual bool exists(int slot) const = 0;
    virtual bool read(int slot, std::vector<uint8_t> &data) = 0;
    virtual bool write(int slot, const std::vector<uint8_t> &data) = 0;
    virtual bool initialize() { return false; }
    virtual const char *description() const = 0;
};
uint32_t crc32(const uint8_t *data, size_t length);
bool encodeSnapshot(const State &state, uint64_t generation, std::vector<uint8_t> &data);
bool decodeSnapshot(const std::vector<uint8_t> &data, State &state, uint64_t &generation);
enum class LoadResult { Fresh, Loaded, Recovered, Corrupt, Unavailable };
class Store {
public:
    LoadResult begin(StorageBackend *backend, State &state);
    bool save(const State &state);
    bool ready() const;
    bool initialize(State &state);
    uint64_t generation() const { return generation_; }
    const std::string &error() const { return error_; }
    const char *description() const;
private:
    StorageBackend *backend_ = nullptr;
    uint64_t generation_ = 0;
    int active_ = -1;
    bool protected_ = false;
    std::string error_;
};
}
