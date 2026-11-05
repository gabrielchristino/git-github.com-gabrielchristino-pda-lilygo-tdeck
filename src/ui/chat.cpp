#ifdef PDA_CHAT
#include "pda/shell.h"
#include "pda/ui_style.h"
#include "pda/datetime.h"
#include <cstring>
#include <cstdio>

namespace pda {
namespace {
const char *welcome = "Digite ajuda ou o que voce faz? para ver os comandos. Consulte, crie, edite ou exclua sem abrir apps. Para mudanças: sim confirma, cancelar descarta.";
}
void Shell::leaveChat() {
    if (chatInput_) chatDraft_ = lv_textarea_get_text(chatInput_);
    chatLog_ = chatInput_ = chatHint_ = nullptr;
    chatSendPending_ = chatClearPending_ = false;
}
void Shell::buildChat() {
    lv_obj_scroll_to_y(body_,0,LV_ANIM_OFF);
    lv_obj_clear_flag(body_,LV_OBJ_FLAG_SCROLLABLE);
    chatHint_ = lv_label_create(body_); lv_obj_set_pos(chatHint_,0,0); lv_obj_set_width(chatHint_,230);
    lv_obj_set_style_text_font(chatHint_,&lv_font_montserrat_12,0);
    lv_label_set_text(chatHint_,"Local / experimental");
    auto button = [&](const char *text,int x,int y,int width,bool clear) {
        auto *b=lv_btn_create(body_); style::button(b);
        lv_obj_set_pos(b,x,y); lv_obj_set_size(b,width,28);
        lv_obj_set_style_text_font(b,&lv_font_montserrat_12,0);
        lv_obj_set_user_data(b,reinterpret_cast<void *>(clear?1:0));
        lv_obj_add_event_cb(b,chatClicked,LV_EVENT_CLICKED,this);
        auto *label=lv_label_create(b); lv_label_set_text(label,text); lv_obj_center(label);
    };
    button("Limpar",240,0,66,true);
    chatLog_=lv_obj_create(body_); lv_obj_remove_style_all(chatLog_);
    lv_obj_set_pos(chatLog_,0,30); lv_obj_set_size(chatLog_,306,104);
    lv_obj_set_style_pad_all(chatLog_,3,0); lv_obj_set_style_pad_row(chatLog_,6,0);
    lv_obj_set_scroll_dir(chatLog_,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(chatLog_,LV_SCROLLBAR_MODE_AUTO);
    chatInput_=lv_textarea_create(body_); lv_obj_set_pos(chatInput_,0,140); lv_obj_set_size(chatInput_,234,30);
    lv_textarea_set_one_line(chatInput_,true); lv_textarea_set_max_length(chatInput_,80);
    lv_textarea_set_placeholder_text(chatInput_,"Digite seu pedido...");
    lv_obj_set_style_pad_all(chatInput_,3,0); lv_obj_set_style_text_font(chatInput_,&pda_font,0);
    for (lv_style_selector_t state : {lv_style_selector_t(0),lv_style_selector_t(LV_STATE_FOCUSED),lv_style_selector_t(LV_STATE_EDITED)}) {
        lv_obj_set_style_bg_color(chatInput_,lv_color_hex(style::paper),state);
        lv_obj_set_style_text_color(chatInput_,lv_color_hex(style::ink),state);
        lv_obj_set_style_border_color(chatInput_,lv_color_hex(style::selected),state);
        lv_obj_set_style_outline_width(chatInput_,0,state);
    }
    lv_obj_set_style_bg_color(chatInput_,lv_color_hex(style::selected),LV_PART_CURSOR);
    lv_textarea_set_text(chatInput_,chatDraft_.c_str());
    lv_group_add_obj(focus_,chatInput_); lv_group_focus_obj(chatInput_);
    lv_obj_add_state(chatInput_,LV_STATE_EDITED);
    button("Enviar",240,140,66,false);
    if (chatMessages_.empty()) chatMessages_.push_back({false,welcome});
    renderChatMessages();
    lv_label_set_text(footer_,"Enter: enviar | Shift+$: limpar");
}
void Shell::renderChatMessages() {
    lv_obj_clean(chatLog_);
    int y=0;
    for (const auto &message:chatMessages_) {
        auto *bubble=lv_obj_create(chatLog_); lv_obj_remove_style_all(bubble);
        lv_obj_set_pos(bubble,message.user?30:0,y);
        lv_obj_set_width(bubble,270); lv_obj_set_height(bubble,LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(bubble,6,0); lv_obj_set_style_radius(bubble,6,0);
        lv_obj_set_style_bg_opa(bubble,LV_OPA_COVER,0);
        lv_obj_set_style_bg_color(bubble,lv_color_hex(message.user?style::selected:style::card),0);
        lv_obj_clear_flag(bubble,LV_OBJ_FLAG_SCROLLABLE);
        auto *label=lv_label_create(bubble); lv_obj_set_width(label,258);
        lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(label,&pda_font,0);
        lv_obj_set_style_text_color(label,lv_color_hex(message.user?style::onSelected:style::ink),0);
        std::string text=message.user?"Você: ":"Assistente: "; text+=message.text;
        lv_label_set_text(label,text.c_str());
        lv_obj_update_layout(bubble);y+=lv_obj_get_height(bubble)+6;
    }
    lv_obj_update_layout(chatLog_);
    if (lv_obj_get_child_cnt(chatLog_)) lv_obj_scroll_to_view(lv_obj_get_child(chatLog_,-1),LV_ANIM_OFF);
}
std::string Shell::chatResponse(const char *text) {
    int label=0;
    chatLastLabel_=-1;
    if (chatClassifier_ && chatClassifier_(text,label)) chatLastLabel_=label;
    const auto response=assistant_.respond(*system_,text);
    if (!response.empty()) return response;
    const int telemetry=assistantTelemetryIntent(text);
    if(telemetry) label=telemetry;
    char reply[128];
    switch (label) {
    case 1:
        if (!batteryStats_.valid()) return "A leitura da bateria está indisponível agora.";
        snprintf(reply,sizeof(reply),"Bateria estimada: %d%%. Estimativa pela tensão do aparelho.",batteryStats_.percent);
        return reply;
    case 2:
        if (!system_->clockValid()) return "O relógio ainda não tem um horário válido. Ajuste a hora em Relógio.";
        { const auto local=fromUtc(system_->utc(),system_->state.preferences.timezoneMinutes);
          snprintf(reply,sizeof(reply),"Agora sao %02d:%02d.",local.hour,local.minute); return reply; }
    case 3: return "Use meus eventos ou agenda hoje. Para ler: le evento 1. Para alterar: remarca evento 1 para DD/MM/AAAA HH:MM.";
    case 4: return "Informe o nome: qual o telefone de Tamires? Para cadastrar: contato Nome: telefone.";
    case 5: return "Use uma conta como quanto é 288 dividido por 32,66.";
    case 6: return "Use timer 45s, cancela o timer ou altera o timer para 5 minutos. Para consultar: quanto tempo falta no timer?";
    case 7: return "Use minhas notas, le nota 1, edita nota 1: texto, apaga nota 1 ou nota: texto para criar.";
    case 8: return "Entendi um pedido para abrir um app. Nenhum app foi aberto.";
    default: return "Não reconheci um pedido disponível com segurança. Tente outra frase.";
    }
}
void Shell::chatClicked(lv_event_t *event) {
    auto *self=static_cast<Shell *>(lv_event_get_user_data(event));
    if (lv_obj_get_user_data(lv_event_get_target(event))) self->chatClearPending_=true;
    else self->chatSendPending_=true;
}
void Shell::handleChat(const InputEvent &event) {
    if (!chatInput_) return;
    if (event.action==Action::Activate) chatSendPending_=true;
    else if (event.action==Action::ToggleNavigation) chatClearPending_=true;
    else if (event.action==Action::Backspace) lv_textarea_del_char(chatInput_);
    else if (event.action==Action::Text && event.character>=32 && event.character<127) {
        if (strlen(lv_textarea_get_text(chatInput_))<80) {
            const char text[]={char(event.character),0}; lv_textarea_add_text(chatInput_,text);
            lv_label_set_text(chatHint_,"Local / experimental");
        } else lv_label_set_text(chatHint_,"Texto cheio: encurte o pedido");
    } else if (event.action==Action::Move) {
        if (event.x<0) lv_textarea_cursor_left(chatInput_);
        if (event.x>0) lv_textarea_cursor_right(chatInput_);
        if (event.y) lv_obj_scroll_by(chatLog_,0,event.y>0?-24:24,LV_ANIM_OFF);
    }
}
void Shell::tickChat() {
    if (chatClearPending_) {
        chatClearPending_=chatSendPending_=false; chatMessages_.clear();
        assistant_.clear();
        chatMessages_.push_back({false,welcome});chatDraft_.clear();lv_textarea_set_text(chatInput_,"");
        chatLastLabel_=-1;
        lv_label_set_text(chatHint_,"Local / experimental");renderChatMessages();return;
    }
    if (!chatSendPending_) return;
    chatSendPending_=false;
    std::string text=lv_textarea_get_text(chatInput_);
    const auto start=text.find_first_not_of(" \t\r\n");
    if (start==std::string::npos) {lv_label_set_text(chatHint_,"Digite um pedido para enviar");return;}
    if (text.size()>80) {lv_label_set_text(chatHint_,"Texto longo: encurte o pedido");return;}
    text=text.substr(start,text.find_last_not_of(" \t\r\n")-start+1);
    const auto reply=chatResponse(text.c_str());
    chatMessages_.push_back({true,text});chatMessages_.push_back({false,reply});
    while (chatMessages_.size()>12) chatMessages_.erase(chatMessages_.begin());
    chatDraft_.clear();lv_textarea_set_text(chatInput_,"");
    lv_label_set_text(chatHint_,"Local / experimental");renderChatMessages();
}
}
#endif
