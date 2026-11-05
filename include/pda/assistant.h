#pragma once
#include "pda/system.h"

namespace pda {
// Explicit read-only queries; independent of neural acceptance thresholds.
int assistantTelemetryIntent(const std::string &text);
// Typed entry points shared by the assistant and future local integrations.
// They never open apps; mutations invalidate only the affected saved editor.
class PdaServices {
public:
    explicit PdaServices(System &system) : sys_(system) {}
    std::string addNote(const std::string &text);
    std::string addContact(const std::string &name, const std::string &phone);
    std::string addEvent(const std::string &title, int64_t utc, bool reminder);
    std::string startTimer(uint64_t milliseconds);
    std::vector<uint32_t> findNotes(const std::string &query) const;
    std::vector<uint32_t> findEvents(const std::string &query) const;
    std::string readNote(uint32_t id, size_t offset = 0, size_t position = 0) const;
    std::string readEvent(uint32_t id, size_t offset = 0, size_t position = 0) const;
    std::string editNote(const Note &expected, const std::string &value, bool rename);
    std::string deleteNote(const Note &expected);
    std::string editEvent(const Appointment &expected, const Appointment &replacement);
    std::string deleteEvent(const Appointment &expected);
    std::string changeTimer(uint64_t milliseconds, uint32_t expectedRevision);
    std::string cancelTimer(uint32_t expectedRevision);
    std::string contact(const std::string &name) const;
    std::string agenda(int dayOffset, bool next, bool remindersOnly) const;
    std::vector<uint32_t> agendaItems(int dayOffset, bool next, bool remindersOnly) const;
    std::string timer() const;
private:
    System &sys_;
    std::string persist(const std::string &success);
};

class Assistant {
public:
    // Empty result means the caller may handle telemetry or give a fallback.
    std::string respond(System &system, const std::string &text);
    void clear();
private:
    enum class Pending { None, Note, Contact, EventTime, RescheduleTime, Event, Timer, EditNote, RenameNote, DeleteNote, EditEvent, DeleteEvent, ChangeTimer, CancelTimer };
    Pending pending_ = Pending::None;
    std::string first_, second_;
    int64_t date_ = 0;
    uint64_t duration_ = 0;
    bool reminder_ = false;
    Note noteBefore_;
    Appointment eventBefore_, eventAfter_;
    uint32_t timerRevision_ = 0;
    enum class Cursor { None, Note, Event, Notes, Events, Help };
    Cursor cursor_ = Cursor::None;
    size_t offset_ = 0;
    std::string query_;
    uint32_t contextId_ = 0, readStamp_ = 0;
    bool contextNote_ = false;
    struct Positions { bool known=false;std::vector<uint32_t> ids,order; };
    Positions notePositions_,eventPositions_;
    void rememberPositions(System &system,bool note,std::vector<uint32_t> ids);
    bool positionsValid(const System &system,bool note) const;
    size_t displayPosition(const System &system,bool note,uint32_t id) const;
    void resetDialogue();
    std::string records(System &system, const std::string &raw, const std::string &normalized);
    std::string readPage(System &system);
};
}
