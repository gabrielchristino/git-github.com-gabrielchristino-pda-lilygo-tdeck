#pragma once
#include "pda/ui_style.h"
#include "game_text_corpus.h"
#include <map>
#include <set>

static FILE *glyphReport=nullptr;
static unsigned glyphChecks=0,labelChecks=0;
static std::map<const lv_font_t *,std::set<uint32_t>> checkedGlyphs;
static const char *fontName(const lv_font_t *font) {
    if(font==&pda_font) return "pda_font";
    if(font==&lv_font_montserrat_12) return "Montserrat12";
    if(font==&lv_font_montserrat_14) return "Montserrat14";
    if(font==&lv_font_montserrat_16) return "Montserrat16";
    if(font==&lv_font_montserrat_28) return "Montserrat28";
    return "other";
}
static void checkGlyphText(const lv_font_t *font,const char *text,const char *context) {
    for(uint32_t i=0;text[i];) {
        const uint32_t cp=_lv_txt_encoded_next(text,&i);
        if(cp=='\n'||cp=='\r'||cp=='\t') continue;
        ++glyphChecks;
        lv_font_glyph_dsc_t glyph={};
        const bool found=lv_font_get_glyph_dsc(font,&glyph,cp,0);
        const bool blank=cp==32||cp==160;
        const bool pixels=blank||(glyph.box_w&&glyph.box_h&&glyph.resolved_font&&lv_font_get_glyph_bitmap(glyph.resolved_font,cp));
        const bool real=found&&!glyph.is_placeholder&&glyph.resolved_font&&pixels;
        if(checkedGlyphs[font].insert(cp).second&&glyphReport)
            fprintf(glyphReport,"%s\tU+%04X\t%s\t%u\t%u\t%s\t%s\n",fontName(font),unsigned(cp),real?"OK":"MISSING",glyph.box_w,glyph.box_h,fontName(glyph.resolved_font),context);
        if(!real) {
            fprintf(stderr,"Missing glyph U+%04X in %s (%s): %s\n",unsigned(cp),fontName(font),context,text);
            exit(1);
        }
    }
}
static void auditGameLabels(lv_obj_t *root,lv_obj_t *body) {
    if(lv_obj_has_flag(root,LV_OBJ_FLAG_HIDDEN)) return;
    if(lv_obj_check_type(root,&lv_label_class)) {
        ++labelChecks;
        const char *text=lv_label_get_text(root);
        const lv_font_t *font=lv_obj_get_style_text_font(root,LV_PART_MAIN);
        checkGlyphText(font,text,"visible game label");
        auto *parent=lv_obj_get_parent(root);
        // A caption must fit its tile, including glyph bearings and descenders.
        if(parent!=body&&lv_obj_get_parent(parent)==body&&lv_obj_check_type(parent,&lv_obj_class)&&!strchr(text,'\n')) {
            lv_area_t label,tile; lv_obj_get_coords(root,&label); lv_obj_get_content_coords(parent,&tile);
            int advance=0;
            for(uint32_t i=0;text[i];) {
                const uint32_t cp=_lv_txt_encoded_next(text,&i); uint32_t next=i;
                const uint32_t following=text[i]?_lv_txt_encoded_next(text,&next):0;
                lv_font_glyph_dsc_t glyph={}; lv_font_get_glyph_dsc(font,&glyph,cp,following);
                if(glyph.box_w&&glyph.box_h) {
                    const int left=label.x1+advance+glyph.ofs_x;
                    const int top=label.y1+font->line_height-font->base_line-glyph.box_h-glyph.ofs_y;
                    if(left<tile.x1||left+glyph.box_w>tile.x2+1||top<tile.y1||top+glyph.box_h>tile.y2+1) {
                        fprintf(stderr,"Clipped game glyph U+%04X (%s): %s; glyph %d,%d %ux%u; tile %d,%d..%d,%d\n",unsigned(cp),fontName(font),text,left,top,glyph.box_w,glyph.box_h,tile.x1,tile.y1,tile.x2,tile.y2); exit(1);
                    }
                }
                advance+=glyph.adv_w;
            }
        }
    }
    for(uint32_t i=0;i<lv_obj_get_child_cnt(root);++i) auditGameLabels(lv_obj_get_child(root,i),body);
}
static void auditGameCorpus() {
    glyphReport=fopen("game-glyph-audit.tsv","wb"); require(glyphReport!=nullptr,"glyph audit report");
    fprintf(glyphReport,"font\tcodepoint\tresult\twidth\theight\tresolved_font\tcontext\n");
    for(unsigned cp=32;cp<127;++cp) {
        const char text[]={char(cp),0};
        checkGlyphText(&pda_font,text,"ASCII and UI text");
    }
    for(const char *text:game_text_corpus) checkGlyphText(&pda_font,text,"game source literal");
    for(unsigned i=0;i<pda::games::levelCount(pda::games::Kind::Hangman);++i) {
        checkGlyphText(&pda_font,pda::games::wordAt(i),"word bank");
        checkGlyphText(&pda_font,pda::games::wordCategory(i),"word category");
    }
    checkGlyphText(&pda_font,"ÁÀÂÃÉÊÍÓÔÕÚÜÇ áàâãéêíóôõúüç … · ×", "Portuguese reference");
    checkGlyphText(&lv_font_montserrat_12,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789@+>*F/.:- $HQXSgmopebt", "small board symbols and HUD");
    checkGlyphText(&lv_font_montserrat_16,"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", "legacy word grid and 2048");
    checkGlyphText(&lv_font_montserrat_28,"0123456789", "2048 and code digits");
    // Prove that the checker rejects the old missing middle dot in the small font.
    lv_font_glyph_dsc_t missing={};
    require(!lv_font_get_glyph_dsc(&lv_font_montserrat_12,&missing,0xB7,0)||missing.is_placeholder,"audit catches old missing middle-dot glyph");
}
static std::string codepointText(uint32_t cp) {
    if(cp<128) return std::string(1,char(cp));
    if(cp<2048) return std::string(1,char(0xC0|(cp>>6)))+char(0x80|(cp&63));
    return std::string(1,char(0xE0|(cp>>12)))+char(0x80|((cp>>6)&63))+char(0x80|(cp&63));
}
