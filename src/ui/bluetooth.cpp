#include "pda/shell.h"
#include "pda/ui_style.h"
#include <stdio.h>
#include <string.h>
#include <algorithm>

namespace pda {
namespace {
constexpr uintptr_t mouseAction = 0x10000, tabAction = 0x20000, modifierAction = 0x30000;
constexpr uintptr_t characterAction = 0x40000, keyAction = 0x50000, symbolsAction = 0x60000;
constexpr uintptr_t peersAction=0x70000, usePeerAction=0x80000, pairAction=0x90000;
constexpr uintptr_t editPeerAction=0xA0000, namePeerAction=0xB0000, forgetPeerAction=0xC0000;
constexpr uintptr_t peersBackAction=0xD0000, confirmPeerAction=0xE0000;
const uint8_t modifierBits[] = {1,4,2,8}; // Ctrl, Alt, Shift, GUI
const char *modifierNames[] = {"Ctrl", "Alt", "Shift", "Win"};
// Every ASCII punctuation mark, with the hard-to-reach ones first.
const char symbols[] = "<>~=[]{}`\\|^%&@#\"':;_+-*/!?$(),.";
std::string peerSignature(const std::vector<BluetoothPeer> &peers) {
    std::string result;
    for(const auto &peer:peers) result+=peer.id+'\n'+peer.name+'\n'+char('0'+peer.selected)+char('0'+peer.connected);
    return result;
}
}
void Shell::sendRemoteKey(uint8_t usage, uint8_t modifiers) {
    if (remote_.key) remote_.key(usage, modifiers | remoteMask_);
    remoteMask_ = 0;
    refreshRemote();
}
void Shell::sendRemoteCharacter(uint8_t character) {
    uint8_t modifier, usage;
    if (hidKey(character, modifier, usage)) sendRemoteKey(usage, modifier);
}
void Shell::buildRemotePanel() {
    lv_group_remove_all_objs(focus_);
    lv_obj_clean(body_); lv_obj_scroll_to_y(body_, 0, LV_ANIM_OFF);
    for (auto &button : remoteModifiers_) button = nullptr;
    remoteStatus_ = lv_label_create(body_); lv_obj_set_width(remoteStatus_, 216);
    lv_label_set_long_mode(remoteStatus_,LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(remoteStatus_, 0, 0);
    lv_obj_set_style_text_font(remoteStatus_, &lv_font_montserrat_12, 0);
    const char *tabs[] = {"Mouse", "Teclas", "F1-F12", "Símbolos"};
    for (unsigned i = 0; i < 4; ++i) {
        auto *btn = remoteTabs_[i] = lv_btn_create(body_); style::button(btn);
        lv_obj_set_pos(btn, i * 76, 20); lv_obj_set_size(btn, 72, 27);
        lv_obj_set_style_text_font(btn, &lv_font_montserrat_12, 0);
        // The Portuguese tab name uses the bundled full UI font.
        if (i == 3) lv_obj_set_style_text_font(btn, &pda_font, 0);
        lv_obj_set_user_data(btn, reinterpret_cast<void *>(tabAction | i));
        lv_obj_add_event_cb(btn, remoteClicked, LV_EVENT_CLICKED, this);
        auto *label = lv_label_create(btn); lv_label_set_text(label, tabs[i]); lv_obj_center(label);
        lv_obj_set_style_bg_color(btn, lv_color_hex(i == remoteTab_ ? style::selected : style::paper), 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(i == remoteTab_ ? style::onSelected : style::ink), 0);
    }
    remotePanel_ = lv_obj_create(body_); lv_obj_remove_style_all(remotePanel_);
    lv_obj_set_pos(remotePanel_, 0, 51); lv_obj_set_size(remotePanel_, 300, 120);
    lv_obj_clear_flag(remotePanel_, LV_OBJ_FLAG_SCROLLABLE);
    auto button = [&](const char *text, unsigned position, uintptr_t action, unsigned columns = 4) {
        auto *btn = lv_btn_create(remotePanel_); style::button(btn);
        const int spacing = columns == 6 ? 50 : 76;
        lv_obj_set_pos(btn, (position % columns) * spacing, (position / columns) * 30);
        lv_obj_set_size(btn, columns == 6 ? 47 : 72, 27);
        lv_obj_set_user_data(btn, reinterpret_cast<void *>(action));
        lv_obj_add_event_cb(btn, remoteClicked, LV_EVENT_CLICKED, this);
        auto *label = lv_label_create(btn); lv_label_set_text(label, text); lv_obj_center(label);
        return btn;
    };
    if (remoteTab_ == 0) {
        const char *names[] = {"Direito", "Arrastar", "Rolar +", "Rolar -"};
        for (unsigned i = 0; i < 4; ++i) button(names[i], i, mouseAction | i);
        const char *shortcuts[] = {"Ctrl+C", "Ctrl+V", "Ctrl+X", "Ctrl+Z", "Ctrl+A", "Ctrl+S", "Alt+Tab", "Win"};
        const uint8_t usages[] = {6,25,27,29,4,22,43,0};
        for (unsigned i = 0; i < 8; ++i)
            button(shortcuts[i], i + 4, keyAction | usages[i] | ((i < 6 ? 1 : i == 6 ? 4 : 8) << 8));
        auto *help = lv_label_create(remotePanel_); lv_obj_set_pos(help, 0, 90); lv_obj_set_width(help, 300);
        lv_obj_set_style_text_font(help, &lv_font_montserrat_12, 0);
        lv_label_set_text(help, "Bola: mouse | clique: esquerdo\nSegure a bola para sair.");
    } else if (remoteTab_ == 1) {
        for (unsigned i = 0; i < 4; ++i)
            remoteModifiers_[i] = button(modifierNames[i], i, modifierAction | modifierBits[i]);
        const char *names[] = {"Esc", "Tab", "Bksp", "Del", "Home", "Cima", "End", "PgUp", "Esq", "Baixo", "Dir", "PgDn"};
        const uint8_t usages[] = {41,43,42,76,74,82,77,75,80,81,79,78};
        for (unsigned i = 0; i < 12; ++i) button(names[i], i + 4, keyAction | usages[i]);
    } else if (remoteTab_ == 2) {
        for (unsigned i = 0; i < 12; ++i) {
            char name[5]; snprintf(name, sizeof(name), "F%u", i + 1);
            button(name, i, keyAction | (58 + i));
        }
        button("Win",12,keyAction | (8 << 8)); button("PrtSc",13,keyAction | 70);
        button("Ins",14,keyAction | 73); button("Pause",15,keyAction | 72);
    } else {
        const unsigned start = remoteSymbols_ * 23;
        for (unsigned i = start; i < sizeof(symbols) - 1 && i < start + 23; ++i) {
            const char name[] = {symbols[i],0};
            button(name, i - start, characterAction | uint8_t(symbols[i]), 6);
        }
        button(remoteSymbols_ ? "Voltar" : "Mais",23,symbolsAction,6);
    }
    auto *peers=lv_btn_create(body_); style::button(peers);
    lv_obj_set_pos(peers,224,0); lv_obj_set_size(peers,76,18);
    lv_obj_set_style_text_font(peers,&lv_font_montserrat_12,0);
    lv_obj_set_user_data(peers,reinterpret_cast<void *>(peersAction));
    lv_obj_add_event_cb(peers,remoteClicked,LV_EVENT_CLICKED,this);
    auto *name=lv_label_create(peers); lv_label_set_text(name,"Aparelhos"); lv_obj_center(name);
    remoteRebuild_ = false;
    refreshRemote();
}
void Shell::remoteClicked(lv_event_t *event) {
    auto *self = static_cast<Shell *>(lv_event_get_user_data(event));
    const uintptr_t action = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_event_get_target(event)));
    const auto value = uint8_t(action);
    switch (action & 0xF0000) {
    case peersAction:
        self->openBluetoothPeers(); break;
    case usePeerAction: case editPeerAction:
        if(value<self->remotePeers_.size()) {
            const auto id=self->remotePeers_[value].id;
            if((action&0xF0000)==usePeerAction) { self->remotePeerId_=id; self->remotePeerAction_=PeerAction::Select; }
            else { self->remoteEditId_=id; self->remoteForgetConfirm_=false; self->remoteRebuild_=true; }
        }
        break;
    case pairAction: self->remotePeerAction_=PeerAction::Pair; break;
    case namePeerAction:
        self->remotePeerId_=self->remoteEditId_;
        self->remotePeerName_=value==0?"Celular":value==1?"Computador":"Aparelho";
        self->remotePeerAction_=PeerAction::Rename; break;
    case forgetPeerAction: self->remoteForgetConfirm_=true; self->remoteRebuild_=true; break;
    case confirmPeerAction:
        self->remotePeerId_=self->remoteEditId_; self->remotePeerAction_=PeerAction::Forget; break;
    case peersBackAction:
        if(self->remoteForgetConfirm_) self->remoteForgetConfirm_=false;
        else if(!self->remoteEditId_.empty()) self->remoteEditId_.clear();
        else self->remoteManaging_=false;
        self->remoteRebuild_=true; break;
    case tabAction: self->remoteTab_ = value; self->remoteRebuild_ = true; break;
    case modifierAction: self->remoteMask_ ^= value; break;
    case symbolsAction: self->remoteSymbols_ ^= 1; self->remoteRebuild_ = true; break;
    case mouseAction: if (self->remote_.command) self->remote_.command(static_cast<RemoteCommand>(value)); break;
    case characterAction: self->sendRemoteCharacter(value); break;
    case keyAction: self->sendRemoteKey(value, uint8_t(action >> 8)); break;
    }
    // Rebuild only on the next tick, after LVGL finishes dispatching the tap.
    self->refreshRemote();
}
void Shell::refreshRemote() {
    if(remoteManaging_) return;
    if (!remoteStatus_) return;
    if (remote_.connected && !remote_.connected()) remoteMask_ = 0;
    const char *text = remote_.status ? remote_.status() : "Prévia: Bluetooth indisponível";
    if (strcmp(lv_label_get_text(remoteStatus_), text)) lv_label_set_text(remoteStatus_, text);
    for (unsigned i = 0; i < 4; ++i) if (remoteModifiers_[i]) {
        const bool selected = remoteMask_ & modifierBits[i];
        lv_obj_set_style_bg_color(remoteModifiers_[i], lv_color_hex(selected ? style::selected : style::paper), 0);
        lv_obj_set_style_text_color(remoteModifiers_[i], lv_color_hex(selected ? style::onSelected : style::ink), 0);
    }
    char footer[100];
    if (remoteMask_) snprintf(footer,sizeof(footer),"%s%s%s%s: próxima tecla",
        remoteMask_ & 1 ? "Ctrl " : "", remoteMask_ & 4 ? "Alt " : "",
        remoteMask_ & 2 ? "Shift " : "", remoteMask_ & 8 ? "Win " : "");
    else snprintf(footer,sizeof(footer),"Shift+$: = | layout Inglês (US)");
    if (strcmp(lv_label_get_text(footer_),footer)) lv_label_set_text(footer_,footer);
}

