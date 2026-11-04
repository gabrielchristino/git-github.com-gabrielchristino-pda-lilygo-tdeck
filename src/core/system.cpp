#include "pda/system.h"
#include "pda/datetime.h"
#include <algorithm>
namespace pda {
LoadResult System::begin(StorageBackend *backend, Platform *platform, uint32_t now) {
    platform_ = platform; now_ = previous_ = lastAttempt_ = lastRuntimeSave_ = lastReminder_ = now;
    const auto result = store.begin(backend, state);
    if (state.stopwatchRunning) { state.stopwatchRunning = false; changed(); }
    if (state.timerRunning) {
        if (clockValid() && state.timerDeadline) {
            state.timerMs = uint64_t(std::max<int64_t>(0, state.timerDeadline - utc())) * 1000;
            state.timerRunning = state.timerMs > 0;
            if (!state.timerRunning) notify("Temporizador concluido.");
        } else { state.timerRunning = false; state.timerDeadline = 0; }
        changed();
    }
    configure();
    return result;
}
int64_t System::utc() const { return platform_ ? platform_->utc() : 0; }
bool System::clockValid() const { return utc() >= 946684800 && utc() <= 4102444799LL; }
void System::changed(bool urgent) {
    if (!dirty_) firstDirty_ = now_;
    dirty_ = true; urgent_ = urgent_ || urgent; lastEdit_ = now_;
}
bool System::save() {
    lastAttempt_ = now_;
    if (clockValid()) state.lastKnownUtc = utc();
    if (!store.save(state)) { urgent_ = false; return false; }
    dirty_ = urgent_ = false;
    return true;
}
void System::configure() { if (platform_) platform_->configure(state.preferences); }
bool System::setClock(int64_t epoch) {
    if (!validCivil(fromUtc(epoch, 0)) || !platform_ || !platform_->setUtc(epoch)) return false;
    state.lastKnownUtc = epoch;
    if (state.timerRunning) state.timerDeadline = epoch + int64_t((state.timerMs + 999) / 1000);
    changed(true);
    return true;
}
void System::startTimer(uint64_t ms) {
    state.timerMs = std::min<uint64_t>(ms, 31536000000ULL); state.timerRunning = state.timerMs > 0;
    state.timerDeadline = clockValid() && state.timerRunning ? utc() + int64_t((state.timerMs + 999) / 1000) : 0;
    changed(true);
}
void System::toggleStopwatch() { state.stopwatchRunning = !state.stopwatchRunning; changed(true); }
void System::notify(const std::string &message) {
    if (notification_.size() < 1024) {
        if (!notification_.empty()) notification_ += "\n";
        notification_ += message;
    }
    if (platform_ && state.preferences.sound) platform_->alert();
}
bool System::exportData(std::string &message) { return platform_ && platform_->exportData(state, message); }
void System::tick(uint32_t now) {
    now_ = now;
    const uint32_t elapsed = now - previous_; previous_ = now;
    if (state.stopwatchRunning) {
        state.stopwatchMs = std::min<uint64_t>(315360000000ULL, state.stopwatchMs + elapsed);
        if (state.stopwatchMs == 315360000000ULL) state.stopwatchRunning = false;
    }
    if (state.timerRunning) {
        if (elapsed >= state.timerMs) { state.timerMs = 0; state.timerRunning = false; state.timerDeadline = 0; notify("Temporizador concluido."); changed(true); }
        else state.timerMs -= elapsed;
    }
    if ((state.stopwatchRunning || state.timerRunning) && uint32_t(now - lastRuntimeSave_) >= 5000) {
        lastRuntimeSave_ = now; changed(true);
    }
    if (clockValid() && uint32_t(now - lastReminder_) >= 1000) {
        lastReminder_ = now;
        for (auto &event : state.appointments) {
            if (event.reminderMinutes >= 0 && !event.alerted && utc() >= event.start - event.reminderMinutes * 60 && utc() < event.start + 86400) {
                event.alerted = true; changed(true); notify("Agenda: " + event.title);
            }
        }
    }
    if (dirty_ && store.ready() && (urgent_ || uint32_t(now - lastEdit_) >= 750 || uint32_t(now - firstDirty_) >= 5000)) {
        // Retry a failed write at most every 5 s, while keeping the draft in RAM.
        if (store.error().empty() || uint32_t(now - lastAttempt_) >= 5000 || urgent_) save();
    }
}
}
