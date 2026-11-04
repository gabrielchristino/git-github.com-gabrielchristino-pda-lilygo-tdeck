#include "pda/workspace.h"
#include "pda/more_games.h"
#include "pda/ui_style.h"
#include <algorithm>

namespace pda {
void Workspace::extraGameScreen() {
    const auto kind=games::Kind(sys_.state.session.document-1); const auto &g=sys_.state.games.slots[sys_.state.session.document-1];
    const char *action=kind==games::Kind::Naval?"Auto":kind==games::Kind::Snake?"Pausa":kind==games::Kind::Code?"Limpar":kind==games::Kind::WordSearch?"Cancelar":"Poção";
    if(kind!=games::Kind::Dungeon) button(body_,action,194,0,120,Command::GameSpecial,0,22);
    if(kind==games::Kind::Naval) {
        static const unsigned lengths[]={4,3,3,2};
        std::string phase=g.level<4?"Barco "+std::to_string(lengths[g.level])+" | Espaço: gira":g.status==1?"Você venceu!":g.status==2?"O PDA venceu":"Batalha Naval | Acertos "+std::to_string(g.score)+"/12";
        auto *heading=label(body_,phase,5,25,310); lv_label_set_long_mode(heading,LV_LABEL_LONG_DOT); lv_obj_set_height(heading,18);
        label(body_,"Sua frota",5,44,150); label(body_,"Adversário",165,44,150);
        for(unsigned half=0;half<2;++half) for(unsigned i=0;i<64;++i) {
            const unsigned p=half*64+i; const char shot=g.marks[p];
            const bool active=(g.level<4?half==0:half==1),selected=active&&i==g.cursor;
            bool preview=false;
            if(g.level<4&&half==0) for(unsigned n=0;n<lengths[g.level];++n) if(i==g.cursor+n*(g.undoCursor?8:1)) preview=true;
            std::string text=shot=='x'?"X":shot=='o'?"·":"";
            uint32_t bg=half==0&&g.board[p]!='.'?0x8CADA1:0xDDE7E7;
            if(half==1&&g.status&&g.board[p]!='.') bg=0x8CADA1;
            if(preview) bg=0xD5A152;
            if(selected) bg=style::ink;
            gameTile(text,5+int(half)*160+int(i%8)*19,63+int(i/8)*19,18,18,bg,selected?0xFFFFFF:shot=='x'?0xB82424:0x21649C,active?int(i):-1);
        }
        return;
    }
    if(kind==games::Kind::Snake) {
        std::string title="Cobrinha  "+std::to_string(g.score)+"  Recorde: "+std::to_string(g.best);
        if(g.status) title+=g.status==1?"  Venceu!":"  Fim!"; else title+=g.undoStatus?"  Pausada":"";
        auto *heading=label(body_,title,5,26,310); lv_label_set_long_mode(heading,LV_LABEL_LONG_DOT); lv_obj_set_height(heading,18);
        const auto body=games::snakeBody(g);
        unsigned fruit=128;
        if(!g.marks.empty()) { const char *digits="0123456789abcdef"; fruit=unsigned(std::find(digits,digits+16,g.marks[0])-digits)*16+unsigned(std::find(digits,digits+16,g.marks[1])-digits); }
        for(unsigned i=0;i<128;++i) {
            const bool segment=std::find(body.begin(),body.end(),i)!=body.end();
            auto *tile=gameTile(i==body[0]?"@":"",8+int(i%16)*19,50+int(i/16)*20,18,19,i==fruit?0xC45239:i==body[0]?0x183F34:segment?0x6A9850:0xDCE5D5,0xFFFFFF);
            if(i==fruit) lv_obj_set_style_radius(tile,LV_RADIUS_CIRCLE,0);
        }
        return;
    }
    if(kind==games::Kind::Code) {
        label(body_,"Senha Secreta",5,26,310);
        for(unsigned i=0;i<4;++i) {
            auto *tile=gameTile(std::string(1,g.undoBoard[i]),5+int(i)*43,49,39,36,i==g.cursor?style::ink:style::card,i==g.cursor?0xFFFFFF:style::ink);
            lv_obj_set_style_text_font(lv_obj_get_child(tile,0),&lv_font_montserrat_28,0);
        }
        label(body_,g.status==1?"Acertou!":g.status==2?"Era: "+g.board:"Tentativa\n"+std::to_string(g.moves+1)+"/10",190,49,125);
        label(body_,"= lugar certo | ~ outro lugar",5,87,310);
        for(unsigned i=0;i<10;++i) {
            const int x=5+int(i/5)*160,y=110+int(i%5)*21;
            if(i<g.moves) {
                const auto row=g.marks.substr(i*6,6);
                label(body_,std::to_string(i+1)+": "+row.substr(0,4)+"  ="+row[4]+" ~"+row[5],x,y,154);
            } else label(body_,std::to_string(i+1)+": _ _ _ _",x,y,154);
        }
        return;
    }
    if(kind==games::Kind::WordSearch) {
        const auto path=g.undoStatus?games::searchPath(g,g.undoCursor,g.cursor):std::vector<unsigned>();
        const unsigned width=games::searchWidth(g); const bool large=g.board.size()>80;
        for(unsigned i=0;i<g.board.size();++i) {
            uint32_t bg=g.undoBoard[i]=='1'?0xA3C48B:style::card;
            if(std::find(path.begin(),path.end(),i)!=path.end()) bg=0xD5A152;
            if(i==g.cursor) bg=style::ink;
            auto *tile=gameTile(std::string(1,g.board[i]),(320-int(width)*(large?19:31))/2+int(i%width)*(large?19:31),27+int(i/width)*(large?16:26),large?18:30,large?15:25,bg,i==g.cursor?0xFFFFFF:style::ink,int(i));
            lv_obj_set_style_text_font(lv_obj_get_child(tile,0),large?&lv_font_montserrat_12:&lv_font_montserrat_16,0);
        }
        if(g.status) {
            auto *banner=gameTile("Você venceu!",60,112,200,30,style::ink,0xFFFFFF);
            lv_obj_set_style_bg_opa(banner,LV_OPA_90,0);
        }
        return;
    }
    button(body_,"Jogos",4,0,61,Command::GameMenu,0,20);
    button(body_,"Nova",70,0,53,Command::GameNew,0,20);
    button(body_,"Ajuda",128,0,60,Command::GameHelp,0,20);
    button(body_,"H: cura",194,0,120,Command::GameSpecial,0,20);
    auto *health=lv_bar_create(body_); lv_obj_set_pos(health,5,24); lv_obj_set_size(health,64,10);
    lv_bar_set_range(health,0,20); lv_bar_set_value(health,int(g.undoScore),LV_ANIM_OFF);
    lv_obj_set_style_radius(health,2,LV_PART_MAIN); lv_obj_set_style_radius(health,2,LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(health,lv_color_hex(style::line),LV_PART_MAIN);
    lv_obj_set_style_bg_color(health,lv_color_hex(g.undoScore<=5?0xB82424:style::selected),LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(health,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_bg_opa(health,LV_OPA_COVER,LV_PART_INDICATOR);
    lv_obj_clear_flag(health,LV_OBJ_FLAG_CLICKABLE);
    auto *life=label(body_,std::to_string(g.undoScore)+"/20",74,22,44);
    lv_obj_set_style_text_font(life,&lv_font_montserrat_12,0);
    const std::string stats=std::to_string(g.level+1)+"/3 H:"+std::to_string(g.undoMoves)+
        " Q:"+std::to_string(games::dungeonSupply(g,1))+" X:"+std::to_string(games::dungeonSupply(g,0))+"  $"+std::to_string(g.score)+
        (g.undoSeed?" S:"+std::to_string(g.undoSeed):"");
    auto *hud=label(body_,stats,122,22,194); lv_obj_set_style_text_font(hud,&lv_font_montserrat_12,0);
    lv_label_set_long_mode(hud,LV_LABEL_LONG_DOT); lv_obj_set_height(hud,15);
    const unsigned width=games::dungeonWidth(g),height=games::dungeonHeight(g);
    const int left=(320-int(width)*16)/2,top=38+(13-int(height))*7;
    for(unsigned i=0;i<g.board.size();++i) {
        const char c=g.board[i]; const bool seen=g.marks[i]=='1';
        const char glyph=c=='E'?'g':c=='B'?'m':c=='O'||c=='o'?'o':c=='T'?'t':c=='H'?'+':c=='P'?'p':c=='Q'?'e':c=='X'?'b':c=='>'?'>':' ';
        std::string text=!seen?"":i==g.cursor?"@":glyph==' '?"":std::string(1,glyph);
        uint32_t bg=!seen?0xB6C3AD:c=='#'?style::ink:style::card;
        if(i==g.cursor) bg=g.undoSeed?0x8ABBD2:0xD5A152;
        const uint32_t ink=games::dungeonEnemy(c)?(c=='B'?0x7950A0:c=='O'||c=='o'?0x963E24:0xB82424):style::ink;
        auto *tile=gameTile(text,left+int(i%width)*16,top+int(i/width)*14,15,13,bg,ink);
        if(lv_obj_get_child_cnt(tile)) {
            auto *caption=lv_obj_get_child(tile,0);
            lv_obj_set_style_text_font(caption,&lv_font_montserrat_12,0);
            // Montserrat 12 uses a 15 px line; keep the @ descender inside 13 px tiles.
            lv_obj_align(caption,LV_ALIGN_CENTER,0,-1);
        }
    }
    const char *message=!sys_.store.ready()?"Sem salvar: abra Ajustes":!sys_.store.error().empty()?"Erro ao salvar: abra Ajustes":games::dungeonMessage(g);
    auto *notice=label(body_,message,5,222,310);
    lv_label_set_long_mode(notice,LV_LABEL_LONG_DOT); lv_obj_set_height(notice,16);
}
}
