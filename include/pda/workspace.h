#pragma once
#include <lvgl.h>
#include "pda/catalog.h"
#include "pda/input.h"
#include "pda/system.h"
#include "pda/map.h"
#include <vector>
#include <string>
#include <deque>
namespace pda {
class Workspace {
public:
    Workspace(System &system, lv_obj_t *body, lv_obj_t *footer, lv_group_t *focus);
    ~Workspace();
    void open(AppId app, bool resume);
    void handle(const InputEvent &event);
    void tick();
    void checkpoint();
    void leave();
    bool consumesHome();
    bool takeHomeRequest() { const bool requested=homeRequested_; homeRequested_=false; return requested; }
private:
    enum class Command {
        None, NewItem, OpenItem, BackList, SaveForm, DeleteItem, ConfirmYes, Cancel,
        Field, Symbols, Symbol, CalPrevious, CalNext, CalToday, CalDay, SetClock,
        Stopwatch, ResetStopwatch, SetTimer, ToggleTimer, ResetTimer, Rename,
        Pen, Eraser, ClearCanvas, GridCell, SaveCell, Initialize, Export, SaveSettings,
        Sound, Composition, SaveNow, Help, Acknowledge, EditSettings, CalMonth,
        GameSelect, GameMenu, GameNew, GameHelp, GameHelpTab, GameWarehouseLevel, GameSpecial, GameCell,
        MapZoomIn, MapZoomOut, MapGps, MapReset, MapMenu, MapLabels, MapHelp, MapHome,
        MapSearch, MapFind, MapResult, MapSearchBack, MapAddressResult
    };
    enum class Modal { None, Message, ConfirmDelete, ConfirmClear, ConfirmInitialize, Symbols, Notification, ConfirmGameReset, ConfirmWarehouseLevel };
    enum class Form { None, Note, Contact, Event, Cell, Rename, Clock, Timer, Settings, Calculator };
    static void clicked(lv_event_t *event);
    static void changed(lv_event_t *event);
    static void canvasTouch(lv_event_t *event);
    static void mapDraw(lv_event_t *event);
    static void mapTouch(lv_event_t *event);
    void mapRestore(); void mapScreen(); void mapHandle(InputEvent event); void mapCheckpoint();
    void mapCommand(Command command);
    void mapControls(bool visible);
    void mapMotion();
    void mapSearchScreen(); void mapSearchHandle(InputEvent event);
    lv_obj_t *label(lv_obj_t *parent, const std::string &text, int x, int y, int width = 296);
    lv_obj_t *button(lv_obj_t *parent, const std::string &text, int x, int y, int width, Command cmd, int argument = 0, int height = 27);
    void clear(); void render(); void list(); void calendar();
    void editor(Form form, bool restoreDraft); void clock(); void calculator();
    void grid(); void canvas(); void settings();
    void gameScreen(); void hangmanScreen(); void gameHandle(InputEvent event); void gameReset(bool next = false);
    void extraGameScreen(); void pauseSnake();
    void gameHelp(unsigned tab = 0);
    lv_obj_t *gameTile(const std::string &text, int x, int y, int width, int height, uint32_t background, uint32_t ink, int cell = -1);
    void execute(Command command, int argument);
    void newItem(); void openItem(uint32_t id); void saveForm(); void deleteItem();
    void formFields(const std::vector<std::string> &names, const std::vector<std::string> &values, const std::vector<bool> &multiline);
    void fieldFocus(size_t index, bool edit); void syncFields();
    void insert(const std::string &text); void symbols();
    void message(const std::string &text, Modal modal = Modal::Message);
    void drawLine(int x, int y, bool touch = false); void refreshCanvas(); void refreshClock(); void refreshFooter();
    void moveFocus(int delta); void markSession(bool urgent = false);
    std::string itemTitle(uint32_t id) const;
    System &sys_;
    lv_obj_t *body_, *footer_;
    lv_group_t *focus_;
    AppId app_ = AppId::Notes;
    Form form_ = Form::None;
    Modal modal_ = Modal::None;
    Command pending_ = Command::None;
    int argument_ = 0;
    unsigned warehouseChoice_ = 0;
    bool rendering_ = false, editing_ = false;
    std::vector<lv_obj_t *> fields_, focusables_;
    std::vector<bool> multiline_;
    size_t field_ = 0;
    std::string search_, message_;
    lv_obj_t *canvas_ = nullptr, *cursor_ = nullptr, *clockLabel_ = nullptr, *stopwatchLabel_ = nullptr, *timerLabel_ = nullptr;
    std::vector<lv_color_t> canvasPixels_;
    uint32_t lastClock_ = 0;
    uint32_t lastGameStep_ = 0;
    bool touchDrawing_ = false;
    bool contentFocus_ = true;
    bool cursorNavigation_ = false, searchEditing_ = false;
    struct ButtonTag { Command command; int argument; };
    std::deque<ButtonTag> tags_;
    int lastTouchX_ = 0, lastTouchY_ = 0;
    maps::View mapView_;
    lv_obj_t *mapCanvas_ = nullptr;
    lv_obj_t *mapPanel_ = nullptr, *mapMenuButton_ = nullptr, *mapNamesButton_ = nullptr;
    bool mapControlsVisible_ = false, mapLabels_ = true, homeRequested_ = false;
    uint32_t mapGpsRevision_ = 0;
    uint32_t mapLastMotion_ = 0;
    bool mapMoving_ = false;
    bool mapSearchOpen_ = false, mapSearchEditing_ = true, mapSearchDone_ = false;
    std::string mapQuery_;
    std::vector<maps::SearchResult> mapResults_;
    std::vector<maps::AddressResult> mapAddressResults_;
    bool mapNumberSearch_ = false, mapApproximate_ = false;
    maps::Coord mapApproximatePoint_ = {0,0};
    lv_obj_t *mapQueryLabel_ = nullptr;
};
}