void Shell::buildRemotePeers() {
    lv_group_remove_all_objs(focus_); lv_obj_clean(body_); lv_obj_scroll_to_y(body_,0,LV_ANIM_OFF);
    remoteStatus_=remotePanel_=nullptr;
    for(auto &modifier:remoteModifiers_) modifier=nullptr;
    remotePeers_=remote_.peers?remote_.peers():std::vector<BluetoothPeer>{};
    remotePeerSignature_=peerSignature(remotePeers_); remotePeerPoll_=system_->now();
    auto label=[&](const std::string &text,int y) {
        auto *obj=lv_label_create(body_); lv_obj_set_pos(obj,0,y); lv_obj_set_width(obj,300);
        lv_obj_set_style_text_font(obj,&pda_font,0); lv_label_set_text(obj,text.c_str()); return obj;
    };
    auto button=[&](const std::string &text,int x,int y,int width,uintptr_t action) {
        auto *obj=lv_btn_create(body_); style::button(obj);
        lv_obj_set_pos(obj,x,y); lv_obj_set_size(obj,width,27);
        lv_obj_set_user_data(obj,reinterpret_cast<void *>(action));
        lv_obj_add_event_cb(obj,remoteClicked,LV_EVENT_CLICKED,this);
        auto *name=lv_label_create(obj); lv_obj_set_width(name,width-8);
        lv_label_set_long_mode(name,LV_LABEL_LONG_DOT); lv_label_set_text(name,text.c_str()); lv_obj_center(name);
        lv_group_add_obj(focus_,obj); return obj;
    };
    auto edit=std::find_if(remotePeers_.begin(),remotePeers_.end(),[&](const BluetoothPeer &p){return p.id==remoteEditId_;});
    if(!remoteEditId_.empty() && edit==remotePeers_.end()) { remoteEditId_.clear(); remoteForgetConfirm_=false; }
    if(remoteEditId_.empty()) {
        label("Aparelhos salvos (* destino)",0);
        if(remotePeers_.empty()) label("Selecione Parear novo.\nNo celular/PC, procure\nT-Deck PDA no Bluetooth.",28);
        for(size_t i=0;i<remotePeers_.size() && i<3;++i) {
            const auto &peer=remotePeers_[i];
            auto *use=button(peer.name+(peer.connected?" [on]":peer.selected?" *":""),0,24+int(i)*31,198,usePeerAction|i);
            if(peer.selected) lv_obj_add_state(use,LV_STATE_CHECKED);
            button("Nome",204,24+int(i)*31,96,editPeerAction|i);
        }
        button("Parear novo",0,124,145,pairAction);
        button("Teclado",151,124,149,peersBackAction);
    } else {
        label(edit->name,0); label(edit->id,22);
        if(remoteForgetConfirm_) {
            label("Esquecer este pareamento?",55);
            auto *cancel=button("Cancelar",0,96,145,peersBackAction);
            button("Confirmar",151,96,149,confirmPeerAction);
            lv_group_focus_obj(cancel);
        } else {
            button("Celular",0,49,94,namePeerAction);
            button("Computador",100,49,100,namePeerAction|1);
            button("Aparelho",206,49,94,namePeerAction|2);
            const auto index=size_t(edit-remotePeers_.begin());
            button("Usar",0,85,145,usePeerAction|index);
            button("Esquecer",151,85,149,forgetPeerAction);
            button("Voltar",0,124,300,peersBackAction);
        }
    }
    lv_label_set_text(footer_,remotePeerMessage_.empty()?"WASD/Enter: escolher | Bksp: voltar":remotePeerMessage_.c_str());
    lv_obj_update_layout(body_);
    remoteRebuild_=false;
}

