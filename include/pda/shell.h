#pragma once
#include <lvgl.h>
#include "pda/catalog.h"
#include "pda/input.h"
#include "pda/workspace.h"
#include "pda/battery.h"
#include "pda/bluetooth.h"
#include <memory>

namespace pda {
class Shell {
public:
    void begin(bool touchReady, System &system);
    void handle(const InputEvent &event);
    void tick();
    void setTrackballStats(const TrackballStats &stats);
    void setGpsDiagnostic(const char *text);
    void setBatteryPercent(int percent);
    void setBatteryStats(const BatteryStats &stats);
    void setBluetoothRemote(const BluetoothRemote &remote) { remote_ = remote; }
    bool openBluetoothPeers();
private:
    static constexpr size_t maxApps = 13;
    static void remoteClicked(lv_event_t *event);
    void buildRemotePanel();
    void sendRemoteKey(uint8_t usage, uint8_t modifiers = 0);
    void sendRemoteCharacter(uint8_t character);
    void refreshRemote();
    void buildRemotePeers();
    void handleRemotePeers(const InputEvent &event);
    void tickRemotePeers();
    bool remoteManaging_ = false, remoteForgetConfirm_ = false;
    std::string remoteEditId_, remotePeerId_, remotePeerName_, remotePeerMessage_, remotePeerSignature_;
    enum class PeerAction { None, Select, Pair, Rename, Forget };
    PeerAction remotePeerAction_ = PeerAction::None;
    std::vector<BluetoothPeer> remotePeers_;
    uint32_t remotePeerPoll_ = 0;
    BluetoothRemote remote_;
    lv_obj_t *remoteStatus_ = nullptr;
    lv_obj_t *remotePanel_ = nullptr;
    lv_obj_t *remoteTabs_[4] = {};
    lv_obj_t *remoteModifiers_[4] = {};
    uint8_t remoteTab_ = 0, remoteMask_ = 0, remoteSymbols_ = 0;
    bool remoteRebuild_ = false;
    static void appClicked(lv_event_t *event);
    static void homeClicked(lv_event_t *event);
    static void alertClicked(lv_event_t *event);
    static void diagnosticClicked(lv_event_t *event);
    void selectDiagnostic(unsigned tab);
    void showHome();
    void showApp(const AppInfo &app);
    void rebuildList();
    void select(int index);
    void updateFooter();
    void refreshDiagnostic();
    void mapLayout(bool enabled);
    bool matches(const char *name) const;

    lv_obj_t *title_ = nullptr;
    lv_obj_t *time_ = nullptr;
    lv_obj_t *battery_ = nullptr;
    lv_obj_t *listCount_ = nullptr;
    lv_obj_t *body_ = nullptr;
    lv_obj_t *footer_ = nullptr;
    lv_obj_t *diagnostic_ = nullptr;
    lv_group_t *focus_ = nullptr;
    lv_obj_t *buttons_[maxApps] = {};
    const AppInfo *visible_[maxApps] = {};
    size_t visibleCount_ = 0;
    int selected_ = 0;
    char search_[25] = {};
    const AppInfo *current_ = nullptr;
    const AppInfo *pendingApp_ = nullptr;
    bool pendingHome_ = false;
    bool immersive_ = false;
    bool touchReady_ = false;
    bool searchEditing_ = false;
    uint32_t events_ = 0;
    System *system_ = nullptr;
    std::unique_ptr<Workspace> workspace_;
    lv_obj_t *alert_ = nullptr;
    TrackballStats ball_;
    int16_t lastX_ = 0, lastY_ = 0;
    uint8_t lastKey_ = 0;
    unsigned diagnosticTab_ = 0;
    lv_obj_t *diagnosticTabs_[3] = {};
    lv_obj_t *batteryBar_ = nullptr;
    BatteryStats batteryStats_;
    char gpsText_[512] = "GPS: aguardando hardware";
};
} // namespace pda
