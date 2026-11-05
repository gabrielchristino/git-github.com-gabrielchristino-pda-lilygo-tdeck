#include <Arduino.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
#include "hal/tdeck.h"
#include "hal/services.h"
#include "hal/audio.h"
#include "hal/gps.h"
#include "hal/bluetooth.h"
#include "hal/memory_probe.h"
#ifdef PDA_CHAT
#include "hal/chat_model.h"
#endif
#include "pda/shell.h"
#include "pda/datetime.h"
#include "pda/calendar_math.h"
#include "pda/map.h"
static pda::System systemState;
static pda::Shell shell;
static bool ready = false;
static void dispatch(const pda::InputEvent &event) {
    if (!pda::hal::wakeScreen()) shell.handle(event);
}
static void status() {
    Serial.printf("PDA BLUETOOTH %s HEAP=%u\n", pda::hal::bluetoothRemote().status(), ESP.getFreeHeap());
    int64_t gpsEpoch = 0;
    const bool gpsTimeValid = pda::hal::gpsUtc(gpsEpoch);
    Serial.printf("PDA CLOCK UTC=%lld GPS_VALID=%d GPS_UTC=%lld\n",
        (long long)systemState.utc(), gpsTimeValid, (long long)gpsEpoch);
    Serial.printf("PDA READY=%d STORAGE=%d GEN=%llu DIRTY=%d APP=%d HOME=%d NOTES=%u CONTACTS=%u EVENTS=%u SHEETS=%u DRAWINGS=%u CLOCK=%d VIEW=%u ROW=%d COL=%d CARET=%lu EDIT=%d\n",
        ready, systemState.store.ready(), static_cast<unsigned long long>(systemState.store.generation()), systemState.dirty(),
        int(systemState.state.session.app), systemState.state.session.home, unsigned(systemState.state.notes.size()),
        unsigned(systemState.state.contacts.size()), unsigned(systemState.state.appointments.size()),
        unsigned(systemState.state.sheets.size()), unsigned(systemState.state.sketches.size()), systemState.clockValid(),
        unsigned(systemState.state.session.view), int(systemState.state.session.row), int(systemState.state.session.column),
        static_cast<unsigned long>(systemState.state.session.caret), systemState.state.session.editing);
}
static void gameStatus(unsigned i) {
    if(i>=pda::games::count) return;
    const auto &g=systemState.state.games.slots[i];
    Serial.printf("GAME I=%u STARTED=%d LEVEL=%u CURSOR=%u STATUS=%u MOVES=%lu SCORE=%lu BEST=%lu BOARD=%lu MARKS=%lu AUX=%lu UM=%lu VALUE=%lu OTHER=%lu FLAG=%u TURN=%u SEED=%lu\n",
        i,g.started,unsigned(g.level),unsigned(g.cursor),unsigned(g.status),
        (unsigned long)g.moves,(unsigned long)g.score,(unsigned long)g.best,
        (unsigned long)pda::crc32(reinterpret_cast<const uint8_t *>(g.board.data()),g.board.size()),
        (unsigned long)pda::crc32(reinterpret_cast<const uint8_t *>(g.marks.data()),g.marks.size()),
        (unsigned long)pda::crc32(reinterpret_cast<const uint8_t *>(g.undoBoard.data()),g.undoBoard.size()),
        (unsigned long)pda::crc32(reinterpret_cast<const uint8_t *>(g.undoMarks.data()),g.undoMarks.size()),
        (unsigned long)g.undoScore,(unsigned long)g.undoMoves,unsigned(g.undoStatus),unsigned(g.undoCursor),(unsigned long)g.seed);
}
static void serialService() {
    static String line;
    static bool reported = false;
    if (Serial && !reported) { Serial.println("PDA 0.2 / T-Deck original"); status(); reported = true; }
    if (!Serial) reported = false;
    int budget = 128;
    while (Serial.available() && --budget > 0) {
        const char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (line == "PDA STATUS") status();
#ifdef PDA_CHAT
            else if (line.startsWith("PDA PT RUN ")) pda::hal::ptClassify(line.c_str()+11);
            else if (line == "PDA CHAT STATUS") {
                Serial.printf("CHAT ACTIVE=%d MESSAGES=%u BYTES=%u LABEL=%d\n",shell.chatActive(),
                    unsigned(shell.chatMessageCount()),unsigned(shell.chatDraftBytes()),shell.chatLastLabel());
                Serial.flush(); Serial.println(); Serial.flush();
            }
#endif
            else if (line.startsWith("PDA CALENDAR ")) {
                const bool valid=systemState.clockValid();const auto zone=systemState.state.preferences.timezoneMinutes;
                Serial.printf("CALENDAR CLOCK=%d TODAY=%s ZONE_MIN=%d\n",valid,
                    valid?pda::formatCivil(systemState.utc(),zone,false).c_str():"UNSET",zone);
                const auto reply=pda::calendarReply(line.c_str()+13,valid,valid?pda::fromUtc(systemState.utc(),zone):pda::CivilTime());
                Serial.print("CALENDAR REPLY=");Serial.println(reply.empty()?"UNRECOGNIZED":reply.c_str());
                Serial.println("CALENDAR DONE");Serial.flush();Serial.println();Serial.flush();
            }
            else if (line == "PDA AUDIO STATUS") {
                const auto a=pda::hal::audioStats();
                Serial.printf("AUDIO ENABLED=%d INIT=%d PLAYING=%d ERROR=%d REQUESTS=%lu COMPLETED=%lu BYTES=%lu TIMER_RUNNING=%d TIMER_MS=%llu NOTIFY=%d LATCHED=%d\n",
                    systemState.state.preferences.sound,a.initialized,a.playing,a.error,(unsigned long)a.requests,
                    (unsigned long)a.completed,(unsigned long)a.bytes,systemState.state.timerRunning,
                    (unsigned long long)systemState.state.timerMs,!systemState.notification().empty(),systemState.timerAlertActive());
                Serial.flush();Serial.println();Serial.flush();
            } else if(line=="PDA AUDIO TEST") {
                if(systemState.state.preferences.sound) {systemState.notify("Teste de som.");Serial.println("AUDIO TEST REQUESTED");}
                else Serial.println("AUDIO TEST MUTED");
                Serial.flush();Serial.println();Serial.flush();
            }
            else if (line == "PDA MEM") pda::hal::memoryReport();
            else if (line == "PDA MEM RESET") {
                pda::hal::memoryResetWindow(); Serial.println("MEM RESET OK");
            } else if (line == "PDA MEM RELEASE") {
                pda::hal::memoryRelease(); Serial.println("MEM RELEASE OK");
            } else if (line == "PDA MEM VERIFY") pda::hal::memoryVerify();
            else if (line == "PDA MEM CHECK") pda::hal::memoryCheck();
            else if (line == "PDA MEM SAVE") {
                const uint32_t started = micros();
                const bool saved = systemState.save();
                Serial.printf("MEM SAVE OK=%d US=%u\n", saved, unsigned(micros() - started));
                pda::hal::memoryReport();
            } else if (line.startsWith("PDA MEM RESERVE ")) {
                char *end = nullptr;
                const unsigned long bytes = strtoul(line.c_str() + 16, &end, 10);
                const bool valid = end != line.c_str() + 16 && *end == '\0' && bytes <= UINT32_MAX;
                Serial.println(valid && pda::hal::memoryReserve(uint32_t(bytes)) ? "MEM RESERVE OK" : "MEM RESERVE ERROR");
            }
            else if(line=="PDA BT LOG") pda::hal::bluetoothReport();
            else if(line=="PDA BT PEERS") {
                const auto remote=pda::hal::bluetoothRemote();
                const auto peers=remote.peers();
                for(size_t i=0;i<peers.size();++i) Serial.printf("BT PEER I=%u ID=%s SELECTED=%d CONNECTED=%d NAME=%s\n",
                    unsigned(i),peers[i].id.c_str(),peers[i].selected,peers[i].connected,peers[i].name.c_str());
                Serial.printf("BT PEERS COUNT=%u\n",unsigned(peers.size()));
            } else if(line=="PDA BT MANAGE") {
                const bool ok=shell.openBluetoothPeers(); if(ok) shell.tick();
                Serial.println(ok?"BT MANAGE OK":"BT MANAGE ERROR");
            } else if(line.startsWith("PDA BT SELECT ")) {
                Serial.println(pda::hal::bluetoothRemote().selectPeer(line.c_str()+14)?"BT SELECT OK":"BT SELECT ERROR");
            }
            else if (line == "PDA GPS") pda::hal::gpsReport();
            else if (line == "PDA MAP") {
                Serial.printf("MAP VECTOR FEATURES=%u POINTS=%u GPS_FIX=%d GPS_INSIDE=%d\n",
                    unsigned(pda::maps::featureCount),unsigned(pda::maps::pointCount),pda::maps::gps().valid,pda::maps::gps().inside);
                const auto &r=pda::maps::renderStats();
                Serial.printf("MAP RENDER CANDIDATES=%lu STROKES=%lu BUILDS=%lu BUILD_MS=%lu\n",
                    (unsigned long)r.candidates,(unsigned long)r.strokes,(unsigned long)r.geometryBuilds,(unsigned long)r.lastBuildMs);
                const auto &s=systemState.state.session;
                if(s.app==int8_t(pda::AppId::Map) && s.draft.size()==3)
                    Serial.printf("MAP VIEW X=%s Y=%s ZOOM=%s\n",s.draft[0].c_str(),s.draft[1].c_str(),s.draft[2].c_str());
            }
            else if (line == "PDA GPS RAW ON") { pda::hal::gpsRaw(true); Serial.println("GPS RAW ON"); }
            else if (line == "PDA GPS RAW OFF") { pda::hal::gpsRaw(false); Serial.println("GPS RAW OFF"); }
            else if (line == "PDA GAMES") {
                for (unsigned i = 0; i < pda::games::count; ++i) { gameStatus(i); Serial.flush(); }
            } else if(line.startsWith("PDA GAME ")) {
                const int i=atoi(line.c_str()+9); if(i>=0&&i<int(pda::games::count)) gameStatus(unsigned(i));
            }
            else if (line == "PDA BALL") {
                const auto b = pda::hal::trackballStats();
                Serial.printf("BALL IRQ L=%lu R=%lu U=%lu D=%lu LEVELS=%u\n", (unsigned long)b.left,(unsigned long)b.right,(unsigned long)b.up,(unsigned long)b.down,unsigned(b.levels));
                Serial.printf("BALL POLL L=%lu R=%lu U=%lu D=%lu\n", (unsigned long)b.polledLeft,(unsigned long)b.polledRight,(unsigned long)b.polledUp,(unsigned long)b.polledDown);
            }
            else if (line == "PDA SAVE") Serial.println(systemState.save() ? "PDA SAVED" : systemState.store.error().c_str());
            else if (line == "PDA HOME") {
                pda::hal::activity(); shell.handle({pda::Action::Home,0,0,0}); shell.tick(); Serial.println("PDA HOME OK");
            }
            else if (line.startsWith("PDA TIME ")) {
                const int64_t epoch = strtoll(line.c_str() + 9, nullptr, 10);
                Serial.println(systemState.setClock(epoch) ? "PDA TIME OK" : "PDA TIME ERROR");
            } else if (line.startsWith("PDA KEY ")) {
                const int key = atoi(line.c_str() + 8);
                pda::Action action = pda::Action::None;
                if (key == 0 || key == 13) action = pda::Action::Activate;
                else if (key == 4) action = pda::Action::ToggleNavigation;
                else if (key == 8) action = pda::Action::Backspace;
                else if (key == 9) action = pda::Action::Next;
                else if (key == 27) action = pda::Action::Home;
                else if (key >= 32 && key < 127) action = pda::Action::Text;
                if (action != pda::Action::None) { pda::hal::activity(); shell.handle({action,0,0,uint8_t(key)}); shell.tick(); Serial.println("PDA INPUT OK"); }
            } else if (line.startsWith("PDA MOVE ")) {
                int x = 0, y = 0;
                if (sscanf(line.c_str() + 9, "%d %d", &x, &y) == 2 && x >= -32 && x <= 32 && y >= -32 && y <= 32) {
                    pda::hal::activity(); shell.handle({pda::Action::Move,int16_t(x),int16_t(y),0}); shell.tick(); Serial.println("PDA INPUT OK");
                }
            } else if (line == "PDA RESTART") { systemState.save(); Serial.println("PDA RESTARTING"); Serial.flush(); ESP.restart(); }
            line = "";
        } else if (line.length() < 96) line += c;
    }
}
void setup() {
    Serial.begin(115200);
    ready = pda::hal::begin();
    if (ready) {
        pda::hal::gpsBegin();
        heap_caps_malloc_extmem_enable(1024);
        pda::hal::beginStorage();
        const auto result = systemState.begin(&pda::hal::storage(), &pda::hal::platform(), millis());
        if (result == pda::LoadResult::Fresh) systemState.changed(true);
        shell.setBluetoothRemote(pda::hal::bluetoothRemote());
#ifdef PDA_CHAT
        shell.setChatClassifier(pda::hal::chatClassify);
#endif
        shell.begin(pda::hal::touchAvailable(), systemState);
        shell.setBatteryStats(pda::hal::readBattery());
        Serial.printf("PDA: PSRAM=%u free=%u storage=%s\n", ESP.getPsramSize(), ESP.getFreePsram(), systemState.store.description());
        pda::hal::memoryResetWindow();
    } else Serial.println("PDA: inicializacao interrompida");
}
void loop() {
    if (ready) {
        pda::hal::memoryProbeTick();
        pda::hal::bluetoothPoll();
        pda::hal::gpsPoll();
        systemState.tick(millis());
        static bool gpsClockChecked = false;
        static uint32_t lastGpsClock = 0;
        const uint32_t clockNow = millis();
        if (!gpsClockChecked || uint32_t(clockNow - lastGpsClock) >= 60000) {
            int64_t epoch = 0;
            if (pda::hal::gpsUtc(epoch)) {
                const int64_t difference = systemState.utc() - epoch;
                if (!systemState.clockValid() || difference <= -2 || difference >= 2) {
                    if (systemState.setClock(epoch))
                        Serial.printf("PDA CLOCK GPS SYNC UTC=%lld\n", (long long)epoch);
                }
                gpsClockChecked = true; lastGpsClock = clockNow;
            }
        }
        pda::hal::poll(dispatch);
        static uint32_t lastBattery = 0;
        if (uint32_t(millis() - lastBattery) >= 10000) {
            lastBattery = millis();
            shell.setBatteryStats(pda::hal::readBattery());
        }
        static uint32_t lastDiagnostics = 0;
        if (uint32_t(millis() - lastDiagnostics) >= 200) {
            lastDiagnostics = millis(); shell.setTrackballStats(pda::hal::trackballStats());
            char gpsText[512]; pda::hal::gpsDescribe(gpsText, sizeof(gpsText)); shell.setGpsDiagnostic(gpsText);
            double lat=0,lon=0; unsigned sats=0;
            const bool positioned=pda::hal::gpsPosition(lat,lon,sats);
            pda::maps::updateGps(positioned,lat,lon,sats);
        }
        shell.tick();
        lv_timer_handler();
        pda::hal::powerTick();
        serialService();
    }
    delay(2);
}
