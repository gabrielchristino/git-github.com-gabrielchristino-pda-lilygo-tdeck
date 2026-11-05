#include "pda/shell.h"
#include "pda/more_games.h"
#include "pda/expression.h"
#include "pda/datetime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#ifdef PDA_CHAT
#include "pda/intent_pt.h"
#include <fstream>
#include <iterator>
static std::vector<uint8_t> chatWeights;
static pda::ai::IntentPt chatModel;
static bool classifyChat(const char *text,int &label) {
    pda::ai::IntentPt::Prediction p;
    if (!chatModel.predict(text,p)) return false;
    label=p.label;return true;
}
static bool chatTextPresent(lv_obj_t *root,const char *text) {
    if (lv_obj_check_type(root,&lv_label_class) && strstr(lv_label_get_text(root),text)) return true;
    for (uint32_t i=0;i<lv_obj_get_child_cnt(root);++i) if (chatTextPresent(lv_obj_get_child(root,i),text)) return true;
    return false;
}
#endif
class Memory : public pda::StorageBackend {
public:
    std::vector<uint8_t> slots[2];
    bool ready() const override { return true; }
    bool exists(int i) const override { return !slots[i].empty(); }
    bool read(int i, std::vector<uint8_t> &v) override { v=slots[i]; return !v.empty(); }
    bool write(int i,const std::vector<uint8_t> &v) override { slots[i]=v; return true; }
    const char *description() const override { return "Memória de teste"; }
};
class Host : public pda::Platform {
public:
    int64_t epoch = 1792425600;
    unsigned alerts=0;
    unsigned stops=0;
    void alert() override {++alerts;}
    void stopAlert() override {++stops;}
    int64_t utc() const override { return epoch; }
    bool setUtc(int64_t e) override { epoch=e; return true; }
};
static unsigned char rgb[320*240*3];
static pda::Shell *shell;
static pda::System *systemState;
static uint32_t milliseconds = 0;
static void require(bool ok,const char *message) { if (!ok) { fprintf(stderr,"FAIL: %s\n",message); exit(1); } }
#include "game_font_audit.h"
static void flush(lv_disp_drv_t *driver,const lv_area_t *area,lv_color_t *pixels) {
    for(int y=area->y1;y<=area->y2;++y) for(int x=area->x1;x<=area->x2;++x) {
        lv_color32_t c; c.full=lv_color_to32(*pixels++); int o=(y*320+x)*3;
        rgb[o]=c.ch.red; rgb[o+1]=c.ch.green; rgb[o+2]=c.ch.blue;
    }
    lv_disp_flush_ready(driver);
}
static void pump(uint32_t elapsed = 10) {
    milliseconds+=elapsed; systemState->tick(milliseconds); shell->tick(); lv_tick_inc(elapsed); lv_timer_handler();
    if(systemState->state.session.app==int8_t(pda::AppId::Games)&&systemState->state.session.document) {
        lv_obj_update_layout(lv_scr_act()); auditGameLabels(lv_scr_act(),lv_obj_get_child(lv_scr_act(),1));
    }
}
static void input(pda::Action action,int x=0,int y=0,uint8_t c=0) { shell->handle({action,int16_t(x),int16_t(y),c}); pump(); }
static void type(const char *text) { while(*text) input(pda::Action::Text,0,0,uint8_t(*text++)); }
static const char *title() { return lv_label_get_text(lv_obj_get_child(lv_obj_get_child(lv_scr_act(),0),1)); }
static void home() { for(int i=0;i<3 && strcmp(title(),"Aplicativos");++i) input(pda::Action::Home); require(!strcmp(title(),"Aplicativos"),"home"); }
static void launch(const char *name) { home(); input(pda::Action::ToggleNavigation); type(name); input(pda::Action::Activate,0,0,13); }
static lv_obj_t *findButton(lv_obj_t *parent,const char *text) {
    if(lv_obj_has_flag(parent,LV_OBJ_FLAG_HIDDEN)) return nullptr;
    if(lv_obj_check_type(parent,&lv_btn_class) && lv_obj_get_child_cnt(parent)) {
        auto *label=lv_obj_get_child(parent,0);
        if(lv_obj_check_type(label,&lv_label_class) && !strcmp(lv_label_get_text(label),text)) return parent;
    }
    for(uint32_t i=0;i<lv_obj_get_child_cnt(parent);++i) { auto *found=findButton(lv_obj_get_child(parent,i),text); if(found) return found; }
    return nullptr;
}
static void click(const char *text) { auto *b=findButton(lv_scr_act(),text); require(b!=nullptr,text); lv_event_send(b,LV_EVENT_CLICKED,nullptr); pump(); }
static void areas(lv_obj_t *root,std::vector<lv_obj_t *> &out) {
    if(lv_obj_check_type(root,&lv_textarea_class)) out.push_back(root);
    for(uint32_t i=0;i<lv_obj_get_child_cnt(root);++i) areas(lv_obj_get_child(root,i),out);
}
static void fill(size_t index,const char *text) {
    std::vector<lv_obj_t *> list; areas(lv_scr_act(),list); require(index<list.size(),"text area index");
    lv_textarea_set_text(list[index],text); pump();
}
static void save(const char *name) {
    lv_obj_update_layout(lv_scr_act()); lv_obj_invalidate(lv_scr_act()); lv_refr_now(nullptr);
    if(systemState->state.session.app==int8_t(pda::AppId::Games) && systemState->state.session.document && !strncmp(name,"game-",5)) {
        auto *body=lv_obj_get_child(lv_scr_act(),1);
        require(lv_obj_get_y(body)==0&&lv_obj_get_width(body)==320&&lv_obj_get_height(body)==240,"every game uses fullscreen");
        require(lv_obj_has_flag(lv_obj_get_child(lv_scr_act(),0),LV_OBJ_FLAG_HIDDEN),"every game hides launcher header");
        const unsigned game=systemState->state.session.document;
        const bool gridOnly=game==1||game==2||game==3||game==9||game==10;
        require(lv_obj_has_flag(lv_obj_get_child(lv_scr_act(),2),LV_OBJ_FLAG_HIDDEN)==gridOnly,"grid-focused games hide footer");
        require(lv_obj_get_scroll_y(body)==0,"game needs no vertical scrolling");
        for(uint32_t i=0;i<lv_obj_get_child_cnt(body);++i) {
            auto *child=lv_obj_get_child(body,i);
            require(lv_obj_get_x(child)>=0&&lv_obj_get_x(child)+lv_obj_get_width(child)<=320,"game content fits screen width");
            require(lv_obj_get_y(child)>=0&&lv_obj_get_y(child)+lv_obj_get_height(child)<=(gridOnly?240:219),"game content fits above footer");
        }
    }
    FILE *file=fopen(name,"wb"); require(file!=nullptr,"capture file");
    fprintf(file,"P6\n320 240\n255\n"); fwrite(rgb,1,sizeof(rgb),file); fclose(file);
}
static lv_obj_t *referencePage(const char *titleText) {
    lv_obj_clean(lv_scr_act()); lv_obj_clean(lv_layer_top());
    auto *page=lv_obj_create(lv_scr_act()); lv_obj_remove_style_all(page);
    lv_obj_set_size(page,320,240); lv_obj_clear_flag(page,LV_OBJ_FLAG_SCROLLABLE);
    auto *titleLabel=lv_label_create(page); lv_label_set_text(titleLabel,titleText);
    lv_obj_set_style_text_font(titleLabel,&pda_font,0); lv_obj_set_pos(titleLabel,8,2);
    return page;
}
static lv_obj_t *referenceTile(lv_obj_t *page,const char *text,const lv_font_t *font,int x,int y,int width,int height,int yShift=0) {
    auto *tile=lv_obj_create(page); lv_obj_remove_style_all(tile);
    lv_obj_set_pos(tile,x,y); lv_obj_set_size(tile,width,height);
    lv_obj_set_style_bg_color(tile,lv_color_hex(pda::style::card),0);
    lv_obj_set_style_bg_opa(tile,LV_OPA_COVER,0);
    lv_obj_clear_flag(tile,LV_OBJ_FLAG_SCROLLABLE);
    auto *caption=lv_label_create(tile); lv_label_set_text(caption,text);
    lv_obj_set_style_text_font(caption,font,0); lv_obj_align(caption,LV_ALIGN_CENTER,0,yShift);
    return tile;
}
static void fontReferenceCaptures() {
    for(const lv_font_t *font:{&pda_font,&lv_font_montserrat_12,&lv_font_montserrat_16,&lv_font_montserrat_28}) {
        auto *page=referencePage(fontName(font)); unsigned i=0;
        const bool large=font==&lv_font_montserrat_28;
        const int columns=large?10:16,step=large?31:19,height=large?40:22;
        for(uint32_t cp:checkedGlyphs[font]) {
            const auto text=codepointText(cp);
            referenceTile(page,text.c_str(),font,8+int(i%columns)*step,26+int(i/columns)*height,step-1,height-1); ++i;
        }
        require(i<=unsigned(columns*(210/height)),"font reference fits screen");
        lv_obj_update_layout(lv_scr_act()); auditGameLabels(page,page);
        const std::string name=std::string("font-")+fontName(font)+".ppm"; save(name.c_str());
    }
    auto *page=referencePage("Símbolos nos tamanhos dos jogos");
    const char *alphabets[]={"12345678F*","ABCDEFGHIJKLMNOPQRSTUVWXYZ","@gmot+peb>"};
    const char *headings[]={"Campo Minado","Caça-palavras","Masmorra"};
    for(unsigned row=0;row<3;++row) {
        auto *heading=lv_label_create(page); lv_label_set_text(heading,headings[row]);
        lv_obj_set_style_text_font(heading,&pda_font,0); lv_obj_set_pos(heading,8,27+int(row)*64);
        for(unsigned i=0;alphabets[row][i];++i) {
            const char text[]={alphabets[row][i],0}; const bool dungeon=row==2;
            auto *tile=referenceTile(page,text,&lv_font_montserrat_12,8+int(i%16)*(dungeon?16:19),49+int(row)*64+int(i/16)*16,dungeon?15:18,dungeon?13:15,dungeon?-1:0);
            if(row==0) {
                lv_obj_set_style_border_width(tile,1,0);
                lv_obj_set_style_border_side(tile,LV_BORDER_SIDE_BOTTOM|LV_BORDER_SIDE_RIGHT,0);
            }
        }
    }
    lv_obj_update_layout(lv_scr_act()); auditGameLabels(page,page); save("font-game-symbols.ppm");
}
int main() {
    lv_init(); auditGameCorpus(); static lv_color_t pixels[320*40]; static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer,pixels,nullptr,320*40);
    static lv_disp_drv_t display; lv_disp_drv_init(&display); display.hor_res=320; display.ver_res=240;
    display.draw_buf=&buffer; display.flush_cb=flush; lv_disp_drv_register(&display);
    Memory memory; Host platform; pda::System system; pda::Shell view;
    shell=&view; systemState=&system; system.begin(&memory,&platform,0); view.begin(true,system); pump();
    view.setBatteryPercent(100); pump();
    save("home.ppm");
    type("s"); input(pda::Action::Activate,0,0,13);
    require(!strcmp(title(),"Calendário"),"launcher WASD moves selection instead of searching");
    home(); type("wwwda"); input(pda::Action::Activate,0,0,13);
    require(!strcmp(title(),"Agenda"),"launcher horizontal WASD navigates without ball");
    launch("notas"); click("Novo"); type("Texto persistente"); click("Símbolos"); click("ç");
    require(system.state.notes.size()==1 && system.state.notes[0].text=="Texto persistenteç","note + UTF-8 symbol insertion");
    fill(0,"Minha nota"); save("notes.ppm");
    const uint32_t noteId=system.state.notes[0].id;
    const uint32_t caret=system.state.session.caret;
    const std::string originalNote=system.state.notes[0].text;
    input(pda::Action::ToggleNavigation); type("aa");
    require(system.state.session.caret==caret-2 && system.state.notes[0].text==originalNote,"cursor mode moves left without inserting navigation letters");
    save("keyboard-cursor.ppm");
    input(pda::Action::ToggleNavigation); type("wasd$");
    require(system.state.notes[0].text.find("wasd$")!=std::string::npos,"text mode preserves all WASD letters and literal dollar");
    input(pda::Action::ToggleNavigation); input(pda::Action::Activate,0,0,13);
    require(!system.state.session.editing,"Enter exits multiline cursor mode without inserting newline");
    type("wwww"); input(pda::Action::Activate,0,0,13); // Symbols button.
    type("sdw"); input(pda::Action::Activate,0,0,13); // Row down, right, row up -> digit 1.
    require(system.state.session.editing && system.state.notes[0].text.find('1')!=std::string::npos,"symbol palette supports WASD rows and Enter without ball or touch");
    launch("contatos"); click("Novo"); fill(0,"Ana"); fill(1,"+55 11 99999-0000"); fill(2,"ana@example.com"); click("Salvar");
    require(system.state.contacts[0].name=="Ana","contact save"); save("contacts.ppm");
    // Complete create/edit/delete using keyboard events, without ball or touch.
    input(pda::Action::Activate,0,0,13);
    for(int i=0;i<20;++i) input(pda::Action::Backspace);
    type("Bruno"); input(pda::Action::Activate,0,0,13); input(pda::Action::Activate,0,0,13); type("1234");
    input(pda::Action::Activate,0,0,13); input(pda::Action::Activate,0,0,13); type("bruno@example.com");
    input(pda::Action::Activate,0,0,13); input(pda::Action::Activate,0,0,13); type("Sem touch");
    input(pda::Action::ToggleNavigation); input(pda::Action::Activate,0,0,13);
    for(int i=0;i<6;++i) type("w");
    input(pda::Action::Activate,0,0,13);
    require(system.state.contacts.size()==2 && system.state.contacts[1].name=="Bruno" && system.state.contacts[1].detail=="Sem touch","keyboard-only multi-field editing");
    type("ss"); input(pda::Action::Activate,0,0,13);
    input(pda::Action::Activate,0,0,13); type("ww"); input(pda::Action::Activate,0,0,13);
    type("a"); input(pda::Action::Activate,0,0,13);
    require(system.state.contacts.size()==1 && system.state.contacts[0].name=="Ana","keyboard-only delete confirmation");
    launch("agenda"); click("Novo"); fill(0,"Reunião"); fill(1,"20/10/2026 09:30"); fill(3,"5"); click("Salvar");
    require(system.state.appointments.size()==1 && system.state.appointments[0].reminderMinutes==5,"appointment save"); save("agenda.ppm");
    launch("calendario"); require(!strcmp(title(),"Calendário"),"accent-insensitive launcher search"); save("calendar.ppm");
    const uint32_t today=system.state.session.day;
    type("a");
    require(system.state.session.day==pda::dateKey(pda::addDays(pda::fromDateKey(today),-1)),"calendar keyboard left");
    type("dw");
    require(system.state.session.day==pda::dateKey(pda::addDays(pda::fromDateKey(today),-7)),"calendar keyboard up");
    type("s"); require(system.state.session.day==today,"calendar keyboard right and down");
    input(pda::Action::Move,1,0); input(pda::Action::Activate,0,0,13); save("calendar-day.ppm");
    input(pda::Action::Move,0,1); input(pda::Action::Activate);
    require(system.state.session.view==2,"calendar day-to-month navigation without touch");
    launch("desenho"); click("Novo"); input(pda::Action::Activate); input(pda::Action::Move,8,2); input(pda::Action::Move,5,-8);
    require(std::count(system.state.sketches[0].pixels.begin(),system.state.sketches[0].pixels.end(),uint8_t(0))<int(pda::limits::sketchBytes),"trackball drawing"); save("drawing.ppm");
    const int cursorX=system.state.session.x, cursorY=system.state.session.y;
    type("aw");
    require(system.state.session.x==cursorX-2 && system.state.session.y==cursorY-2,"drawing keyboard left and up");
    type("ds");
    require(system.state.session.x==cursorX && system.state.session.y==cursorY,"drawing keyboard right and down");
    click("Limpar"); click("Cancelar"); require(system.state.sketches[0].pixel(144,64),"cancel clear keeps drawing");
    launch("planilha"); click("Novo"); type("12"); input(pda::Action::Activate,0,0,13); input(pda::Action::Move,1,0);
    type("_-A1*2"); input(pda::Action::Activate,0,0,13);
    require(system.state.sheets[0].get(0,1)=="=A1*2","keyboard symbol composition");
    require(pda::Evaluator(&system.state.sheets[0]).cell(0,1).number==24,"sheet UI formula result"); save("sheet.ppm");
    // Traverse all viewport boundaries without any ball movement or touch.
    for(int i=0;i<30;++i) type("d");
    for(int i=0;i<105;++i) type("s");
    require(system.state.session.column==25 && system.state.session.row==99,"keyboard reaches Z100 and clamps at sheet bounds");
    type("W");
    require(system.state.session.row==98,"uppercase keyboard up");
    for(int i=0;i<30;++i) type("a");
    require(system.state.session.column==0 && system.state.session.row==98,"keyboard returns to column A across viewports");
    for(int i=0;i<105;++i) type("w");
    require(system.state.session.column==0 && system.state.session.row==0,"keyboard returns to A1 and clamps at origin");
    type("s"); input(pda::Action::Activate,0,0,13); type("wasd WASD m"); input(pda::Action::Activate,0,0,13);
    require(system.state.sheets[0].get(1,0)=="wasd WASD m","navigation keys remain writable in cell editor");
    type("m"); type("a");
    require(system.state.session.view==4 && system.state.sheets[0].get(1,0)=="wasd WASD m","menu keystrokes do not replace a cell");
    type("mwd");
    require(system.state.session.row==0 && system.state.session.column==1,"keyboard resumes content navigation after menu");
    launch("calculadora"); type("2+3*4"); input(pda::Action::Activate,0,0,13); save("calculator-result.ppm"); click("Voltar"); save("calculator.ppm");
    launch("relogio"); save("clock.ppm"); click("Definir timer"); fill(0,"1"); click("Salvar"); require(system.state.timerRunning && system.state.timerMs<=60000,"timer start");
    launch("ajustes"); save("settings.ppm"); click("Salvar agora"); click("Voltar");
    const bool sound=system.state.preferences.sound;
    type("s"); input(pda::Action::Activate,0,0,13);
    require(system.state.preferences.sound!=sound,"settings keyboard selects and activates buttons");
    const bool soundAfterKeyboard=system.state.preferences.sound;
    if(system.state.preferences.sound) click("Som: ligado");
    const auto alertsBeforeTest=platform.alerts;
    click("Testar som");require(platform.alerts==alertsBeforeTest,"muted sound test does not call speaker");click("Voltar");
    click("Som: desligado");click("Testar som");
    require(platform.alerts==alertsBeforeTest+1&&!system.notification().empty(),"sound test button routes through system alert");
    save("settings-sound-test.ppm");input(pda::Action::Activate,0,0,13);
    require(system.notification().empty(),"sound test notification dismisses normally");
    if(!soundAfterKeyboard) click("Som: ligado");
    launch("jogos"); require(system.state.session.app==10 && system.state.session.view==8,"games launcher"); save("games-menu.ppm");
    type("1"); type("wasd"); require(system.state.games.slots[0].moves>0,"2048 keyboard moves");
    save("game-2048.ppm"); click("Ajuda"); require(findButton(lv_scr_act(),"Partida"),"2048 stats moved into help"); save("game-2048-stats.ppm"); click("Voltar"); type("u"); const auto mergeBoard=system.state.games.slots[0].board;
    type("n"); input(pda::Action::Activate,0,0,13);
    require(system.state.games.slots[0].board==mergeBoard,"new game confirmation defaults to cancel");
    type("?"); save("game-help.ppm"); input(pda::Action::Activate,0,0,13);
    type("m2"); type("wasdwasd"); require(system.state.games.slots[1].moves>0,"Sokoban keyboard movement"); save("game-sokoban.ppm");
    const auto oldWarehouse=system.state.games.slots[1];
    click("Ajuda"); click("Fases"); save("game-sokoban-phases.ppm");
    click("13"); click("Cancelar");
    require(system.state.games.slots[1].board==oldWarehouse.board&&system.state.games.slots[1].moves==oldWarehouse.moves,"Sokoban phase selection can be cancelled");
    click("Ajuda"); click("Fases"); click("13"); click("Confirmar");
    auto &largeWarehouse=system.state.games.slots[1];
    require(largeWarehouse.level==12&&largeWarehouse.board.size()==192&&pda::games::valid(pda::games::Kind::Warehouse,largeWarehouse),"Sokoban selects larger phase");
    type("wasd"); const auto largeSteps=largeWarehouse.moves; require(largeSteps>0,"Sokoban larger board movement");
    type("u"); require(largeWarehouse.moves==largeSteps-1,"Sokoban larger board undo");
    save("game-sokoban-large.ppm"); click("Ajuda"); save("game-sokoban-stats.ppm");
    click("Controles"); save("game-sokoban-help.ppm"); click("Voltar");
    type("m3f"); input(pda::Action::Activate,0,0,13);
    require(system.state.games.slots[2].moves==0 && pda::games::flags(system.state.games.slots[2])==1,"flag prevents accidental mine opening");
    type("f"); input(pda::Action::Activate,0,0,13);
    require(system.state.games.slots[2].moves==1 && system.state.games.slots[2].status!=2,"mine first opening safe via keyboard"); save("game-mines.ppm"); click("Ajuda"); require(findButton(lv_scr_act(),"Partida"),"mine stats moved into help"); save("game-mines-stats.ppm"); click("Voltar");
    type("m4"); auto &sudoku=system.state.games.slots[3];
    const unsigned blank=unsigned(sudoku.marks.find('0'));
    for(unsigned i=0;i<blank/9;++i) type("s"); for(unsigned i=0;i<blank%9;++i) type("d");
    input(pda::Action::Activate,0,0,13); require(sudoku.board[blank]=='1',"Sudoku Enter cycles digits without modifier keys");
    const auto beforeHint=sudoku.board; type("h");
    if(sudoku.board!=beforeHint) { type("u"); require(sudoku.board==beforeHint,"Sudoku hint undo"); }
    type("9"); require(sudoku.board[blank]=='9',"Sudoku direct number input"); save("game-sudoku.ppm");
    type("m5"); auto &hangman=system.state.games.slots[4];
    require(hangman.started && system.state.session.document==5,"hangman menu selection");
    type("wasdmn");
    require(hangman.marks=="wasdmn" && system.state.session.document==5,"all navigation and menu letters are guesses in hangman");
    save("game-forca.ppm");
    pda::games::reset(pda::games::Kind::Hangman,hangman,1,5); type("camleao");
    require(hangman.status==1 && pda::games::hangmanDisplay(hangman)=="c a m a l e ã o","hangman keyboard accent win"); save("game-forca-win.ppm");
    pda::games::reset(pda::games::Kind::Hangman,hangman,1,34); type("bdefgh");
    require(hangman.status==2 && hangman.score==6,"hangman keyboard loss"); save("game-forca-loss.ppm");
    const auto lostWord=hangman.board, lostLetters=hangman.marks;
    input(pda::Action::Backspace); input(pda::Action::Activate,0,0,13);
    require(hangman.board==lostWord && hangman.marks==lostLetters,"hangman new round can be cancelled");
    type(" "); require(!hangman.status && hangman.moves==0 && hangman.board!=lostWord,"hangman next word differs and resets guesses");
    type("a"); input(pda::Action::Activate,0,0,13); type("5");
    require(hangman.marks=="a","hangman Enter returns to menu and resumes progress");
    input(pda::Action::Activate,0,0,13); type("6"); auto &naval=system.state.games.slots[5]; save("game-naval-setup.ppm");
    type(" "); require(naval.undoCursor==1,"naval keyboard rotates ship");
    type("r"); require(naval.level==4,"naval keyboard auto positioning");
    input(pda::Action::Activate,0,0,13); const auto navalMarks=naval.marks;
    input(pda::Action::Activate,0,0,13); require(naval.moves==1&&naval.marks==navalMarks,"naval repeat shot does not give AI extra turns"); save("game-naval.ppm");
    type("m7"); auto &snake=system.state.games.slots[6]; require(snake.undoStatus,"snake starts paused");
    input(pda::Action::Activate,0,0,13); pump(360); require(snake.moves==1,"snake advances on tick");
    type("w"); pump(360); input(pda::Action::Activate,0,0,13); require(snake.undoStatus,"snake Enter pauses"); save("game-snake.ppm");
    const auto snakeBoard=snake.board; input(pda::Action::Activate,0,0,13); input(pda::Action::Home); pump(500);
    require(snake.undoStatus&&snake.board==snakeBoard,"home pauses snake without background moves");
    launch("jogos"); type("7"); require(snake.undoStatus&&snake.board==snakeBoard,"snake resumes same board paused");
    type("m8"); type("1234"); input(pda::Action::Activate,0,0,13);
    require(system.state.games.slots[7].moves==1&&system.state.games.slots[7].marks.size()==6,"secret code keyboard submit with feedback"); save("game-code.ppm");
    type("m9"); auto &search=system.state.games.slots[8];
    input(pda::Action::Activate,0,0,13); type("d"); input(pda::Action::Activate,0,0,13);
    require(search.moves==1&&!search.undoStatus,"word search keyboard endpoints");
    input(pda::Action::Activate,0,0,13); type(" "); require(!search.undoStatus,"word search space cancels selection"); save("game-wordsearch.ppm");
    pda::games::reset(pda::games::Kind::WordSearch,search,501,23);
    input(pda::Action::Activate,0,0,13); type(" "); save("game-wordsearch-new.ppm");
    require(search.board.size()==208&&pda::games::searchCount(search)==10,"large word search has 208 cells and ten targets");
    auto *searchBody=lv_obj_get_child(lv_scr_act(),1); lv_obj_t *lastCell=nullptr;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(searchBody);++i) {
        auto *child=lv_obj_get_child(searchBody,i);
        if(lv_obj_get_x(child)==293&&lv_obj_get_y(child)==219) lastCell=child;
    }
    require(lastCell!=nullptr,"last large-grid cell fits screen");
    lv_event_send(lastCell,LV_EVENT_CLICKED,nullptr); pump();
    require(search.cursor==207&&search.undoStatus==1,"touch can select beyond the old 80-cell limit");
    const auto searchBeforeHelp=search;
    click("Ajuda"); require(findButton(lv_scr_act(),"Palavras")&&findButton(lv_scr_act(),"Controles"),"word search help has word list and controls tabs");
    save("game-wordsearch-words.ppm"); click("Controles"); save("game-wordsearch-help.ppm");
    click("Palavras"); click("Voltar");
    require(search.cursor==searchBeforeHelp.cursor&&search.marks==searchBeforeHelp.marks&&search.score==searchBeforeHelp.score&&search.moves==searchBeforeHelp.moves&&search.undoStatus==searchBeforeHelp.undoStatus,"help tabs preserve word search progress and selection");
    type("m0"); auto &dungeon=system.state.games.slots[9]; type(dungeon.board[21]=='#'?"s":"d");
    require(dungeon.moves==1&&dungeon.undoScore>0,"dungeon keyboard turn and fog"); save("game-dungeon.ppm");
    require(dungeon.board.size()==247,"dungeon expanded grid");
    auto healthBar=[]() -> lv_obj_t * {
        auto *body=lv_obj_get_child(lv_scr_act(),1);
        for(uint32_t i=0;i<lv_obj_get_child_cnt(body);++i) {
            auto *child=lv_obj_get_child(body,i);
            if(lv_obj_check_type(child,&lv_bar_class)) return child;
        }
        return nullptr;
    };
    require(healthBar()&&lv_bar_get_value(healthBar())==20&&lv_bar_get_max_value(healthBar())==20,"dungeon visible health bar");
    type("q"); require(dungeon.undoSeed==5&&dungeon.undoBoard[1]=='0',"dungeon keyboard shield");
    type("f"); require(dungeon.undoStatus==12&&dungeon.moves==3,"dungeon keyboard defense");
    type("x"); require(dungeon.undoBoard[0]=='0'&&dungeon.moves==4,"dungeon keyboard bomb");
    const auto fog=dungeon.marks; dungeon.marks.assign(247,'1');
    dungeon.undoScore=5;
    // A revealed review capture makes item symbols and enemy colors inspectable.
    type("?"); save("game-dungeon-help.ppm"); click("Voltar"); save("game-dungeon-map.ppm");
    require(healthBar()&&lv_bar_get_value(healthBar())==5,"dungeon wounded health bar");
    require(lv_obj_get_style_bg_color(healthBar(),LV_PART_INDICATOR).full==lv_color_hex(0xB82424).full,"dungeon low health turns red");
    dungeon.marks=fog;
    type("m"); require(lv_obj_get_y(lv_obj_get_child(lv_scr_act(),1))==30,"dungeon exit restores standard layout");
    type("0"); require(lv_obj_get_y(lv_obj_get_child(lv_scr_act(),1))==0,"dungeon resume restores fullscreen");
    type("m"); save("games-menu-more.ppm");
    launch("mapa"); require(system.state.session.app==11 && system.state.session.view==9,"vector map launcher");
    auto *mapBody=lv_obj_get_child(lv_scr_act(),1);
    require(lv_obj_has_flag(lv_obj_get_child(lv_scr_act(),0),LV_OBJ_FLAG_HIDDEN) &&
        lv_obj_has_flag(lv_obj_get_child(lv_scr_act(),2),LV_OBJ_FLAG_HIDDEN),"map hides launcher header and footer");
    require(lv_obj_get_width(mapBody)==320 && lv_obj_get_height(mapBody)==240 && lv_obj_get_y(mapBody)==0,"map uses full display");
    require(findButton(lv_scr_act(),"Menu") && !findButton(lv_scr_act(),"+ Zoom"),"map controls start collapsed");
    require(system.state.session.draft.size()==3,"map viewport session"); save("map-vectors.ppm");
    const auto mapStart=system.state.session.draft;
    type("b"); require(findButton(lv_scr_act(),"Pesquisar"),"map street search opens from keyboard");
    type("zzzzzzzz"); input(pda::Action::Activate,0,0,13); save("map-search-empty.ppm");
    require(system.state.session.draft==mapStart,"unsuccessful street search preserves viewport");
    click("Voltar"); click("Menu"); click("Buscar");
    type("nacoes unidas"); input(pda::Action::Activate,0,0,13); save("map-search-results.ppm");
    input(pda::Action::Activate,0,0,13);
    require(findButton(lv_scr_act(),"Menu") && !findButton(lv_scr_act(),"Pesquisar") && system.state.session.draft[2]=="2","map search Enter centers selected street");
    type("c");
    require(system.state.session.draft==mapStart,"map reset after search");
    type("bhebe camargx"); input(pda::Action::Backspace); type("o"); click("Pesquisar");
    click("Av. Hebe Camargo"); require(system.state.session.draft[2]=="2","map street search backspace and touch result");
    type("c");
    type("bnacoes unidas, 12501"); input(pda::Action::Activate,0,0,13);
    require(findButton(lv_scr_act(),"AVENIDA DAS NACOES UNIDAS"),"map approximate address results");
    save("map-address-results.ppm"); click("AVENIDA DAS NACOES UNIDAS"); pump(400);
    require(system.state.session.draft[2]=="3" && std::abs(std::stoi(system.state.session.draft[0])-284)<=1 &&
        std::abs(std::stoi(system.state.session.draft[1])+3534)<=1,"map interpolation matches independent Python reference");
    save("map-address-estimate.ppm"); const auto estimatedView=system.state.session.draft;
    type("bhebe camargo, 65000"); input(pda::Action::Activate,0,0,13); save("map-address-no-range.ppm");
    require(system.state.session.draft==estimatedView && findButton(lv_scr_act(),"Pesquisar"),"unknown number does not fabricate position");
    click("Voltar"); type("c"); pump(400); require(system.state.session.draft==mapStart,"reset approximate target");
    type("h"); require(findButton(lv_scr_act(),"Voltar")!=nullptr,"map attribution and help"); click("Voltar");
    const auto builds=pda::maps::renderStats().geometryBuilds;
    type("d"); save("map-moving.ppm");
    require(pda::maps::renderStats().geometryBuilds==builds,"short pan reuses offscreen vector margin");
    const std::vector<unsigned char> movingFrame(rgb,rgb+sizeof(rgb));
    pump(400); save("map-settled.ppm");
    require(memcmp(movingFrame.data(),rgb,sizeof(rgb))!=0,"street names return after movement stops");
    for(int i=0;i<4;++i) { input(pda::Action::Move,8,0); pump(30); }
    require(pda::maps::renderStats().geometryBuilds>builds,"long pan refreshes vector margin");
    type("c"); pump(220);
    type("sddi"); require(system.state.session.draft!=mapStart && system.state.session.draft[2]=="3","map keyboard pan and zoom");
    save("map-zoom.ppm"); type("f");
    require(findButton(lv_scr_act(),"Voltar")!=nullptr,"map cannot invent GPS fix"); click("Voltar");
    click("Menu"); require(findButton(lv_scr_act(),"+ Zoom")!=nullptr,"map touch opens controls"); save("map-menu.ppm");
    click("+ Zoom"); require(system.state.session.draft[2]=="2","map touch button zoom");
    click("Fechar"); require(!findButton(lv_scr_act(),"+ Zoom"),"map touch closes controls");
    type("l"); save("map-no-labels.ppm"); type("l");
    type("m"); require(findButton(lv_scr_act(),"Inicio")!=nullptr,"map keyboard opens controls"); type("m");
    pda::maps::updateGps(true,-23.6305,-46.7200,8); pump(); type("f"); save("map-test-gps.ppm");
    // Rebuilding help/search must keep the live viewport and GPS follow mode.
    pda::maps::updateGps(true,-23.6315,-46.7210,8); pump();
    type("h"); click("Voltar"); type("i");
    require(system.state.session.draft[0]==std::to_string(pda::maps::gps().point.x) &&
        system.state.session.draft[1]==std::to_string(pda::maps::gps().point.y),"map help preserves current GPS position");
    type("b"); click("Voltar");
    pda::maps::updateGps(true,-23.6325,-46.7220,8); pump(); type("o");
    require(system.state.session.draft[0]==std::to_string(pda::maps::gps().point.x) &&
        system.state.session.draft[1]==std::to_string(pda::maps::gps().point.y),"map keeps following GPS after help and search");
    std::vector<uint8_t> mapSnapshot; pda::State mapRestored; uint64_t mapGeneration=0;
    require(pda::encodeSnapshot(system.state,77,mapSnapshot) && pda::decodeSnapshot(mapSnapshot,mapRestored,mapGeneration),"map session snapshot roundtrip");
    require(mapRestored.session.app==11 && mapRestored.session.draft==system.state.session.draft,"map viewport survives snapshot");
    pda::maps::updateGps(false,0,0,0); pump(); save("map-test-no-fix.ppm");
    click("Menu"); click("Inicio");
    require(!strcmp(title(),"Aplicativos") && !lv_obj_has_flag(lv_obj_get_child(lv_scr_act(),0),LV_OBJ_FLAG_HIDDEN) &&
        !lv_obj_has_flag(lv_obj_get_child(lv_scr_act(),2),LV_OBJ_FLAG_HIDDEN),"map menu Home restores launcher bars");
    require(lv_obj_get_height(mapBody)==184 && lv_obj_get_y(mapBody)==30,"normal app layout restored after map");
    launch("notas"); input(pda::Action::ToggleNavigation); type("Minha nota"); input(pda::Action::ToggleNavigation);
    type("s"); input(pda::Action::Activate,0,0,13);
    require(system.state.session.document==noteId && system.state.session.editing,"list search and WASD selection coexist");
    type(" salvo"); pump(800); require(!system.dirty(),"idle autosave");
    pda::System reboot; require(reboot.begin(&memory,&platform,0)==pda::LoadResult::Loaded,"reload complete state");
    require(reboot.state.session.document==noteId && !reboot.state.session.home && reboot.state.session.editing,"restore document and editing mode");
    require(reboot.state.notes[0].text==system.state.notes[0].text,"restore note content");
    require(reboot.state.contacts.size()==1 && reboot.state.sheets.size()==1 && reboot.state.sketches.size()==1,"all apps survive reboot");
    for(unsigned i=0;i<pda::games::count;++i) {
        require(reboot.state.games.slots[i].board==system.state.games.slots[i].board,"all game boards survive reboot");
        require(reboot.state.games.slots[i].marks==system.state.games.slots[i].marks,"all game flags and givens survive reboot");
    }
    // Recreate the actual shell from its saved editor session.
    lv_obj_clean(lv_scr_act()); pda::Shell restoredView; shell=&restoredView; systemState=&reboot; restoredView.begin(true,reboot); pump();
    require(!strcmp(title(),"Notas"),"restore actual screen"); save("restored.ppm");
    launch("diagnostico");
    pda::BatteryStats battery;
    battery.sample(0); require(!battery.valid() && battery.percent == -1 && battery.samples == 0,"missing battery is not empty charge");
    battery.sample(3300); require(battery.percent == 0,"empty battery estimate");
    battery.sample(4200); require(battery.percent == 100,"full battery estimate");
    battery.sample(3750);
    require(battery.percent == 50 && battery.minimum == 3300 && battery.maximum == 4200 && battery.samples == 3,"battery voltage range since boot");
    restoredView.setBatteryStats(battery); pump();
    auto *batteryLabel=lv_obj_get_child(lv_obj_get_child(lv_scr_act(),1),0);
    require(strstr(lv_label_get_text(batteryLabel),"50%") && strstr(lv_label_get_text(batteryLabel),"3750 mV"),"battery tab displays actual voltage and estimate");
    lv_obj_update_layout(batteryLabel);
    require(lv_obj_get_y(batteryLabel)+lv_obj_get_height(batteryLabel)<=172,"battery diagnostics fit without scrolling");
    save("diagnostic-battery.ppm");
    battery.sample(6000); restoredView.setBatteryStats(battery); pump();
    require(battery.percent==-1 && battery.samples==3 && battery.minimum==3300,"invalid sample preserves valid history");
    require(strstr(lv_label_get_text(batteryLabel),"indisponível"),"invalid battery displays no fake measurement");
    save("diagnostic-battery-unavailable.ppm");
    battery.sample(3390); restoredView.setBatteryStats(battery); pump();
    require(strstr(lv_label_get_text(batteryLabel),"crítico"),"critical charge indication recovers after invalid reading");
    save("diagnostic-battery-low.ppm");
    click("Entradas");
    pda::TrackballStats ball; ball.left=7; ball.right=2; ball.polledLeft=5; ball.levels=15;
    restoredView.setTrackballStats(ball);
    const char *diagnostic=lv_label_get_text(lv_obj_get_child(lv_obj_get_child(lv_scr_act(),1),0));
    require(strstr(diagnostic,"Esq: 7") && strstr(diagnostic,"polling: 5"),"raw diagnostic updates without a net movement event");
    save("diagnostic.ppm");
    restoredView.setGpsDiagnostic("GPS: SERIAL OK / SEM POSICAO\nLigado: 20:00 / fix: aguardando\nAntena: OK / usados: 0\nReportados: 0 / com sinal: 0\nC/N0 max: 0 dB-Hz / pico: 0\nNMEA: 12000 / erros: 0 / perdas: 0\nUltimo NMEA: 0s / RX44\nSem sinal GNSS reportado");
    type("g"); pump();
    auto *gpsLabel=lv_obj_get_child(lv_obj_get_child(lv_scr_act(),1),0);
    require(strstr(lv_label_get_text(gpsLabel),"Reportados: 0"),"GPS diagnostics toggle");
    lv_obj_update_layout(gpsLabel);
    require(lv_obj_get_y(gpsLabel)+lv_obj_get_height(gpsLabel)<=172,"GPS diagnostic fits below tabs without scrolling");
    save("diagnostic-gps-no-signal.ppm");
    restoredView.setGpsDiagnostic("GPS: SERIAL OK / SEM POSICAO\nLigado: 20:15 / 1o fix: 95s\nAntena: OK / usados: 0\nReportados: 12 / com sinal: 5\nC/N0 max: 38 dB-Hz / pico: 41\nNMEA: 12150 / erros: 0 / perdas: 0\nUltimo NMEA: 0s / RX44\nRecebendo; aguardando posicao");
    pump();
    require(strstr(lv_label_get_text(gpsLabel),"com sinal: 5"),"GPS updates live");
    save("diagnostic-gps-signal.ppm");
    type("g");
    require(strstr(lv_label_get_text(gpsLabel),"Esq: 7"),"GPS toggle preserves trackball diagnostics");
    input(pda::Action::Next);
    require(strstr(lv_label_get_text(gpsLabel),"Carga estimada"),"Tab cycles diagnostics");
    type("2"); require(strstr(lv_label_get_text(gpsLabel),"com sinal: 5"),"number shortcut selects GPS");
    click("Bateria"); require(strstr(lv_label_get_text(gpsLabel),"3390 mV"),"touch tab preserves battery telemetry");
    launch("jogos"); type("5"); type("w"); require(reboot.save(),"game session save");
    pda::System gamesReboot; require(gamesReboot.begin(&memory,&platform,0)==pda::LoadResult::Loaded,"game session reload");
    lv_obj_clean(lv_scr_act()); pda::Shell gamesView; shell=&gamesView; systemState=&gamesReboot; gamesView.begin(true,gamesReboot); pump();
    require(!strcmp(title(),"Jogos") && gamesReboot.state.session.document==5 && gamesReboot.state.games.slots[4].marks=="aw","restore actual hangman screen and guesses");
    save("game-restored.ppm");
    input(pda::Action::Activate,0,0,13); type("7"); input(pda::Action::Activate,0,0,13);
    const auto activeSnake=gamesReboot.state.games.slots[6]; require(!activeSnake.undoStatus,"snake running before save");
    require(gamesReboot.save(),"running snake saved");
    pda::System snakeReboot; require(snakeReboot.begin(&memory,&platform,0)==pda::LoadResult::Loaded,"running snake reload");
    lv_obj_clean(lv_scr_act()); pda::Shell snakeView; shell=&snakeView; systemState=&snakeReboot; snakeView.begin(true,snakeReboot); pump(600);
    require(snakeReboot.state.games.slots[6].undoStatus&&snakeReboot.state.games.slots[6].board==activeSnake.board,"restored snake is paused before any simulation step");
    input(pda::Action::Activate,0,0,13); snakeReboot.notify("Lembrete de teste"); pump(600);
    require(snakeReboot.state.games.slots[6].undoStatus&&snakeReboot.state.games.slots[6].board==activeSnake.board,"notification pauses snake before its next step");
    input(pda::Action::Activate,0,0,13); save("game-snake-restored.ppm");
    // Exercise the 2048 fonts with four, five and ten digits, including its limit.
    launch("jogos"); type("m1");
    require(systemState->state.session.document==1,"large-number audit opens 2048");
    auto &largeNumbers=systemState->state.games.slots[0];
    largeNumbers.board[0]=char('a'+30); largeNumbers.board[1]=char('a'+14);
    largeNumbers.board[2]=char('a'+11); largeNumbers.board[3]=char('a'+13);
    type("?"); click("Voltar"); save("game-2048-large-values.ppm");
    bool static remoteEnabled = false;
    unsigned static remoteInputs = 0, remoteCommands = 0;
    uint8_t static remoteCharacter = 0;
    pda::BluetoothRemote remote;
    remote.enable = [](bool enabled) { remoteEnabled = enabled; };
    remote.status = []() { return "Conectado"; };
    remote.input = [](const pda::InputEvent &event) { ++remoteInputs; remoteCharacter = event.character; };
    remote.command = [](pda::RemoteCommand) { ++remoteCommands; };
    shell->setBluetoothRemote(remote);
    launch("bluetooth"); require(remoteEnabled,"Bluetooth starts when opening app");
    type("wasd$"); input(pda::Action::Activate,0,0,13);
    input(pda::Action::Home,0,0,27); input(pda::Action::Move,-2,3);
    input(pda::Action::Activate);
    require(remoteInputs==9&&!strcmp(title(),"Bluetooth"),"remote receives text, Enter, Escape, movement and click");
    input(pda::Action::ToggleNavigation,0,0,4);
    require(remoteInputs==10 && remoteCharacter=='=',"Shift+$ sends missing equals sign over Bluetooth");
    const pda::InputEvent dismissInputs[] = {
        {pda::Action::Activate,0,0,13}, {pda::Action::Backspace,0,0,8},
        {pda::Action::Home,0,0,0}
    };
    for (const auto &dismiss : dismissInputs) {
        systemState->notify("Alarme durante Bluetooth"); pump();
        type("x"); input(pda::Action::Move,1,1);
        require(!systemState->notification().empty() && remoteInputs==10,"Bluetooth alert blocks remote input");
        input(dismiss.action,0,0,dismiss.character);
        require(systemState->notification().empty() && remoteInputs==10 && remoteEnabled &&
            !strcmp(title(),"Bluetooth"),"Bluetooth alert consumes dismissal without sending keys or leaving app");
    }
    click("Direito"); click("Arrastar"); click("Rolar +"); click("Rolar -");
    require(remoteCommands==4,"all mouse controls dispatch commands");
    static std::vector<std::pair<uint8_t,uint8_t>> remoteKeys;
    static bool remoteConnected = true;
    remote.key = [](uint8_t usage,uint8_t modifiers) { remoteKeys.push_back({usage,modifiers}); };
    remote.connected = []() { return remoteConnected; };
    shell->setBluetoothRemote(remote);
    auto lastRemoteKey = [&](uint8_t usage,uint8_t modifiers) {
        require(!remoteKeys.empty() && remoteKeys.back()==std::make_pair(usage,modifiers),"touch/physical key sends expected HID chord");
    };
    auto remoteBounds = [&]() {
        lv_obj_update_layout(lv_scr_act());
        auto *body=lv_obj_get_child(lv_scr_act(),1);
        auto *panel=lv_obj_get_child(body,5);
        lv_area_t bounds; lv_obj_get_coords(panel,&bounds);
        require(bounds.x1>=0 && bounds.x2<320 && bounds.y1>=30 && bounds.y2<219,"Bluetooth panel fits body above footer");
        for(uint32_t i=0;i<lv_obj_get_child_cnt(panel);++i) {
            auto *child=lv_obj_get_child(panel,i); lv_area_t area; lv_obj_get_coords(child,&area);
            require(area.x1>=bounds.x1 && area.y1>=bounds.y1 && area.x2<=bounds.x2 && area.y2<=bounds.y2,"Bluetooth control fits panel");
            if(lv_obj_check_type(child,&lv_btn_class)) {
                auto *label=lv_obj_get_child(child,0);
                checkGlyphText(lv_obj_get_style_text_font(label,LV_PART_MAIN),lv_label_get_text(label),"Bluetooth touch button");
                require(lv_obj_get_width(label)<=lv_obj_get_width(child),"Bluetooth button caption fits");
            }
        }
    };
    remoteBounds(); save("bluetooth-mouse.ppm");
    click("Ctrl+C"); lastRemoteKey(6,1); click("Ctrl+V"); lastRemoteKey(25,1);
    click("Ctrl+X"); lastRemoteKey(27,1); click("Ctrl+Z"); lastRemoteKey(29,1);
    click("Ctrl+A"); lastRemoteKey(4,1); click("Ctrl+S"); lastRemoteKey(22,1);
    click("Alt+Tab"); lastRemoteKey(43,4); click("Win"); lastRemoteKey(0,8);
    click("Teclas"); remoteBounds(); save("bluetooth-teclas.ppm");
    click("Ctrl"); click("Shift"); save("bluetooth-modificadores.ppm");
    type("c"); lastRemoteKey(6,3);
    type("c"); lastRemoteKey(6,0);
    click("Alt"); click("Alt"); type("a"); lastRemoteKey(4,0);
    click("Ctrl"); click("Alt"); click("Del"); lastRemoteKey(76,5);
    click("Cima"); lastRemoteKey(82,0); click("Baixo"); lastRemoteKey(81,0);
    click("Esq"); lastRemoteKey(80,0); click("Dir"); lastRemoteKey(79,0);
    click("Home"); lastRemoteKey(74,0); click("End"); lastRemoteKey(77,0);
    click("PgUp"); lastRemoteKey(75,0); click("PgDn"); lastRemoteKey(78,0);
    click("Esc"); lastRemoteKey(41,0); click("Tab"); lastRemoteKey(43,0);
    click("Bksp"); lastRemoteKey(42,0); click("Del"); lastRemoteKey(76,0);
    click("Win"); type("e"); lastRemoteKey(8,8);
    click("Ctrl"); input(pda::Action::Move,1,0); input(pda::Action::Activate);
    type("v"); lastRemoteKey(25,1);
    click("Alt"); click("F1-F12"); remoteBounds();
    click("F4"); lastRemoteKey(61,4); click("F4"); lastRemoteKey(61,0);
    for(unsigned i=1;i<=12;++i) { char name[5]; snprintf(name,sizeof(name),"F%u",i); click(name); lastRemoteKey(57+i,0); }
    click("PrtSc"); lastRemoteKey(70,0); click("Ins"); lastRemoteKey(73,0); click("Pause"); lastRemoteKey(72,0);
    save("bluetooth-fn.ppm");
    click("Símbolos"); remoteBounds(); save("bluetooth-simbolos.ppm");
    click("<"); lastRemoteKey(54,2); click(">"); lastRemoteKey(55,2);
    click("~"); lastRemoteKey(53,2); click("="); lastRemoteKey(46,0);
    click("\\"); lastRemoteKey(49,0); click("|"); lastRemoteKey(49,2);
    click("Mais"); remoteBounds(); save("bluetooth-simbolos-mais.ppm");
    click("?"); lastRemoteKey(56,2); click("$"); lastRemoteKey(33,2);
    click("Voltar"); click("Teclas"); click("Ctrl");
    remoteConnected=false; pump(); remoteConnected=true;
    type("a"); lastRemoteKey(4,0);
    click("Shift"); home(); launch("bluetooth"); type("a"); lastRemoteKey(4,0);
    static std::vector<pda::BluetoothPeer> pairedPeers;
    pda::BluetoothPeer pcPeer; pcPeer.id="pc"; pcPeer.name="Computador"; pcPeer.selected=pcPeer.connected=true;
    pda::BluetoothPeer phonePeer; phonePeer.id="phone"; phonePeer.name="Celular";
    pairedPeers={pcPeer,phonePeer};
    static std::string chosenPeer;
    static unsigned pairRequests=0,forgetRequests=0;
    remote.peers=[](){ return pairedPeers; };
    remote.selectPeer=[](const std::string &id) {
        chosenPeer=id; remoteConnected=false;
        for(auto &peer:pairedPeers) { peer.selected=peer.id==id; peer.connected=false; }
        return true;
    };
    remote.pairNew=[](){ ++pairRequests; return pairedPeers.size()<3; };
    remote.renamePeer=[](const std::string &id,const std::string &name) {
        for(auto &peer:pairedPeers) if(peer.id==id) { peer.name=name; return true; } return false;
    };
    remote.forgetPeer=[](const std::string &id) {
        ++forgetRequests; pairedPeers.erase(std::remove_if(pairedPeers.begin(),pairedPeers.end(),[&](const pda::BluetoothPeer &peer){return peer.id==id;}),pairedPeers.end()); return true;
    };
    shell->setBluetoothRemote(remote);
    click("Teclas"); click("Shift"); click("Aparelhos");
    auto peerBounds=[]() {
        auto *body=lv_obj_get_child(lv_scr_act(),1); lv_obj_update_layout(body);
        for(uint32_t i=0;i<lv_obj_get_child_cnt(body);++i) {
            auto *child=lv_obj_get_child(body,i);
            require(lv_obj_get_x(child)>=0 && lv_obj_get_x(child)+lv_obj_get_width(child)<=300 &&
                lv_obj_get_y(child)>=0 && lv_obj_get_y(child)+lv_obj_get_height(child)<=172,"peer controls fit display");
        }
    };
    peerBounds(); save("bluetooth-aparelhos.ppm");
    const auto keysBeforePeerMenu=remoteKeys.size(); const auto inputsBeforePeerMenu=remoteInputs;
    type("wasd"); input(pda::Action::Move,1,0);
    require(remoteKeys.size()==keysBeforePeerMenu && remoteInputs==inputsBeforePeerMenu,"device manager consumes navigation locally");
    click("Celular");
    require(chosenPeer=="phone" && !remoteConnected,"peer selection uses stable identity and waits for host");
    remoteConnected=true; pairedPeers[1].connected=true; pump(600);
    click("Teclado"); type("a"); lastRemoteKey(4,0);
    click("Aparelhos"); click("Nome"); click("Aparelho");
    require(pairedPeers[0].id=="pc" && pairedPeers[0].name=="Aparelho","rename changes label without changing identity");
    peerBounds(); save("bluetooth-aparelho-nome.ppm");
    click("Esquecer"); peerBounds(); save("bluetooth-esquecer-confirmacao.ppm");
    input(pda::Action::Backspace,0,0,8);
    require(pairedPeers.size()==2 && forgetRequests==0,"forget can be cancelled by keyboard");
    for(unsigned i=0;i<4;++i) input(pda::Action::Next,0,0,9);
    input(pda::Action::Activate,0,0,13); // Forget, with Cancel focused by default.
    input(pda::Action::Next,0,0,9); input(pda::Action::Activate,0,0,13);
    require(pairedPeers.size()==1 && pairedPeers[0].id=="phone" && forgetRequests==1,"forget removes only the confirmed identity");
    click("Parear novo"); require(pairRequests==1,"new pairing is explicit");
    input(pda::Action::Backspace,0,0,8);
    type("a"); lastRemoteKey(4,0);
    require(systemState->save(),"Bluetooth session saves");
    pda::System remoteReboot;
    require(remoteReboot.begin(&memory,&platform,0)==pda::LoadResult::Loaded && remoteReboot.state.session.app==int8_t(pda::AppId::Bluetooth),"Bluetooth session reloads");
    save("bluetooth.ppm");
    home(); require(!remoteEnabled,"holding ball exits and disables Bluetooth");
    puts("Bluetooth app routing, controls and persistence OK");
    {
        auto &soundSystem=*systemState;
        const auto ms=soundSystem.state.timerMs;const auto deadline=soundSystem.state.timerDeadline;
        const bool running=soundSystem.state.timerRunning,sound=soundSystem.state.preferences.sound;
        soundSystem.dismissNotification();pump();soundSystem.state.preferences.sound=true;
        for(unsigned way=0;way<3;++way) {
            const auto before=platform.alerts,stops=platform.stops;
            soundSystem.startTimer(1);pump(2);
            require(soundSystem.timerAlertActive()&&!soundSystem.notification().empty(),"timer opens latched alert");
            pump(1500);require(platform.alerts==before+2,"timer repeats while modal stays open");
            if(way==0) input(pda::Action::Activate,0,0,13);
            else if(way==1) {
                pda::CenterButton ball;const auto now=milliseconds;
                ball.update(true,now);ball.update(true,now+25);ball.update(false,now+50);
                require(ball.update(false,now+75)==pda::Action::Activate,"ball click maps to activation");
                input(pda::Action::Activate);
            } else {
                auto *ok=findButton(lv_layer_top(),"OK");require(ok!=nullptr,"timer modal has touch acknowledgment");
                lv_event_send(ok,LV_EVENT_CLICKED,nullptr);pump();
            }
            require(!soundSystem.timerAlertActive()&&soundSystem.notification().empty()&&platform.stops==stops+1,"acknowledgment stops audio and closes timer modal");
            const auto after=platform.alerts;pump(3000);require(platform.alerts==after,"dismissed timer never restarts sound");
        }
        soundSystem.state.timerMs=ms;soundSystem.state.timerDeadline=deadline;
        soundSystem.state.timerRunning=running;soundSystem.state.preferences.sound=sound;soundSystem.changed(true);
    }