bool Shell::openBluetoothPeers() {
    if(!current_ || current_->id!=AppId::Bluetooth || alert_) return false;
    remoteManaging_=true; remoteMask_=0; remoteEditId_.clear(); remotePeerMessage_.clear();
    remoteForgetConfirm_=false; remotePeerAction_=PeerAction::None; remoteRebuild_=true;
    return true;
}

void Shell::handleRemotePeers(const InputEvent &input) {
    InputEvent event=input;
    wasdMovement(event);
    if(event.action==Action::Move) {
        if((event.y?event.y:event.x)>0) lv_group_focus_next(focus_); else lv_group_focus_prev(focus_);
    } else if(event.action==Action::Next) lv_group_focus_next(focus_);
    else if(event.action==Action::Activate) {
        auto *focused=lv_group_get_focused(focus_); if(focused) lv_event_send(focused,LV_EVENT_CLICKED,nullptr);
    } else if(event.action==Action::Backspace || event.action==Action::Home) {
        if(remoteForgetConfirm_) remoteForgetConfirm_=false;
        else if(!remoteEditId_.empty()) remoteEditId_.clear();
        else remoteManaging_=false;
        remoteRebuild_=true;
    }
}

void Shell::tickRemotePeers() {
    if(remotePeerAction_!=PeerAction::None) {
        const auto action=remotePeerAction_; remotePeerAction_=PeerAction::None;
        bool ok=false; remoteMask_=0;
        if(action==PeerAction::Select && remote_.selectPeer) ok=remote_.selectPeer(remotePeerId_);
        else if(action==PeerAction::Pair && remote_.pairNew) ok=remote_.pairNew();
        else if(action==PeerAction::Rename && remote_.renamePeer) ok=remote_.renamePeer(remotePeerId_,remotePeerName_);
        else if(action==PeerAction::Forget && remote_.forgetPeer) ok=remote_.forgetPeer(remotePeerId_);
        remotePeerMessage_=ok?(action==PeerAction::Pair?"Pareie no celular/PC: T-Deck PDA":"Pronto. Use Teclado para digitar."):
            action==PeerAction::Pair && remotePeers_.size()>=3?"Limite: 3 aparelhos. Esqueça um.":"Falha na ação. Tente novamente.";
        if(action==PeerAction::Forget && ok) { remoteEditId_.clear(); remoteForgetConfirm_=false; }
        remoteRebuild_=true;
    }
    if(remoteManaging_ && remoteEditId_.empty() && remote_.peers && uint32_t(system_->now()-remotePeerPoll_)>=500) {
        remotePeerPoll_=system_->now();
        if(peerSignature(remote_.peers())!=remotePeerSignature_) remoteRebuild_=true;
    }
}
}
