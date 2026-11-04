#include "pda/workspace.h"
#include "pda/ui_style.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdio.h>

namespace pda {
namespace {
void text(lv_draw_ctx_t *ctx, int x, int y, int width, const char *value, uint32_t color=0x263442) {
    lv_draw_label_dsc_t d; lv_draw_label_dsc_init(&d); d.font=&pda_font; d.color=lv_color_hex(color);
    lv_area_t a={lv_coord_t(x),lv_coord_t(y),lv_coord_t(x+width-1),lv_coord_t(y+17)};
    lv_draw_label(ctx,&d,&a,value,nullptr);
}
void rectangle(lv_draw_ctx_t *ctx,int x,int y,int w,int h,uint32_t color) {
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d); d.bg_color=lv_color_hex(color); d.bg_opa=LV_OPA_COVER;
    d.border_width=0; d.radius=0;
    lv_area_t a={lv_coord_t(x),lv_coord_t(y),lv_coord_t(x+w-1),lv_coord_t(y+h-1)};
    lv_draw_rect(ctx,&d,&a);
}
void line(lv_draw_ctx_t *ctx,int x0,int y0,int x1,int y1,uint32_t color,int width=1) {
    lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d); d.color=lv_color_hex(color); d.width=width;
    lv_point_t a={lv_coord_t(x0),lv_coord_t(y0)},b={lv_coord_t(x1),lv_coord_t(y1)};
    lv_draw_line(ctx,&d,&a,&b);
}
bool parse(const std::string &s,int32_t &v) {
    if(s.empty() || s.size()>8) return false;
    char *end=nullptr; const long value=strtol(s.c_str(),&end,10);
    if(!end || *end || value < -32768 || value > 32767) return false;
    v=int32_t(value); return true;
}
struct MapStroke { int16_t x0,y0,x1,y1; uint8_t kind,width; };
struct MapLabel { int x,y,width; uint32_t name; uint8_t kind; };
struct MapDrawCache {
    int32_t x=0,y=0;
    int width=0,height=0,scale=0;
    int32_t labelX=0,labelY=0;
    int labelWidth=0,labelHeight=0,labelScale=0;
    std::vector<uint32_t> candidates;
    std::vector<MapStroke> strokes;
    std::vector<MapLabel> labels;
} mapCache;
maps::RenderStats mapStats;
constexpr int mapMargin=96;
void prepareMap(const maps::View &view,int w,int h,bool labelsWanted) {
    const int scale=view.metersPerPixel();
    const bool rebuild=mapCache.width!=w || mapCache.height!=h || mapCache.scale!=scale ||
        std::abs(mapCache.x-view.center.x)>mapMargin*scale || std::abs(mapCache.y-view.center.y)>mapMargin*scale;
    if(rebuild) {
        const uint32_t started=lv_tick_get();
        mapCache.width=w; mapCache.height=h; mapCache.scale=scale;
        mapCache.x=view.center.x; mapCache.y=view.center.y; mapCache.labelWidth=0;
        mapCache.strokes.clear();
        const int cw=w+2*mapMargin,ch=h+2*mapMargin;
        const int hx=(cw/2+1)*scale,hy=(ch/2+1)*scale;
        maps::queryFeatures({view.center.x-hx,view.center.y-hy,view.center.x+hx,view.center.y+hy},mapCache.candidates);
        // Keep an off-screen margin of vectors. Small pans only translate this
        // list; the spatial index is queried again after crossing the margin.
        const int layers[]={0,5,1,4,2,3};
        for(int kind:layers) {
            if((kind==1 && scale>16)||(kind==4 && scale>8)||(kind==5 && scale>64)) continue;
            for(uint32_t i:mapCache.candidates) {
                const auto &f=maps::features[i];
                if(f.kind!=kind) continue;
                const int width=kind==0?std::max(2,std::min(10,70/scale)):kind==3?3:kind==2?2:1;
                for(unsigned j=1;j<f.count;++j) {
                    int x0,y0,x1,y1;
                    if(view.segment(maps::points[f.first+j-1],maps::points[f.first+j],cw,ch,x0,y0,x1,y1) && (x0!=x1 || y0!=y1))
                        mapCache.strokes.push_back({int16_t(x0-mapMargin),int16_t(y0-mapMargin),int16_t(x1-mapMargin),int16_t(y1-mapMargin),uint8_t(kind),uint8_t(width)});
                }
            }
        }
        ++mapStats.geometryBuilds;
        mapStats.lastBuildMs=lv_tick_get()-started;
        mapStats.candidates=uint32_t(mapCache.candidates.size());
        mapStats.strokes=uint32_t(mapCache.strokes.size());
    }
    if(!labelsWanted || (mapCache.labelWidth==w && mapCache.labelHeight==h && mapCache.labelScale==scale &&
        mapCache.labelX==view.center.x && mapCache.labelY==view.center.y)) return;
    mapCache.labelWidth=w; mapCache.labelHeight=h; mapCache.labelScale=scale;
    mapCache.labelX=view.center.x; mapCache.labelY=view.center.y; mapCache.labels.clear();
    const int labels[]={0,3,2,1};
    for(int kind:labels) {
        if((kind==1&&scale>4)||(kind==2&&scale>32)) continue;
        for(uint32_t i:mapCache.candidates) {
            if(mapCache.labels.size()>=9) break;
            const auto &f=maps::features[i];
            if(f.kind!=kind || !f.name || !view.visible(f,w,h)) continue;
            bool duplicate=false; for(const auto &label:mapCache.labels) if(label.name==f.name) duplicate=true;
            if(duplicate) continue;
            const auto p=maps::points[f.first+f.count/2]; int x,y;
            view.screen({p.x,p.y},w,h,x,y);
            const auto name=maps::featureName(f.name);
            lv_point_t size; lv_txt_get_size(&size,name.c_str(),&pda_font,0,0,220,LV_TEXT_FLAG_NONE);
            const int width=std::min(210,int(size.x));
            x-=width/2; y-=8;
            if(x<3||x+width>w-3||y<26||y>h-18||size.y>18) continue;
            bool overlap=false;
            for(const auto &label:mapCache.labels)
                if(x<label.x+label.width+5 && x+width+5>label.x && std::abs(y-label.y)<19) overlap=true;
            if(!overlap) mapCache.labels.push_back({x,y,width,f.name,uint8_t(kind)});
        }
    }
}
}
namespace maps { const RenderStats &renderStats() { return mapStats; } }
void Workspace::mapMotion() { mapLastMotion_=sys_.now(); mapMoving_=true; }
void Workspace::mapCheckpoint() {
    auto &s=sys_.state.session;
    s.view=9; s.form=0; s.editing=false;
    s.draft={std::to_string(mapView_.center.x),std::to_string(mapView_.center.y),std::to_string(mapView_.zoom)};
    sys_.changed();
}
void Workspace::mapRestore() {
    auto &s=sys_.state.session;
    int32_t x=0,y=0,z=0;
    if(s.view==9 && s.draft.size()==3 && parse(s.draft[0],x) && parse(s.draft[1],y) && parse(s.draft[2],z))
        mapView_.restore(x,y,z);
    else { mapView_.reset(); contentFocus_=true; mapCheckpoint(); }
    s.view=9;
}
void Workspace::mapScreen() {
    lv_obj_set_style_pad_all(body_,0,0);
    lv_obj_set_scroll_dir(body_,LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(body_,LV_SCROLLBAR_MODE_OFF);
    if(mapSearchOpen_) { mapSearchScreen(); return; }
    mapCanvas_=lv_obj_create(body_);
    lv_obj_set_pos(mapCanvas_,0,0); lv_obj_set_size(mapCanvas_,320,240);
    lv_obj_set_style_pad_all(mapCanvas_,0,0); lv_obj_set_style_radius(mapCanvas_,0,0);
    lv_obj_set_style_border_width(mapCanvas_,0,0); lv_obj_set_style_bg_color(mapCanvas_,lv_color_hex(0xF4F5EC),0);
    lv_obj_clear_flag(mapCanvas_,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(mapCanvas_,mapDraw,LV_EVENT_DRAW_MAIN,this);
    lv_obj_add_event_cb(mapCanvas_,mapTouch,LV_EVENT_PRESSED,this);
    lv_obj_add_event_cb(mapCanvas_,mapTouch,LV_EVENT_PRESSING,this);
    lv_obj_add_event_cb(mapCanvas_,mapTouch,LV_EVENT_RELEASED,this);
    mapPanel_=lv_obj_create(body_); lv_obj_set_pos(mapPanel_,0,135); lv_obj_set_size(mapPanel_,320,105);
    lv_obj_set_style_pad_all(mapPanel_,4,0); lv_obj_set_style_radius(mapPanel_,0,0);
    lv_obj_set_style_bg_color(mapPanel_,lv_color_hex(style::paper),0);
    lv_obj_set_style_bg_opa(mapPanel_,LV_OPA_COVER,0); lv_obj_set_style_border_width(mapPanel_,0,0);
    lv_obj_clear_flag(mapPanel_,LV_OBJ_FLAG_SCROLLABLE);
    button(mapPanel_,"+ Zoom",0,0,74,Command::MapZoomIn);
    button(mapPanel_,"- Zoom",79,0,74,Command::MapZoomOut);
    button(mapPanel_,"GPS",158,0,74,Command::MapGps);
    button(mapPanel_,"Centro",237,0,74,Command::MapReset);
    mapNamesButton_=button(mapPanel_,"Nomes",0,33,74,Command::MapLabels);
    lv_obj_set_style_bg_color(mapNamesButton_,lv_color_hex(style::selected),LV_STATE_CHECKED);
    lv_obj_set_style_text_color(mapNamesButton_,lv_color_hex(style::ink),LV_STATE_CHECKED);
    if(mapLabels_) lv_obj_add_state(mapNamesButton_,LV_STATE_CHECKED);
    button(mapPanel_,"Ajuda",79,33,74,Command::MapHelp);
    button(mapPanel_,"Inicio",158,33,74,Command::MapHome);
    button(mapPanel_,"Fechar",237,33,74,Command::MapMenu);
    button(mapPanel_,"Buscar",0,66,74,Command::MapSearch);
    label(mapPanel_,"B: rua ou rua, numero",85,70,220);
    mapMenuButton_=button(body_,"Menu",264,1,54,Command::MapMenu,0,23);
    mapControls(mapControlsVisible_);
    mapGpsRevision_=maps::gps().revision;
}
void Workspace::mapControls(bool visible) {
    mapControlsVisible_=visible; contentFocus_=!visible;
    if(!mapPanel_ || !mapMenuButton_) return;
    if(visible) {
        lv_obj_clear_flag(mapPanel_,LV_OBJ_FLAG_HIDDEN);
        if(!focusables_.empty()) lv_group_focus_obj(focusables_[0]);
    } else {
        lv_group_focus_obj(mapMenuButton_);
        lv_obj_add_flag(mapPanel_,LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text(lv_obj_get_child(mapMenuButton_,0),visible?"X":"Menu");
    if(mapCanvas_) lv_obj_invalidate(mapCanvas_);
}
void Workspace::mapCommand(Command command) {
    if(command==Command::MapSearch) {
        mapSearchOpen_=true; mapSearchEditing_=true; mapSearchDone_=false;
        mapQuery_.clear(); mapResults_.clear(); mapAddressResults_.clear(); mapNumberSearch_=false; render(); return;
    }
    if(command==Command::MapSearchBack) {
        mapSearchOpen_=false; mapControlsVisible_=false; contentFocus_=true; render(); return;
    }
    if(command==Command::MapFind) {
        mapResults_.clear();
        mapNumberSearch_=maps::searchApproximateAddresses(mapQuery_,mapView_.center,mapAddressResults_);
        if(!mapNumberSearch_) maps::searchStreets(mapQuery_,mapView_.center,mapResults_);
        mapSearchDone_=true; mapSearchEditing_=mapResults_.empty()&&mapAddressResults_.empty(); render(); return;
    }
    if(command==Command::MapMenu) { mapControls(!mapControlsVisible_); return; }
    if(command==Command::MapHome) { homeRequested_=true; return; }
    if(command==Command::MapHelp) {
        message("WASD: mover | I/O: zoom\nB: rua, numero (aproximado)\nF: GPS | C: centro | M: menu\nMapa: OpenStreetMap, ODbL 1.0\nopenstreetmap.org/copyright\nEnderecos: IBGE CNEFE 2022"); return;
    }
    if(command==Command::MapLabels) {
        mapLabels_=!mapLabels_;
        if(mapNamesButton_) {
            if(mapLabels_) lv_obj_add_state(mapNamesButton_,LV_STATE_CHECKED);
            else lv_obj_clear_state(mapNamesButton_,LV_STATE_CHECKED);
        }
        if(mapCanvas_) lv_obj_invalidate(mapCanvas_);
        return;
    }
    if(command==Command::MapZoomIn) mapView_.changeZoom(1);
    else if(command==Command::MapZoomOut) mapView_.changeZoom(-1);
    else if(command==Command::MapReset) mapView_.reset();
    else if(command==Command::MapGps && !mapView_.followGps()) {
        message(maps::gps().valid ? "GPS fora da area deste mapa.\nRecorte: Pinheiros / Zona Sul." : "GPS ainda sem posicao valida.\nVoce pode explorar o mapa por WASD ou pela bola.");
        return;
    }
    if(command==Command::MapReset || command==Command::MapGps) mapApproximate_=false;
    mapMotion(); mapCheckpoint(); if(mapCanvas_) lv_obj_invalidate(mapCanvas_);
}
void Workspace::mapHandle(InputEvent event) {
    if(mapSearchOpen_) { mapSearchHandle(event); return; }
    const bool keyboard=event.action==Action::Text;
    if(event.action==Action::Text) {
        const char c=char(event.character);
        if(c=='b'||c=='B') { mapCommand(Command::MapSearch); return; }
        if(c=='m'||c=='M') { mapCommand(Command::MapMenu); return; }
        if(c=='i'||c=='I'||c=='+'||c=='=') { mapCommand(Command::MapZoomIn); return; }
        if(c=='o'||c=='O'||c=='-') { mapCommand(Command::MapZoomOut); return; }
        if(c=='f'||c=='F') { mapCommand(Command::MapGps); return; }
        if(c=='c'||c=='C') { mapCommand(Command::MapReset); return; }
        if(c=='h'||c=='H'||c=='?') { mapCommand(Command::MapHelp); return; }
        if(c=='l'||c=='L') { mapCommand(Command::MapLabels); return; }
    }
    wasdMovement(event);
    if(event.action==Action::Move) {
        if(contentFocus_) { mapView_.pan(event.x,event.y,keyboard?16:4); mapMotion(); mapCheckpoint(); if(mapCanvas_) lv_obj_invalidate(mapCanvas_); }
        else moveFocus(event.y?event.y:event.x);
    } else if(event.action==Action::Next) { if(!mapControlsVisible_) mapControls(true); else moveFocus(1); }
    else if(event.action==Action::Backspace && mapControlsVisible_) mapControls(false);
    else if(event.action==Action::Activate) {
        if(contentFocus_) mapCommand(Command::MapZoomIn);
        else { auto *focused=lv_group_get_focused(focus_); if(focused) lv_event_send(focused,LV_EVENT_CLICKED,nullptr); }
    }
}
void Workspace::mapSearchScreen() {
    label(body_,"Buscar rua / numero",6,3,308);
    mapQueryLabel_=label(body_,mapQuery_.empty()?"> Digite parte do nome":"> "+mapQuery_,6,28,308);
    lv_label_set_long_mode(mapQueryLabel_,LV_LABEL_LONG_DOT);
    lv_obj_set_style_bg_color(mapQueryLabel_,lv_color_hex(0xFFFFFF),0);
    lv_obj_set_style_bg_opa(mapQueryLabel_,LV_OPA_COVER,0);
    label(body_,"Offline | numero = posicao aproximada",6,52,308);
    if(mapNumberSearch_ && !mapAddressResults_.empty()) {
        for(size_t i=0;i<mapAddressResults_.size();++i) {
            const auto &result=mapAddressResults_[i];
            char details[80]; snprintf(details,sizeof(details),"~ %u | CEP %05lu-%03lu",unsigned(result.number),
                (unsigned long)(result.postcode/1000),(unsigned long)(result.postcode%1000));
            auto *b=button(body_,maps::addressStreetName(result.street),6,77+int(i)*43,308,Command::MapAddressResult,int(i),40);
            auto *title=lv_obj_get_child(b,0); lv_obj_set_align(title,LV_ALIGN_TOP_LEFT); lv_obj_set_pos(title,2,0);
            auto *description=label(b,details,2,18,298);
            lv_obj_set_style_text_color(description,lv_color_hex(0x866020),0);
        }
    } else if(mapResults_.empty()) {
        const char *empty=mapNumberSearch_?"Nao foi possivel estimar esse numero.\nUse rua, numero (sem complemento).\nOu pesquise somente o nome da rua.":"Nenhuma rua encontrada no recorte.\nTente outro nome (min. 2 letras).";
        label(body_,mapSearchDone_?empty:"Ex.: nacoes unidas, 12501\nSem numero: digite apenas a rua.\nEnter: buscar | bola: escolher",6,83,308);
    } else {
        for(size_t i=0;i<mapResults_.size();++i) {
            const auto &f=maps::features[mapResults_[i].feature];
            button(body_,maps::featureName(f.name),6,77+int(i)*25,308,Command::MapResult,int(i),23);
        }
    }
    auto *find=button(body_,"Pesquisar",6,211,149,Command::MapFind);
    button(body_,"Voltar",165,211,149,Command::MapSearchBack);
    lv_group_focus_obj(mapResults_.empty()&&mapAddressResults_.empty()?find:focusables_[0]);
}
void Workspace::mapSearchHandle(InputEvent event) {
    if(event.action==Action::Text && event.character>=32 && event.character<127) {
        if(mapQuery_.size()>=64) return;
        mapQuery_+=char(event.character); mapSearchEditing_=true;
        if(mapSearchDone_) { mapResults_.clear(); mapAddressResults_.clear(); mapNumberSearch_=false; mapSearchDone_=false; render(); }
        else lv_label_set_text(mapQueryLabel_,("> "+mapQuery_).c_str());
    } else if(event.action==Action::Backspace) {
        if(mapQuery_.empty()) { mapCommand(Command::MapSearchBack); return; }
        mapQuery_.pop_back(); mapSearchEditing_=true; mapSearchDone_=false; mapResults_.clear(); mapAddressResults_.clear(); mapNumberSearch_=false; render();
    } else if(event.action==Action::Move || event.action==Action::Next) {
        mapSearchEditing_=false;
        moveFocus(event.action==Action::Next?1:event.y?event.y:event.x);
    } else if(event.action==Action::Activate) {
        if(mapSearchEditing_) mapCommand(Command::MapFind);
        else { auto *focused=lv_group_get_focused(focus_); if(focused) lv_event_send(focused,LV_EVENT_CLICKED,nullptr); }
    }
}
void Workspace::mapTouch(lv_event_t *event) {
    auto *self=static_cast<Workspace *>(lv_event_get_user_data(event));
    auto *device=lv_indev_get_act(); if(!device) return;
    lv_point_t p; lv_indev_get_point(device,&p);
    if(lv_event_get_code(event)==LV_EVENT_PRESSING) {
        const int dx=p.x-self->lastTouchX_,dy=p.y-self->lastTouchY_;
        if(dx||dy) {
            const auto center=self->mapView_.center; const int scale=self->mapView_.metersPerPixel();
            self->mapView_.restore(center.x-dx*scale,center.y-dy*scale,self->mapView_.zoom);
            self->mapMotion();
            lv_obj_invalidate(self->mapCanvas_);
        }
    } else if(lv_event_get_code(event)==LV_EVENT_RELEASED) self->mapCheckpoint();
    self->lastTouchX_=p.x; self->lastTouchY_=p.y;
}
void Workspace::mapDraw(lv_event_t *event) {
    auto *self=static_cast<Workspace *>(lv_event_get_user_data(event));
    auto *ctx=lv_event_get_draw_ctx(event); if(!ctx) return;
    lv_area_t area; lv_obj_get_coords(lv_event_get_target(event),&area);
    const int ox=area.x1,oy=area.y1,w=lv_area_get_width(&area),h=lv_area_get_height(&area);
    const lv_area_t *previous=ctx->clip_area;
    lv_area_t clip={std::max(area.x1,previous->x1),std::max(area.y1,previous->y1),std::min(area.x2,previous->x2),std::min(area.y2,previous->y2)};
    if(clip.x1>clip.x2||clip.y1>clip.y2) return;
    ctx->clip_area=&clip;
    const auto &view=self->mapView_; const int scale=view.metersPerPixel();
    const int scaleY=h-(self->mapControlsVisible_?123:18);
    // Cached vectors in RAM, not images: the map remains entirely image-free.
    const bool labelsWanted=self->mapLabels_ && !self->mapMoving_;
    prepareMap(view,w,h,labelsWanted);
    const int dx=(mapCache.x-view.center.x)/scale,dy=(mapCache.y-view.center.y)/scale;
    const uint32_t colors[]={0x70B8CB,0xB7BDC2,0xDBB96E,0xD99043,0x63AA84,0xA6A2AF};
    for(const auto &s:mapCache.strokes) {
        if(self->mapMoving_ && (s.kind==1 || s.kind==4 || s.kind==5)) continue;
        if(oy+dy+std::max(s.y0,s.y1)+s.width<clip.y1 || oy+dy+std::min(s.y0,s.y1)-s.width>clip.y2 ||
            ox+dx+std::max(s.x0,s.x1)+s.width<clip.x1 || ox+dx+std::min(s.x0,s.x1)-s.width>clip.x2) continue;
        line(ctx,ox+dx+s.x0,oy+dy+s.y0,ox+dx+s.x1,oy+dy+s.y1,colors[s.kind],s.width);
    }
    if(labelsWanted) for(const auto &label:mapCache.labels) {
        if(oy+label.y+17<clip.y1 || oy+label.y>clip.y2) continue;
        if(label.x+label.width>w-68 && label.y+17>scaleY && label.y<scaleY+18) continue;
        rectangle(ctx,ox+label.x-1,oy+label.y,label.width+2,17,0xF4F5EC);
        const auto name=maps::featureName(label.name);
        text(ctx,ox+label.x,oy+label.y,label.width+1,name.c_str(),label.kind==0?0x176B86:0x4A4B46);
    }
    // Crosshair denotes the viewed center, never an invented GPS position.
    line(ctx,ox+w/2-5,oy+h/2,ox+w/2+5,oy+h/2,0x63717C);
    line(ctx,ox+w/2,oy+h/2-5,ox+w/2,oy+h/2+5,0x63717C);
    if(self->mapApproximate_) {
        int x,y; view.screen(self->mapApproximatePoint_,w,h,x,y);
        if(x>=7 && x<w-7 && y>=31 && y<h-7) {
            rectangle(ctx,ox+x-6,oy+y-6,13,13,0xFFFFFF);
            line(ctx,ox+x-5,oy+y-5,ox+x+5,oy+y+5,0xB66C16,3);
            line(ctx,ox+x-5,oy+y+5,ox+x+5,oy+y-5,0xB66C16,3);
        }
    }
    const auto &gps=maps::gps();
    if(gps.valid && gps.inside) {
        int x,y; view.screen(gps.point,w,h,x,y);
        if(x>=5&&x<w-5&&y>=25&&y<h-6) {
            lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d); d.radius=LV_RADIUS_CIRCLE;
            d.bg_color=lv_color_hex(0x1568C4); d.bg_opa=LV_OPA_COVER; d.border_color=lv_color_white(); d.border_width=2;
            lv_area_t marker={lv_coord_t(ox+x-5),lv_coord_t(oy+y-5),lv_coord_t(ox+x+5),lv_coord_t(oy+y+5)};
            lv_draw_rect(ctx,&d,&marker);
        }
    }
    rectangle(ctx,ox,oy,w,24,0xE8EBDD);
    const char *status=self->mapApproximate_?"Endereco aproximado":!gps.valid?"Sem GPS | explorar":!gps.inside?"GPS fora do recorte":view.follow?"GPS | seguindo":"GPS valido | F: seguir";
    text(ctx,ox+4,oy+3,w-91,status);
    text(ctx,ox+w-77,oy+3,16,"N");
    char scaleText[32]; const int meters=scale*40;
    if(meters>=1000) snprintf(scaleText,sizeof(scaleText),"%.1f km",meters/1000.0);
    else snprintf(scaleText,sizeof(scaleText),"%d m",meters);
    rectangle(ctx,ox+w-65,oy+scaleY,65,17,0xF4F5EC);
    text(ctx,ox+w-62,oy+scaleY,62,scaleText);
    line(ctx,ox+w-52,oy+scaleY+16,ox+w-12,oy+scaleY+16,0x263442,2);
    ctx->clip_area=previous;
}
} // namespace pda
