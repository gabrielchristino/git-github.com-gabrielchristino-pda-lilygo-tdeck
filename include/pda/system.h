#pragma once
#include "pda/storage.h"
namespace pda {
class Platform {
public:
    virtual ~Platform() = default;
    virtual int64_t utc() const { return 0; }
    virtual bool setUtc(int64_t) { return false; }
    virtual void configure(const Preferences &) {}
    virtual void alert() {}
    virtual bool exportData(const State &, std::string &message) { message = "SD indisponivel."; return false; }
};
class System {
public:
    State state;
    Store store;
    LoadResult begin(StorageBackend *backend, Platform *platform, uint32_t now);
    void tick(uint32_t now);
    void changed(bool urgent = false);
    bool save();
    bool dirty() const { return dirty_; }
    uint32_t now() const { return now_; }
    int64_t utc() const;
    bool clockValid() const;
    bool setClock(int64_t epoch);
    void configure();
    void startTimer(uint64_t ms);
    void toggleStopwatch();
    bool exportData(std::string &message);
    void notify(const std::string &message);
    const std::string &notification() const { return notification_; }
    void dismissNotification() { notification_.clear(); }
private:
    Platform *platform_ = nullptr;
    bool dirty_ = false, urgent_ = false;
    uint32_t now_ = 0, previous_ = 0, firstDirty_ = 0, lastEdit_ = 0, lastAttempt_ = 0, lastRuntimeSave_ = 0, lastReminder_ = 0;
    std::string notification_;
};
}