#ifdef PDA_CHAT
    std::ifstream weights(PDA_CHAT_MODEL_PATH,std::ios::binary);
    chatWeights.assign(std::istreambuf_iterator<char>(weights),{});
    require(chatModel.begin(chatWeights.data(),chatWeights.size()),"chat model loads");
    shell->setChatClassifier(classifyChat);
    auto &chatSystem=*systemState;
    const auto notesBeforeChat=chatSystem.state.notes.size(),contactsBeforeChat=chatSystem.state.contacts.size(),eventsBeforeChat=chatSystem.state.appointments.size();
    const auto timerBeforeChat=chatSystem.state.timerMs;const bool runningBeforeChat=chatSystem.state.timerRunning;
    const auto deadlineBeforeChat=chatSystem.state.timerDeadline;
    pda::BatteryStats chatBattery;chatBattery.sample(3900);shell->setBatteryStats(chatBattery);
    launch("assistente");require(shell->chatActive(),"chat launches from real catalog");
    require(chatSystem.state.session.app<=12 && chatSystem.state.session.home,"chat never persists unsupported app id");
    save("chat-welcome.ppm");
    shell->setChatClassifier(nullptr);
    type("pode mostrar o nivel da bateria?");click("Enviar");
    require(chatTextPresent(lv_scr_act(),"Bateria estimada: 66%"),"explicit battery variants work without neural acceptance");
    type("poderia me dizer a hora?");click("Enviar");
    require(chatTextPresent(lv_scr_act(),"Agora sao"),"explicit time variants work without neural acceptance");
    click("Limpar");
    type("por favor, crie uma nota sobre comprar pao");click("Enviar");
    require(chatTextPresent(lv_scr_act(),"comprar pao")&&chatTextPresent(lv_scr_act(),"Digite sim"),"conversational note variant uses original payload");
    type("deixa pra la");click("Enviar");
    require(chatSystem.state.notes.size()==notesBeforeChat,"conversational cancellation preserves notes");
    type("cadastre contato Oficina com o telefone 11999998888");click("Enviar");
    require(chatTextPresent(lv_scr_act(),"Criar contato:")&&chatTextPresent(lv_scr_act(),"11999998888"),"natural contact variant previews typed API");
    type("cancelar");click("Enviar");
    type("inicie uma contagem regressiva de vinte e cinco segundos");click("Enviar");
    require(chatTextPresent(lv_scr_act(),"0 min 25 s"),"spoken compound timer through UI");
    type("cancelar");click("Enviar");
    require(chatSystem.state.contacts.size()==contactsBeforeChat&&chatSystem.state.timerDeadline==deadlineBeforeChat&&chatSystem.state.timerRunning==runningBeforeChat,"new variants do not bypass confirmation");
    shell->setChatClassifier(classifyChat);
    click("Limpar");
    type("wasd$");require(shell->chatDraftBytes()==5,"chat preserves WASD as text");save("chat-draft.ppm");
    input(pda::Action::ToggleNavigation);require(shell->chatDraftBytes()==0,"chat clear shortcut");
    type("o que voce faz?");click("Enviar");
    require(chatTextPresent(lv_scr_act(),"Posso fazer no PDA"),"chat capability overview");save("chat-help.ppm");
    type("mais");click("Enviar");require(chatTextPresent(lv_scr_act(),"Ajuda: notas"),"chat help continuation");
    click("Limpar");
    type("quando cai dia 8 de novembro de 2026");click("Enviar");
    require(chatTextPresent(lv_scr_act(),"08/11/2026: domingo"),"calendar weekday reply through real chat");save("chat-calendar-weekday.ppm");
    type("do dia 1 de novembro ate 25 de dezembro sao quantos dias");click("Enviar");
    require(chatTextPresent(lv_scr_act(),"54 dias corridos"),"calendar interval through real chat");save("chat-calendar-interval.ppm");
    type("dia 8 de novembro de 2026 mais 5 dias uteis");click("Enviar");
    require(chatTextPresent(lv_scr_act(),"13/11/2026 (sexta-feira)"),"business-day result through real chat");save("chat-calendar-business.ppm");
    require(chatSystem.state.notes.size()==notesBeforeChat&&chatSystem.state.appointments.size()==eventsBeforeChat,"calendar questions do not create documents");
    click("Limpar");
    type("crie um evento para dia 5 de novembro de 2026 para comprar pao");click("Enviar");
    require(chatTextPresent(lv_scr_act(),"Qual horário?"),"natural event asks missing time in chat");
    require(chatSystem.state.appointments.size()==eventsBeforeChat,"missing time never creates event");
    type("9h");click("Enviar");
    require(chatTextPresent(lv_scr_act(),"05/11/2026 09:00")&&chatTextPresent(lv_scr_act(),"Digite sim"),"time followup presents event confirmation");
    save("chat-event-natural.ppm");
    type("cancelar");click("Enviar");
    require(chatSystem.state.appointments.size()==eventsBeforeChat,"natural event cancellation leaves agenda unchanged");
    click("Limpar");
    type("Quanto tenho de bateria?");input(pda::Action::Activate,0,0,13);
    require(shell->chatLastLabel()==1 && shell->chatDraftBytes()==0,"chat sends and recognizes battery");
    require(chatTextPresent(lv_scr_act(),"Bateria estimada: 66%"),"chat battery answer uses actual supplied telemetry");
    save("chat-battery.ppm");
    input(pda::Action::Activate,0,0,13);require(shell->chatMessageCount()==3,"empty chat does not send");
    type("timer 45s");click("Enviar");require(shell->chatLastLabel()==6,"touch send uses real model");
    save("chat-timer.ppm");
    require(chatSystem.state.timerMs<=timerBeforeChat && chatSystem.state.timerRunning==runningBeforeChat && chatSystem.state.timerDeadline==deadlineBeforeChat,"chat never starts or extends timer");
    type("cancelar");input(pda::Action::Activate,0,0,13);
    type("timer de 5 segundos");click("Enviar");
    require(chatTextPresent(lv_scr_act(),"0 min 5 s"),"timer with connector and full unit reaches action preview");
    require(chatSystem.state.timerDeadline==deadlineBeforeChat && chatSystem.state.timerRunning==runningBeforeChat,"timer variation still waits for confirmation");
    save("chat-timer-variations.ppm");
    type("cancelar");input(pda::Action::Activate,0,0,13);
    type("anote comprar madeira");input(pda::Action::Activate,0,0,13);
    require(chatSystem.state.notes.size()==notesBeforeChat && chatSystem.state.contacts.size()==contactsBeforeChat && chatSystem.state.appointments.size()==eventsBeforeChat,"chat does not create documents");
    require(chatTextPresent(lv_scr_act(),"comprar madeira"),"note preview shows actual payload");
    save("chat-note-preview.ppm");
    type("sim");input(pda::Action::Activate,0,0,13);
    require(chatSystem.state.notes.size()==notesBeforeChat+1 && chatSystem.state.notes.back().text=="comprar madeira","chat confirmation invokes shared note service");
    save("chat-note-saved.ppm");
    type("sim");input(pda::Action::Activate,0,0,13);
    require(chatSystem.state.notes.size()==notesBeforeChat+1,"repeated confirmation cannot duplicate note");
    const auto chatNoteId=chatSystem.state.notes.back().id;
    type(("le nota "+std::to_string(notesBeforeChat+1)).c_str());click("Enviar");
    require(chatTextPresent(lv_scr_act(),"comprar madeira"),"chat reads actual stored note");
    type("edita essa nota: comprar parafusos");click("Enviar");
    require(chatSystem.state.notes.back().text=="comprar madeira","chat edit remains a preview");
    save("chat-note-edit.ppm");type("sim");click("Enviar");
    require(chatSystem.state.notes.back().text=="comprar parafusos","chat edits selected note");
    type("renomeia essa nota para Compras do projeto");click("Enviar");type("sim");click("Enviar");
    require(chatSystem.state.notes.back().title=="Compras do projeto","chat renames selected note");
    type("apaga essa nota");click("Enviar");
    require(chatSystem.state.notes.size()==notesBeforeChat+1,"chat deletion waits for confirmation");save("chat-note-delete.ppm");
    type("cancelar");click("Enviar");
    type(("exclui nota "+std::to_string(notesBeforeChat+1)).c_str());click("Enviar");type("sim");click("Enviar");
    require(chatSystem.state.notes.size()==notesBeforeChat&&!pda::findById(chatSystem.state.notes,chatNoteId),"chat deletes only selected note");
    type("evento Teste Chat 18/11/2026 09:00");click("Enviar");type("sim");click("Enviar");
    require(chatSystem.state.appointments.size()==eventsBeforeChat+1,"chat creates fixture event");
    const auto chatEventId=chatSystem.state.appointments.back().id;
    type(("le evento "+std::to_string(eventsBeforeChat+1)).c_str());click("Enviar");
    require(chatTextPresent(lv_scr_act(),"Teste Chat"),"chat reads actual event");
    type("remarca esse evento para 19/11/2026 10:30");click("Enviar");type("sim");click("Enviar");
    require(pda::formatCivil(chatSystem.state.appointments.back().start,chatSystem.state.preferences.timezoneMinutes)=="19/11/2026 10:30","chat reschedules event");
    type("edita esse evento: levar documentos");click("Enviar");type("sim");click("Enviar");
    require(chatSystem.state.appointments.back().detail=="levar documentos","chat edits event detail");
    type("exclui esse evento");click("Enviar");type("sim");click("Enviar");
    require(chatSystem.state.appointments.size()==eventsBeforeChat&&!pda::findById(chatSystem.state.appointments,chatEventId),"chat deletes selected event");
    type("timer 30s");click("Enviar");type("sim");click("Enviar");
    type("altera o timer para 5 minutos");click("Enviar");type("sim");click("Enviar");
    require(chatSystem.state.timerRunning&&chatSystem.state.timerMs<=300000&&chatSystem.state.timerMs>299000,"chat changes remaining timer duration");
    type("cancela o timer");click("Enviar");type("sim");click("Enviar");
    require(!chatSystem.state.timerRunning&&!chatSystem.state.timerMs,"chat cancels timer");
    chatSystem.state.timerMs=timerBeforeChat;chatSystem.state.timerRunning=runningBeforeChat;chatSystem.state.timerDeadline=deadlineBeforeChat;
    type("q horas sao");input(pda::Action::Activate,0,0,13);
    char chatTime[40];const auto chatLocal=pda::fromUtc(chatSystem.utc(),chatSystem.state.preferences.timezoneMinutes);
    snprintf(chatTime,sizeof(chatTime),"Agora sao %02d:%02d",chatLocal.hour,chatLocal.minute);
    require(chatTextPresent(lv_scr_act(),chatTime),"chat time answer uses device clock and timezone");
    const auto savedEpoch=platform.epoch;platform.epoch=0;
    type("q horas sao");input(pda::Action::Activate,0,0,13);
    require(chatTextPresent(lv_scr_act(),"não tem um horário válido"),"chat does not invent unavailable time");platform.epoch=savedEpoch;
    for(int i=0;i<8;++i) {type("q horas sao");input(pda::Action::Activate,0,0,13);}
    require(shell->chatMessageCount()==12,"chat history is bounded");
    type("rascunho");home();launch("assistente");require(shell->chatDraftBytes()==8,"unsent draft survives app switch in RAM");
    click("Limpar");require(shell->chatMessageCount()==1 && shell->chatDraftBytes()==0,"touch clear resets chat only");
    type(std::string(81,'x').c_str());require(shell->chatDraftBytes()==80,"chat physical input bounded to 80 bytes");
    click("Limpar");std::string multibyte;for(int i=0;i<41;++i) multibyte+="á";fill(0,multibyte.c_str());
    input(pda::Action::Activate,0,0,13);
    require(shell->chatMessageCount()==1 && shell->chatDraftBytes()==82,"UTF8 byte overflow is rejected without discarding draft");
    click("Limpar");type("anote chat-privado-abc908");input(pda::Action::Activate,0,0,13);
    require(chatSystem.save(),"save while chat open succeeds");
    for (const auto &slot:memory.slots) require(std::string(slot.begin(),slot.end()).find("chat-privado-abc908")==std::string::npos,"chat transcript absent from persisted data");
    click("Limpar");home();require(chatSystem.save(),"chat-safe session saves");
    pda::System chatReboot;require(chatReboot.begin(&memory,&platform,0)==pda::LoadResult::Loaded,"chat session remains compatible with original storage schema");
    puts("chat: real model, keyboard/touch, note confirmation, transient history and storage compatibility OK");
#endif
    fontReferenceCaptures();
    unsigned unique=0; for(const auto &entry:checkedGlyphs) unique+=unsigned(entry.second.size());
    fprintf(glyphReport,"# %u unique font/codepoint pairs; %u glyph checks; %u visible labels; zero missing/clipped glyphs\n",unique,glyphChecks,labelChecks);
    fclose(glyphReport);
    printf("font audit: %u font/codepoint pairs, %u checks, %u labels; no missing or clipped glyphs\n",unique,glyphChecks,labelChecks);
    puts("preview: all apps, creation/editing, symbols, formulas, timer, autosave, touch callbacks and restored editor OK");
}
