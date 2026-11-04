#include "pda/workspace.h"
#include "pda/ui_style.h"
#include "pda/more_games.h"
#include <algorithm>
#include <stdio.h>

namespace pda {
lv_obj_t *Workspace::gameTile(const std::string &text, int x, int y, int width, int height, uint32_t background, uint32_t ink, int cell) {
    auto *tile = lv_obj_create(body_);
    lv_obj_set_pos(tile, x, y); lv_obj_set_size(tile, width, height);
    lv_obj_set_style_pad_all(tile, 0, 0); lv_obj_set_style_radius(tile, 2, 0);
    lv_obj_set_style_border_width(tile, 0, 0); lv_obj_set_style_bg_color(tile, lv_color_hex(background), 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    if (cell >= 0) {
        tags_.push_back({Command::GameCell, cell}); lv_obj_set_user_data(tile, &tags_.back());
        lv_obj_add_event_cb(tile, clicked, LV_EVENT_CLICKED, this);
    } else lv_obj_clear_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    if (!text.empty()) {
        auto *caption = lv_label_create(tile); lv_label_set_text(caption, text.c_str());
        lv_obj_set_style_text_font(caption, &pda_font, 0); lv_obj_set_style_text_color(caption, lv_color_hex(ink), 0); lv_obj_center(caption);
    }
    return tile;
}
void Workspace::gameReset(bool next) {
    auto &v = sys_.state.session;
    if (v.document < 1 || v.document > games::count) return;
    const auto kind = games::Kind(v.document - 1); auto &g = sys_.state.games.slots[v.document - 1];
    const uint32_t seed = g.seed * 1664525u + 1013904223u + sys_.now();
    unsigned level = g.level + (next ? 1 : 0);
    if (kind == games::Kind::Hangman) {
        level = seed % games::levelCount(kind);
        if (g.started && level == g.level) level = (level + 1) % games::levelCount(kind);
    }
    if(kind==games::Kind::WordSearch) {
        level=seed%games::extraLevels(kind);
        if(g.started&&level==g.level) level=(level+1)%games::extraLevels(kind);
    }
    games::reset(kind, g, seed, level); sys_.changed(true);
    lastGameStep_=sys_.now();
}
void Workspace::pauseSnake() {
    if(app_==AppId::Games&&sys_.state.session.document==7&&games::snakePause(sys_.state.games.slots[6])) sys_.changed(true);
}
void Workspace::gameHandle(InputEvent event) {
    const unsigned index = sys_.state.session.document - 1;
    if (index >= games::count) return;
    const auto kind = games::Kind(index); auto &g = sys_.state.games.slots[index];
    if (kind == games::Kind::Hangman) {
        bool changed = false;
        if (event.action == Action::Activate) pending_ = Command::GameMenu;
        else if (event.action == Action::Backspace) pending_ = Command::GameNew;
        else if (event.action == Action::Text) {
            if (event.character == '?') pending_ = Command::GameHelp;
            else if (event.character == ' ' && g.status) { gameReset(); render(); return; }
            else changed = games::guess(g, char(event.character));
        }
        if (changed) { sys_.changed(); render(); }
        return;
    }
    wasdMovement(event); bool changed = false;
    if (event.action == Action::Move) changed = games::move(kind, g, event.x, event.y);
    else if (event.action == Action::Activate) changed = games::activate(kind, g);
    else if (event.action == Action::Backspace && kind == games::Kind::Sudoku) changed = games::digit(g, 0);
    else if (event.action == Action::Text) {
        const char key = char(event.character);
        if (key == 'm' || key == 'M') pending_ = Command::GameMenu;
        else if (key == 'n' || key == 'N') pending_ = Command::GameNew;
        else if (key == '?') pending_ = Command::GameHelp;
        else if ((key == 'u' || key == 'U') && (kind==games::Kind::Merge||kind==games::Kind::Warehouse||kind==games::Kind::Sudoku)) changed = games::undo(g);
        else if (kind==games::Kind::Dungeon && (key=='q'||key=='Q'||key=='x'||key=='X'||key=='f'||key=='F')) changed=games::dungeonAction(g,key);
        else if ((key == 'r' || key == 'R') && games::extra(kind) && kind!=games::Kind::Dungeon) changed = games::extraSpecial(kind,g);
        else if ((key == 'h' || key == 'H') && kind==games::Kind::Dungeon) changed=games::extraSpecial(kind,g);
        else if ((key == 'f' || key == 'F') && kind == games::Kind::Mines) changed = games::special(kind, g);
        else if ((key == 'h' || key == 'H') && kind == games::Kind::Sudoku) changed = games::special(kind, g);
        else if (key == ' ') changed = games::extra(kind)?games::extraSpace(kind,g):games::activate(kind, g);
        else if (key >= '0' && key <= '9' && kind == games::Kind::Sudoku) changed = games::digit(g, unsigned(key - '0'));
        else if (key >= '1' && key <= '6' && kind == games::Kind::Code) changed = games::codeDigit(g,unsigned(key-'0'));
    }
    if (changed) { if(kind==games::Kind::Snake&&event.action!=Action::Move) lastGameStep_=sys_.now(); sys_.changed(); render(); }
}
void Workspace::gameScreen() {
    auto &v = sys_.state.session; v.view = 8; v.editing = false; v.form = 0;
    if (v.document > games::count) v.document = 0;
    if (!v.document) {
        for (unsigned i = 0; i < games::count; ++i) {
            const auto &game = sys_.state.games.slots[i];
            const std::string title = std::to_string((i+1)%10) + "  " + games::name(games::Kind(i)) + (game.started ? " / continuar" : "");
            button(body_, title, 0, int(i) * 29, 292, Command::GameSelect, int(i), 26);
        }
        label(body_, "Partidas salvas ao sair. Escolha um jogo.", 0, int(games::count)*29+1, 294);
        return;
    }
    const auto kind = games::Kind(v.document - 1); auto &g = sys_.state.games.slots[v.document - 1];
    if (!g.started) gameReset();
    lv_obj_set_style_pad_all(body_,0,0);
    if(kind==games::Kind::Dungeon) { extraGameScreen(); return; }
    button(body_, "Jogos", 4, 0, 61, Command::GameMenu, 0, 22);
    button(body_, "Nova", 70, 0, 53, Command::GameNew, 0, 22);
    button(body_, "Ajuda", 128, 0, 60, Command::GameHelp, 0, 22);
    if (kind == games::Kind::Hangman) { hangmanScreen(); return; }
    if (games::extra(kind)) { extraGameScreen(); return; }
    button(body_, kind == games::Kind::Mines ? "Bandeira" : kind == games::Kind::Sudoku ? "Dica" : "Desfazer", 194, 0, 120, Command::GameSpecial, 0, 22);
    const std::string result = g.status == 1 ? "Você venceu!" : g.status == 2 ? "Fim de jogo" : "";
    if (kind == games::Kind::Merge) {
        static const uint32_t colors[] = {0xD7E2D1, 0xEEE9D6, 0xE7D7AA, 0xEFC487, 0xE4A268, 0xD77B52, 0xBB563A, 0xC2B753, 0xA6A13E, 0x898934, 0x677D39, 0x365F43};
        for (unsigned i = 0; i < 16; ++i) {
            const unsigned power = unsigned(g.board[i] - 'a');
            const std::string number = power ? std::to_string(uint32_t(1) << power) : "";
            auto *tile=gameTile(number, 3 + int(i % 4) * 79, 26 + int(i / 4) * 53, 76, 50, colors[std::min<unsigned>(power, 11)], power >= 5 ? 0xFFFFFF : style::ink);
            if(!number.empty()) lv_obj_set_style_text_font(lv_obj_get_child(tile,0),number.size()<5?&lv_font_montserrat_28:number.size()<8?&lv_font_montserrat_16:&lv_font_montserrat_12,0);
        }
        if (g.status == 2) {
            auto *banner = gameTile(result, 60, 115, 200, 30, style::ink, 0xFFFFFF);
            lv_obj_set_style_bg_opa(banner, LV_OPA_90, 0);
        }
        return;
    }
    if (kind == games::Kind::Warehouse) {
        const unsigned width=games::warehouseWidth(g); const bool large=g.board.size()>64;
        const int stepX=large?19:39,stepY=large?17:26,left=(320-int(width)*stepX)/2;
        for (unsigned i = 0; i < g.board.size(); ++i) {
            const char c = g.board[i]; const bool target = c == '.' || c == '*' || c == '+';
            const std::string text = c == '@' || c == '+' ? "@" : c == '$' || c == '*' ? "O" : target ? "+" : "";
            const uint32_t bg = c == '#' ? style::ink : c == '*' ? 0x83A85E : c == '$' ? 0xD5A152 : target ? 0xBED9A1 : 0xE1E8DA;
            auto *tile=gameTile(text,left+int(i%width)*stepX,27+int(i/width)*stepY,stepX-1,stepY-1,bg,style::ink);
            if(!text.empty()) lv_obj_set_style_text_font(lv_obj_get_child(tile,0),large?&lv_font_montserrat_12:&lv_font_montserrat_16,0);
        }
        if(g.status==1) {
            auto *banner=gameTile("Venceu! Enter: próxima",50,112,220,30,style::ink,0xFFFFFF);
            lv_obj_set_style_bg_opa(banner,LV_OPA_90,0);
        }
        return;
    }
    const bool sudoku = kind == games::Kind::Sudoku;
    const unsigned width=sudoku?9:games::mineWidth(g);
    for (unsigned i = 0; i < g.board.size(); ++i) {
        std::string text; uint32_t bg = style::card, ink = style::ink;
        if (sudoku) {
            text = g.board[i] == '0' ? "" : std::string(1, g.board[i]);
            if (g.marks[i] != '0') bg = 0xD3DDCD; else ink = 0x21649C;
            if (games::conflict(g, i)) ink = 0xB82424;
        } else {
            const char mark = g.marks[i];
            if (mark == 'f') { text = "F"; ink = 0xAD382A; }
            else if (mark == 'r' || (g.status == 2 && g.board[i] == '*')) {
                text = g.board[i] == '0' ? "" : std::string(1, g.board[i]); bg = style::paper;
                ink = g.board[i] == '*' ? 0xB82424 : g.board[i] == '1' ? 0x21649C : style::ink;
            }
            // Covered cells are drawn, never represented by a font glyph.
            if(mark!='r'&&!(g.status==2&&g.board[i]=='*')) bg=0xC4D1C2;
        }
        if (i == g.cursor) { bg = style::ink; if (ink != 0xB82424) ink = 0xFFFFFF; }
        const int x = sudoku?4+int(i % 9)*20+int(i % 9 / 3)*2:(320-int(width)*(g.board.size()==81?35:19))/2+int(i%width)*(g.board.size()==81?35:19);
        const int y = sudoku?32+int(i / 9)*20+int(i / 27)*2:27+int(i/width)*(g.board.size()==81?23:16);
        auto *tile = gameTile(text, x, y, sudoku?19:g.board.size()==81?34:18, sudoku?19:g.board.size()==81?22:15, bg, ink, int(i));
        if(!sudoku&&g.marks[i]!='r'&&!(g.status==2&&g.board[i]=='*')) {
            lv_obj_set_style_border_width(tile,1,0);
            lv_obj_set_style_border_side(tile,LV_BORDER_SIDE_BOTTOM|LV_BORDER_SIDE_RIGHT,0);
            lv_obj_set_style_border_color(tile,lv_color_hex(0x879C84),0);
        }
        if(!sudoku&&g.board.size()>81&&!text.empty()) lv_obj_set_style_text_font(lv_obj_get_child(tile,0),&lv_font_montserrat_12,0);
        if (i == g.cursor) { lv_obj_set_style_outline_width(tile, 1, 0); lv_obj_set_style_outline_color(tile, lv_color_hex(0xD5A152), 0); lv_obj_set_style_outline_pad(tile, 0, 0); }
    }
    if (sudoku) {
        const std::string cell = "L" + std::to_string(g.cursor / 9 + 1) + " C" + std::to_string(g.cursor % 9 + 1);
        label(body_, "Sudoku " + std::to_string(g.level + 1) + "/16\n" + cell + (g.marks[g.cursor] != '0' ? " fixo" : "") +
            "\n\n1-9: preencher\n0: apagar\nH: dica\nU: desfazer", 198, 32, 118);
        if (g.status == 1) label(body_, "Venceu!\nEnter: próxima", 198, 176, 118);
    } else if(!result.empty()) {
        auto *banner=gameTile(result,60,112,200,30,style::ink,0xFFFFFF);
        lv_obj_set_style_bg_opa(banner,LV_OPA_90,0);
    }
}
void Workspace::gameHelp(unsigned tab) {
    const unsigned document=sys_.state.session.document;
    if(app_!=AppId::Games||document<1||document>games::count) return;
    const auto kind=games::Kind(document-1); const auto &g=sys_.state.games.slots[document-1];
    if(kind!=games::Kind::WordSearch&&kind!=games::Kind::Mines&&kind!=games::Kind::Merge&&kind!=games::Kind::Warehouse) { message(games::help(kind)); return; }
    pauseSnake(); checkpoint(); modal_=Modal::Message; rendering_=true; clear();
    lv_obj_set_style_pad_all(body_,4,0);
    const bool warehouse=kind==games::Kind::Warehouse;
    auto *summary=button(body_,kind==games::Kind::WordSearch?"Palavras":"Partida",0,0,warehouse?96:148,Command::GameHelpTab,0,26);
    auto *controls=button(body_,"Controles",warehouse?104:156,0,warehouse?96:148,Command::GameHelpTab,1,26);
    auto *phases=warehouse?button(body_,"Fases",208,0,96,Command::GameHelpTab,2,26):nullptr;
    auto *active=warehouse&&tab==2?phases:tab==1?controls:summary;
    lv_obj_set_style_bg_color(active,lv_color_hex(style::selected),0);
    lv_obj_set_style_text_color(active,lv_color_hex(0xFFFFFF),0);
    if(warehouse&&tab==2) {
        for(unsigned i=0;i<games::levelCount(kind);++i) {
            auto *phase=button(body_,std::to_string(i+1),4+int(i%6)*50,42+int(i/6)*31,46,Command::GameWarehouseLevel,int(i),26);
            if(i==g.level) { lv_obj_set_style_bg_color(phase,lv_color_hex(style::selected),0); lv_obj_set_style_text_color(phase,lv_color_hex(0xFFFFFF),0); }
        }
    } else if(tab==1) label(body_,games::help(kind),4,40,300);
    else if(kind==games::Kind::WordSearch) {
        const unsigned count=games::searchCount(g),rows=(count+1)/2;
        unsigned found=0; for(unsigned i=0;i<count;++i) if(g.score&(1u<<i)) ++found;
        label(body_,games::wordCategory(g.level*16),4,38,300);
        label(body_,"Encontradas: "+std::to_string(found)+"/"+std::to_string(count),4,60,300);
        for(unsigned i=0;i<count;++i) {
            const bool done=(g.score&(1u<<i))!=0;
            auto *word=label(body_,std::string(done?"+ ":"  ")+games::searchWord(g,i),4+int(i/rows)*152,84+int(i%rows)*18,148);
            if(done) lv_obj_set_style_text_color(word,lv_color_hex(style::selected),0);
        }
    } else if(warehouse) {
        const unsigned boxes=unsigned(std::count(g.board.begin(),g.board.end(),'$')+std::count(g.board.begin(),g.board.end(),'*'));
        label(body_,"Sokoban - fase "+std::to_string(g.level+1)+"/"+std::to_string(games::levelCount(kind))+"\nGrade: "+std::to_string(games::warehouseWidth(g))+"x"+std::to_string(games::warehouseHeight(g))+"\nPassos: "+std::to_string(g.moves)+"\nCaixas no alvo: "+std::to_string(std::count(g.board.begin(),g.board.end(),'*'))+"/"+std::to_string(boxes)+(g.status==1?"\nVocê venceu!":""),4,40,300);
    } else if(kind==games::Kind::Merge) {
        label(body_,"2048 - grade 4x4\nPontos: "+std::to_string(g.score)+"\nRecorde: "+std::to_string(g.best)+"\nJogadas: "+std::to_string(g.moves)+
            (g.status==2?"\nFim de jogo":g.status==1?"\n2048 alcançado!":""),4,40,300);
    } else {
        label(body_,"Campo Minado - "+std::to_string(games::mineWidth(g))+"x"+std::to_string(games::mineHeight(g))+"\nMinas: "+std::to_string(games::mineCount(g))+"\nBandeiras: "+std::to_string(games::flags(g))+"\nSeguras: "+std::to_string(g.score)+"/"+std::to_string(g.board.size()-games::mineCount(g))+
            (g.status==1?"\nVocê venceu!":g.status==2?"\nFim de jogo":""),4,40,300);
    }
    auto *back=button(body_,"Voltar",0,180,304,Command::Cancel,0,26);
    lv_group_focus_obj(back); rendering_=false; refreshFooter();
}
void Workspace::hangmanScreen() {
    const auto &g = sys_.state.games.slots[unsigned(games::Kind::Hangman)];
    label(body_, "Forca", 218, 3, 96);
    auto *drawing = lv_obj_create(body_); lv_obj_remove_style_all(drawing);
    lv_obj_set_pos(drawing, 6, 27); lv_obj_set_size(drawing, 112, 104);
    lv_obj_clear_flag(drawing, LV_OBJ_FLAG_CLICKABLE); lv_obj_clear_flag(drawing, LV_OBJ_FLAG_SCROLLABLE);
    static const lv_point_t gallows[][2] = {
        {{8, 99}, {108, 99}}, {{22, 99}, {22, 4}}, {{22, 4}, {80, 4}}, {{80, 4}, {80, 19}}
    };
    static const lv_point_t figure[][2] = {
        {{80, 42}, {80, 68}}, {{80, 48}, {61, 61}}, {{80, 48}, {99, 61}},
        {{80, 68}, {62, 92}}, {{80, 68}, {98, 92}}
    };
    auto line = [&](const lv_point_t *points) {
        auto *object = lv_line_create(drawing); lv_line_set_points(object, points, 2);
        lv_obj_set_style_line_color(object, lv_color_hex(style::ink), 0);
        lv_obj_set_style_line_width(object, 2, 0); lv_obj_clear_flag(object, LV_OBJ_FLAG_CLICKABLE);
    };
    for (const auto &points : gallows) line(points);
    if (g.score >= 1) {
        auto *head = gameTile("", 0, 0, 24, 24, style::paper, style::ink);
        lv_obj_set_parent(head, drawing); lv_obj_set_pos(head, 68, 19);
        lv_obj_set_style_radius(head, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_border_width(head, 2, 0);
        lv_obj_set_style_border_color(head, lv_color_hex(style::ink), 0);
    }
    for (unsigned i = 0; i < 5 && i + 2 <= g.score; ++i) line(figure[i]);
    label(body_, games::hangmanCategory(g), 128, 30, 186);
    label(body_, "Erros: " + std::to_string(g.score) + "/6  Vitórias: " + std::to_string(g.best), 128, 52, 186);
    for(unsigned i=0;i<6;++i) gameTile("",128+int(i)*30,76,26,7,i<6-g.score?style::selected:0xB82424,style::ink);
    label(body_, g.status == 1 ? "Acertou!\nEspaço: próxima" : g.status == 2 ? "Fim da rodada\nEspaço: próxima" : "Digite uma letra\nBackspace: nova", 128, 92, 186);
    auto *word = label(body_, games::hangmanDisplay(g, g.status == 2), 5, 137, 310);
    lv_obj_set_style_text_align(word, LV_TEXT_ALIGN_CENTER, 0);
    const auto answer=lowerAscii(g.board);
    for(unsigned i=0;i<26;++i) {
        const char c=char('a'+i); const bool used=g.marks.find(c)!=std::string::npos;
        const uint32_t bg=!used?style::card:answer.find(c)!=std::string::npos?style::selected:0xB82424;
        gameTile(std::string(1,char('A'+i)),5+int(i%13)*24,166+int(i/13)*25,22,22,bg,used?0xFFFFFF:style::ink);
    }
}
}
