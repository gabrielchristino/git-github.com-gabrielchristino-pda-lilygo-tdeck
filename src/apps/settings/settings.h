#pragma once
#include <lvgl.h>
#include "input/input.h"
#include <WiFi.h>
#include <Preferences.h>
#include "driver/ledc.h"
#include <ctype.h>
#include <ArduinoJson.h>
#include "input/trackball/trackball.h"

// Este arquivo é incluído a partir de `apps.h`, que já define o AppManager.

namespace Settings {

// --- UI Variables ---
const char KEY_BACKSPACE = 8;
const char KEY_CLEAR_ENTRY = 2; // Corresponds to a special key from the firmware (e.g., 'C')

// --- Constants for Backlight ---
#define BACKLIGHT_PIN 10
#define LEDC_CHANNEL_BRIGHTNESS 0

// --- Constants for Wi-Fi History ---
#define MAX_SAVED_NETWORKS 5
#define SAVED_NETWORKS_KEY "saved_wifis"

static lv_obj_t* settings_screen = nullptr; // A tela principal
static lv_obj_t* main_container = nullptr;  // O contêiner principal para os itens do menu
static lv_obj_t* wifi_list = nullptr;       // A lista de redes Wi-Fi
static lv_obj_t* wifi_content_panel = nullptr; // Ponteiro para o painel de conteúdo do Wi-Fi

// --- UI Variables for Password Modal ---
static lv_obj_t* password_modal_cont = nullptr;
static lv_obj_t* password_textarea = nullptr;

// --- UI Variables for Display Settings ---
static lv_obj_t* brightness_slider = nullptr;
static lv_obj_t* brightness_label = nullptr;

// --- Preferences ---
static Preferences preferences;

// --- State Variables for Navigation ---
static int selected_setting_index = 0;
static const int SETTING_ITEM_COUNT = 3; // Total de itens no menu (Wi-Fi, Display, About)

// --- State for Wi-Fi Connection ---
static char selected_ssid[33] = ""; // Store the SSID of the selected network
static int wifi_list_selected_index = -1; // -1 significa sem seleção
static char entered_password[65] = ""; // Store the password temporarily
enum WifiConnectStatus { IDLE, CONNECTING, CONNECTED, FAILED };
static WifiConnectStatus wifi_connect_status = IDLE;
static unsigned long wifi_connect_start_time = 0;
static bool is_disconnect_ui_focused = false; // Controla se a UI de "conectado" está em foco
static bool is_display_ui_focused = false; // Controla se a UI de "display" está em foco
static bool is_wifi_list_focused = false; // Controla se a navegação está na lista de Wi-Fi

// --- Function Prototypes ---
static void back_button_event_cb(lv_event_t* e);
static void setting_header_event_cb(lv_event_t* e);
static lv_obj_t* create_accordion_item(lv_obj_t* parent, const char* icon, const char* title);
static void update_wifi_list_selection_visuals();
static void wifi_list_item_click_event_cb(lv_event_t* e);
static void show_password_modal(const char* ssid);
static void attempt_wifi_connection(const char* ssid, const char* password, bool is_auto_connect = false);
static void close_password_modal();
static void password_cancel_event_cb(lv_event_t* e);
static void display_wifi_connected_ui();
static void start_wifi_scan();
static void disconnect_wifi_event_cb(lv_event_t* e);
static void auto_connect_wifi();
static void set_brightness(uint8_t value);
static void brightness_slider_event_cb(lv_event_t* e);
static void update_selection_visuals();

/**
 * @brief Retorna uma representação visual da força do sinal Wi-Fi.
 * @param rssi A força do sinal em dBm.
 * @return Uma string representando a força do sinal.
 */
static const char* get_wifi_signal_icon(int32_t rssi) {
    if (rssi >= -60) return LV_SYMBOL_BATTERY_FULL; // Forte
    if (rssi >= -70) return LV_SYMBOL_BATTERY_3;    // Bom
    if (rssi >= -80) return LV_SYMBOL_BATTERY_2;    // Médio
    if (rssi >= -90) return LV_SYMBOL_BATTERY_1;    // Fraco
    return LV_SYMBOL_BATTERY_EMPTY;                // Muito Fraco
}

/**
 * @brief Esconde todos os painéis de conteúdo do acordeão.
 */
static void close_all_accordion_items() {
    uint32_t child_count = lv_obj_get_child_cnt(main_container);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t* item_container = lv_obj_get_child(main_container, i);
        lv_obj_t* content_panel = lv_obj_get_child(item_container, 1); // O segundo filho é o painel de conteúdo
        lv_obj_add_flag(content_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief Callback para o botão de voltar. Retorna ao menu principal.
 */
static void back_button_event_cb(lv_event_t* e) {
    AppManager::show_app(AppManager::APP_MAIN_MENU);
}

/**
 * @brief Callback para o clique no cabeçalho de um item de configuração.
 * Expande ou recolhe o painel de conteúdo.
 */
static void setting_header_event_cb(lv_event_t* e) {
    // Atualiza a seleção visual quando um item é tocado
    lv_obj_t* header = lv_event_get_target(e);
    lv_obj_t* item_container = lv_obj_get_parent(header);
    int index = lv_obj_get_child_id(item_container);
    selected_setting_index = index;
    update_selection_visuals();

    lv_obj_t* content_panel = (lv_obj_t*)lv_event_get_user_data(e);
    bool is_hidden = lv_obj_has_flag(content_panel, LV_OBJ_FLAG_HIDDEN);

    // Reseta todos os focos antes de decidir o que fazer
    is_wifi_list_focused = false;
    is_disconnect_ui_focused = false;
    is_display_ui_focused = false;

    // Se o painel clicado já estava aberto, ele será fechado.
    // Apenas precisamos agir se um painel estiver sendo aberto.
    bool will_be_open = is_hidden;

    // Fecha todos os outros itens antes de abrir o novo
    close_all_accordion_items();

    // Alterna a visibilidade do item clicado
    if (will_be_open) {
        lv_obj_clear_flag(content_panel, LV_OBJ_FLAG_HIDDEN);

        // Define o foco correto com base no painel que foi aberto
        switch(selected_setting_index) {
            case 0: // Wi-Fi
                (WiFi.status() == WL_CONNECTED) ? display_wifi_connected_ui() : start_wifi_scan();
                break;
            case 1: // Display
                is_display_ui_focused = true;
                break;
        }
    }
}

/**
 * @brief Atualiza o destaque visual do item selecionado na lista de Wi-Fi.
 */
static void update_wifi_list_selection_visuals() {
    if (!wifi_list) return;
    uint32_t child_count = lv_obj_get_child_cnt(wifi_list);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t* btn = lv_obj_get_child(wifi_list, i);
        if (i == wifi_list_selected_index) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A5FB4), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_30, 0);
        } else {
            lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        }
    }
}

/**
 * @brief Callback para o clique em um item da lista de Wi-Fi.
 */
static void wifi_list_item_click_event_cb(lv_event_t* e) {
    int network_index = (intptr_t)lv_event_get_user_data(e);
    // Atualiza o índice selecionado e o feedback visual
    wifi_list_selected_index = network_index;
    update_wifi_list_selection_visuals();

    wifi_auth_mode_t encryption = WiFi.encryptionType(network_index);
    String ssid = WiFi.SSID(network_index);

    strncpy(selected_ssid, ssid.c_str(), sizeof(selected_ssid) - 1);
    selected_ssid[sizeof(selected_ssid) - 1] = '\0';

    if (encryption == WIFI_AUTH_OPEN) {
        LV_LOG_USER("Connecting to open network: %s", selected_ssid);
        strcpy(entered_password, ""); // Garante que a senha temporária está vazia
        attempt_wifi_connection(selected_ssid, "");
    } else {
        LV_LOG_USER("Secure network selected: %s. Showing password modal.", selected_ssid);
        show_password_modal(selected_ssid);
    }
}

/**
 * @brief Fecha e limpa o modal de senha.
 */
static void close_password_modal() {
    if (password_modal_cont) {
        lv_obj_del(password_modal_cont);
        password_modal_cont = nullptr;
        password_textarea = nullptr;
    }
}

/**
 * @brief Callback para o botão de cancelar no modal de senha.
 */
static void password_cancel_event_cb(lv_event_t* e) {
    LV_LOG_USER("Password entry cancelled via button.");
    close_password_modal();
}

/**
 * @brief Mostra uma janela modal com um teclado para digitar a senha do Wi-Fi.
 */
static void show_password_modal(const char* ssid) {
    // Cria um fundo semi-transparente para o modal
    password_modal_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(password_modal_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(password_modal_cont, lv_color_hex(0xFDF5E6), 0);
    lv_obj_set_style_bg_opa(password_modal_cont, LV_OPA_80, 0);
    lv_obj_set_style_border_width(password_modal_cont, 0, 0);
    lv_obj_set_style_radius(password_modal_cont, 0, 0);
    lv_obj_clear_flag(password_modal_cont, LV_OBJ_FLAG_SCROLLABLE);

    // Botão de Cancelar (X no canto)
    lv_obj_t* cancel_btn = lv_btn_create(password_modal_cont);
    lv_obj_align(cancel_btn, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_obj_set_size(cancel_btn, 40, 40);
    lv_obj_add_event_cb(cancel_btn, password_cancel_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_radius(cancel_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(cancel_btn, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x1A5FB4), 0);

    lv_obj_t* cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, LV_SYMBOL_CLOSE);
    lv_obj_center(cancel_label);

    // Cria a área de texto para a senha
    password_textarea = lv_textarea_create(password_modal_cont);
    lv_textarea_set_one_line(password_textarea, true);
    lv_textarea_set_password_mode(password_textarea, true);
    lv_obj_set_width(password_textarea, lv_pct(80));
    lv_obj_align(password_textarea, LV_ALIGN_CENTER, 0, 10);
    char placeholder[64];
    snprintf(placeholder, sizeof(placeholder), "Password for %s", ssid);
    lv_textarea_set_placeholder_text(password_textarea, placeholder);
    // Adiciona foco à área de texto para que o cursor apareça
    lv_obj_add_state(password_textarea, LV_STATE_FOCUSED);
}

/**
 * @brief (Placeholder) Inicia a tentativa de conexão Wi-Fi e mostra o status.
 */
static void attempt_wifi_connection(const char* ssid, const char* password, bool is_auto_connect) {
    if (!is_auto_connect) {
        is_wifi_list_focused = false; // Desabilita a navegação na lista enquanto conecta
        lv_obj_clean(wifi_list);
        char status_text[64];
        snprintf(status_text, sizeof(status_text), "Connecting to %s...", ssid);
        lv_list_add_text(wifi_list, status_text);
        lv_timer_handler(); // Força a UI a redesenhar
    }

    WiFi.begin(ssid, password);
    wifi_connect_status = CONNECTING;
    wifi_connect_start_time = millis();
}

/**
 * @brief Mostra a UI de status quando o Wi-Fi está conectado.
 */
static void display_wifi_connected_ui() {
    is_disconnect_ui_focused = true;
    is_wifi_list_focused = false; // Não há lista para navegar
    lv_obj_clean(wifi_content_panel); // Limpa tudo que estava no painel

    // Recria a lista para exibir o status
    wifi_list = lv_list_create(wifi_content_panel);
    lv_obj_set_width(wifi_list, lv_pct(100));
    lv_obj_set_height(wifi_list, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(wifi_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wifi_list, 0, 0);

    char status_text[64];
    snprintf(status_text, sizeof(status_text), "Connected to: %s", WiFi.SSID().c_str());
    lv_list_add_text(wifi_list, status_text);

    snprintf(status_text, sizeof(status_text), "IP: %s", WiFi.localIP().toString().c_str());
    lv_list_add_text(wifi_list, status_text);

    // Botão para desconectar
    lv_obj_t* disconnect_btn = lv_list_add_btn(wifi_list, LV_SYMBOL_CLOSE, "Disconnect");
    lv_obj_add_event_cb(disconnect_btn, disconnect_wifi_event_cb, LV_EVENT_CLICKED, NULL);

    // Aplica o estilo limpo aos itens de texto e ao botão
    uint32_t child_count = lv_obj_get_child_cnt(wifi_list);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t* child = lv_obj_get_child(wifi_list, i);
        lv_obj_set_style_bg_opa(child, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(child, 0, LV_PART_MAIN);
        lv_obj_set_style_text_color(child, lv_color_hex(0x0B3C5D), LV_PART_MAIN);
    }
}

/**
 * @brief Callback para o botão de desconectar.
 */
static void disconnect_wifi_event_cb(lv_event_t* e) {
    LV_LOG_USER("Disconnecting Wi-Fi...");
    // A lógica de "esquecer" a rede poderia ser mais complexa, por agora apenas desconecta.
    WiFi.disconnect(true);
    is_disconnect_ui_focused = false;
    wifi_connect_status = IDLE;
    // Inicia um novo escaneamento para mostrar a lista de redes novamente
    start_wifi_scan();
}

/**
 * @brief Inicia o escaneamento de redes Wi-Fi e atualiza a UI.
 */
static void start_wifi_scan() {
    wifi_connect_status = IDLE; // Garante que o status está limpo
    is_wifi_list_focused = true; // Habilita a navegação na lista
    wifi_list_selected_index = -1;

    // Garante que o rádio Wi-Fi está pronto para escanear
    LV_LOG_USER("Configuring Wi-Fi for scanning...");
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);

    lv_obj_clean(wifi_content_panel); // Limpa o painel (remove o status de conectado)

    // Recria a lista de Wi-Fi
    wifi_list = lv_list_create(wifi_content_panel);
    lv_obj_set_width(wifi_list, lv_pct(100));
    lv_obj_set_height(wifi_list, 120);
    lv_obj_align(wifi_list, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(wifi_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wifi_list, 0, 0);

    lv_list_add_text(wifi_list, "Scanning for networks...");
    // Força uma atualização imediata da tela para garantir que a mensagem "Scanning..."
    // seja exibida antes da chamada de escaneamento síncrono, que bloqueia a UI.
    lv_refr_now(NULL);

    int n = WiFi.scanNetworks(false, true);

    LV_LOG_USER("Wi-Fi scan finished, %d networks found.", n);
    lv_obj_clean(wifi_list);
    if (n == 0) {
        lv_list_add_text(wifi_list, "No networks found.");
    } else {
        int found_count = 0;
        for (int i = 0; i < n; ++i) {
            String ssid = WiFi.SSID(i);
            if (ssid.length() == 0) {
                continue;
            }
            char item_text[100];
            int32_t rssi = WiFi.RSSI(i);
            const char* encryption_symbol = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "" : " " LV_SYMBOL_EYE_CLOSE;
            snprintf(item_text, sizeof(item_text), "%s %s%s", get_wifi_signal_icon(rssi), ssid.c_str(), encryption_symbol);
            lv_obj_t* btn = lv_list_add_btn(wifi_list, NULL, item_text);
            lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
            lv_obj_set_style_text_color(btn, lv_color_hex(0x0B3C5D), LV_PART_MAIN);
            lv_obj_add_event_cb(btn, wifi_list_item_click_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            found_count++;
        }
        if (found_count == 0) {
            lv_list_add_text(wifi_list, "No visible networks found.");
        } else {
            wifi_list_selected_index = 0;
        }
    }
    update_wifi_list_selection_visuals();
}

/**
 * @brief Define o brilho da tela usando o canal LEDC.
 * @param value O brilho em porcentagem (0-100).
 */
static void set_brightness(uint8_t value) {
    // Mapeia o valor de 0-100 para o range do LEDC (0-255 para 8 bits)
    uint32_t duty_cycle = (value * 255) / 100;
    ledcWrite(LEDC_CHANNEL_BRIGHTNESS, duty_cycle);
}

/**
 * @brief Callback para o slider de brilho.
 */
static void brightness_slider_event_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);

    // Atualiza o texto do label com a porcentagem
    if (brightness_label) {
        lv_label_set_text_fmt(brightness_label, "%d%%", value);
    }

    // Define o brilho real da tela
    set_brightness(value);
}

/**
 * @brief Função auxiliar para criar um item de menu acordeão.
 * @param parent O objeto pai onde o item será criado.
 * @param icon O símbolo do ícone (ex: LV_SYMBOL_WIFI).
 * @param title O texto do título para o item.
 * @return O painel de conteúdo (inicialmente oculto) para adicionar widgets.
 */
static lv_obj_t* create_accordion_item(lv_obj_t* parent, const char* icon, const char* title) {
    // Contêiner para o item (cabeçalho + conteúdo)
    lv_obj_t* item_container = lv_obj_create(parent);
    lv_obj_set_width(item_container, lv_pct(100));
    lv_obj_set_height(item_container, LV_SIZE_CONTENT); // Garante que o contêiner se ajuste ao conteúdo
    lv_obj_set_layout(item_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(item_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(item_container, 0, 0);
    lv_obj_set_style_bg_opa(item_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(item_container, 0, 0);

    // Cabeçalho clicável
    lv_obj_t* header = lv_obj_create(item_container);
    lv_obj_add_flag(header, LV_OBJ_FLAG_CLICKABLE); // Torna o objeto clicável
    lv_obj_set_height(header, LV_SIZE_CONTENT); // Garante que o cabeçalho se ajuste ao conteúdo
    lv_obj_set_width(header, lv_pct(100));
    // Remove o estilo padrão do botão para um visual mais limpo
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_shadow_width(header, 0, 0);
    lv_obj_set_style_radius(header, 8, 0);
    lv_obj_set_style_pad_ver(header, 8, 0); // Aumenta o padding para dar mais altura ao item
    lv_obj_set_style_pad_hor(header, 5, 0);

    lv_obj_t* icon_label = lv_label_create(header);
    lv_label_set_text(icon_label, icon);
    lv_obj_set_style_text_color(icon_label, lv_color_hex(0x0B3C5D), 0);
    lv_obj_align(icon_label, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* title_label = lv_label_create(header);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x0B3C5D), 0);
    lv_obj_align_to(title_label, icon_label, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // Painel de conteúdo (inicialmente oculto)
    lv_obj_t* content_panel = lv_obj_create(item_container);
    lv_obj_set_width(content_panel, lv_pct(100));
    lv_obj_set_height(content_panel, LV_SIZE_CONTENT);
    lv_obj_add_flag(content_panel, LV_OBJ_FLAG_HIDDEN); // Começa oculto
    lv_obj_set_style_pad_left(content_panel, 20, 0); // Indenta o conteúdo para alinhar abaixo do título
    lv_obj_set_style_pad_right(content_panel, 10, 0);
    lv_obj_set_style_pad_ver(content_panel, 5, 0);
    lv_obj_set_style_bg_opa(content_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content_panel, 0, 0);

    // Adiciona o evento de clique ao cabeçalho para mostrar/ocultar o conteúdo
    lv_obj_add_event_cb(header, setting_header_event_cb, LV_EVENT_CLICKED, content_panel);

    return content_panel;
}

/**
 * @brief Salva uma rede Wi-Fi na lista de redes recentes.
 * @param ssid O SSID da rede.
 * @param password A senha da rede.
 */
static void save_wifi_network(const char* ssid, const char* password) {
    preferences.begin("wifi-creds", false);

    // Lê a lista existente ou cria uma nova
    String json_string = preferences.getString(SAVED_NETWORKS_KEY, "[]");
    JsonDocument old_doc;
    deserializeJson(old_doc, json_string);
    JsonArray old_array = old_doc.as<JsonArray>();

    // Cria um novo documento e array para a lista atualizada
    JsonDocument new_doc;
    JsonArray new_array = new_doc.to<JsonArray>();

    // Adiciona a nova rede no início da lista
    JsonObject new_entry = new_array.add<JsonObject>();
    new_entry["ssid"] = ssid;
    new_entry["pass"] = password;

    // Adiciona as redes antigas, pulando qualquer duplicata do SSID recém-adicionado
    for (JsonObject old_entry : old_array) {
        if (new_array.size() >= MAX_SAVED_NETWORKS) {
            break; // Para de adicionar se o limite for atingido
        }
        if (strcmp(old_entry["ssid"], ssid) != 0) {
            new_array.add(old_entry);
        }
    }

    // Salva a lista atualizada
    String new_json_string;
    serializeJson(new_doc, new_json_string);
    preferences.putString(SAVED_NETWORKS_KEY, new_json_string);

    preferences.end();
    LV_LOG_USER("Wi-Fi network list updated.");
}

/**
 * @brief Tenta se conectar automaticamente ao Wi-Fi na inicialização usando credenciais salvas.
 */
static void auto_connect_wifi() {
    preferences.begin("wifi-creds", true); // Abre em modo somente leitura
    String json_string = preferences.getString(SAVED_NETWORKS_KEY, "[]");
    preferences.end();

    JsonDocument doc; // Usar JsonDocument aqui também para consistência
    deserializeJson(doc, json_string);
    JsonArray array = doc.as<JsonArray>();

    if (array.size() == 0) {
        LV_LOG_USER("No saved Wi-Fi networks found.");
        return;
    }

    for (JsonObject network : array) {
        const char* ssid = network["ssid"];
        const char* pass = network["pass"];
        LV_LOG_USER("Attempting to auto-connect to: %s", ssid);
        WiFi.begin(ssid, pass);

        unsigned long start_time = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start_time < 10000) { // Timeout de 10s por rede
            delay(500);
        }

        if (WiFi.status() == WL_CONNECTED) {
            LV_LOG_USER("Auto-connected successfully to %s", ssid);
            // Re-salva para mover esta rede para o topo da lista de "mais recentes"
            save_wifi_network(ssid, pass);
            return; // Sai da função ao conectar
        } else {
            LV_LOG_USER("Failed to connect to %s", ssid);
            WiFi.disconnect(); // Garante que está limpo para a próxima tentativa
        }
    }
}

/**
 * @brief Inicializa a tela de configurações e seus elementos.
 */
inline void init() {
    settings_screen = lv_obj_create(NULL);
    lv_obj_add_flag(settings_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(settings_screen, 320, 240);
    lv_obj_set_style_bg_color(settings_screen, lv_color_hex(0xFDF5E6), LV_PART_MAIN);

    // --- Botão de Voltar ---
    lv_obj_t* back_btn = lv_btn_create(settings_screen);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x1A5FB4), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_radius(back_btn, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(back_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(back_btn, 0, LV_PART_MAIN);

    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_16, 0);
    lv_obj_center(back_label);

    // --- Título da Tela ---
    lv_obj_t* screen_title = lv_label_create(settings_screen);
    lv_label_set_text(screen_title, "Settings");
    lv_obj_set_style_text_font(screen_title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(screen_title, lv_color_hex(0x0B3C5D), 0);
    lv_obj_align(screen_title, LV_ALIGN_TOP_MID, 0, 10);

    // --- Contêiner principal para a lista de configurações ---
    main_container = lv_obj_create(settings_screen);
    lv_obj_set_size(main_container, 320, 240 - 55);
    lv_obj_align(main_container, LV_ALIGN_TOP_LEFT, 0, 55);
    lv_obj_set_layout(main_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(main_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(main_container, 0, 0); // Remove completamente o espaçamento entre os itens
    lv_obj_set_style_pad_hor(main_container, 10, 0);
    lv_obj_set_style_bg_opa(main_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_container, 0, 0);

    // Habilita a animação para as mudanças de layout (efeito de expandir/recolher)
    static lv_style_t style_cont_transitions;
    lv_style_init(&style_cont_transitions);
    lv_style_set_anim_time(&style_cont_transitions, 200); // Animação de 200ms
    lv_obj_add_style(main_container, &style_cont_transitions, 0);

    // --- Item 1: Wi-Fi ---
    // Armazena o painel de conteúdo do Wi-Fi para identificá-lo mais tarde
    wifi_content_panel = create_accordion_item(main_container, LV_SYMBOL_WIFI, "Wi-Fi");

    wifi_list = lv_list_create(wifi_content_panel);
    lv_obj_set_width(wifi_list, lv_pct(100));
    lv_obj_set_height(wifi_list, 120); // Define uma altura fixa para a lista
    lv_obj_align(wifi_list, LV_ALIGN_TOP_LEFT, 0, 0);
    // Remove o estilo da lista para que não interfira com os botões internos
    lv_obj_set_style_bg_opa(wifi_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wifi_list, 0, 0);

    // --- Item 2: Display (Placeholder) ---
    lv_obj_t* display_content = create_accordion_item(main_container, LV_SYMBOL_SETTINGS, "Display");

    // Label para o slider
    lv_obj_t* slider_title = lv_label_create(display_content);
    lv_label_set_text(slider_title, "Brightness");
    lv_obj_set_style_text_color(slider_title, lv_color_hex(0x0B3C5D), 0);
    lv_obj_align(slider_title, LV_ALIGN_TOP_LEFT, 0, 0);

    // Slider de brilho
    brightness_slider = lv_slider_create(display_content);
    lv_obj_set_width(brightness_slider, lv_pct(80));
    lv_obj_align_to(brightness_slider, slider_title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    lv_slider_set_range(brightness_slider, 10, 100); // Mínimo de 10% para não apagar a tela
    lv_obj_add_event_cb(brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Label para mostrar a porcentagem
    brightness_label = lv_label_create(display_content);
    lv_obj_set_style_text_color(brightness_label, lv_color_hex(0x0B3C5D), 0);
    lv_obj_align_to(brightness_label, brightness_slider, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // Inicializa o controle de brilho
    ledcSetup(LEDC_CHANNEL_BRIGHTNESS, 5000, 8); // Canal, Frequência, Resolução (8 bits)
    ledcAttachPin(BACKLIGHT_PIN, LEDC_CHANNEL_BRIGHTNESS);
    // Define o valor inicial e atualiza a UI
    int initial_brightness = 80;
    lv_slider_set_value(brightness_slider, initial_brightness, LV_ANIM_OFF);
    set_brightness(initial_brightness);
    lv_label_set_text_fmt(brightness_label, "%d%%", initial_brightness);

    // --- Item 3: About (Placeholder) ---
    // LV_SYMBOL_INFO pode não estar habilitado na sua configuração do LVGL. Usando LV_SYMBOL_FILE como uma alternativa segura.
    lv_obj_t* about_content = create_accordion_item(main_container, LV_SYMBOL_FILE, "About");
    lv_obj_t* about_label = lv_label_create(about_content);
    lv_obj_set_style_text_color(about_label, lv_color_hex(0x0B3C5D), 0);
    char about_text[256];
    snprintf(about_text, sizeof(about_text),
             "Firmware: v1.0\n"
             "Developer: Gabriel\n"
             "Chip: %s\n"
             "Cores: %d\n"
             "Free RAM: %d KB",
             ESP.getChipModel(),
             ESP.getChipCores(),
             ESP.getFreeHeap() / 1024);
    lv_label_set_text(about_label, about_text);

    auto_connect_wifi();

    update_selection_visuals();
}

static void update_selection_visuals() {
    for (int i = 0; i < SETTING_ITEM_COUNT; i++) {
        lv_obj_t* item_container = lv_obj_get_child(main_container, i);
        lv_obj_t* header = lv_obj_get_child(item_container, 0); // O cabeçalho é o primeiro filho
        if (i == selected_setting_index) {
            // Destaca o item selecionado com um fundo sutil
            lv_obj_set_style_bg_color(header, lv_color_hex(0x1A5FB4), 0);
            lv_obj_set_style_bg_opa(header, LV_OPA_30, 0);
        } else {
            // Garante que itens não selecionados fiquem transparentes
            lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
        }
    }
}

/**
 * @brief Mostra a tela de configurações.
 */
inline void show() {
    if (settings_screen) {
        // A lógica de inicialização do Wi-Fi foi movida para auto_connect_wifi e start_wifi_scan
        selected_setting_index = 0;
        lv_obj_clear_flag(settings_screen, LV_OBJ_FLAG_HIDDEN);
        lv_scr_load(settings_screen);
    }
}

/**
 * @brief Gerencia a entrada do usuário e atualiza a interface.
 */
inline void handle() {
    // Se o modal de senha estiver aberto, desabilita a navegação de fundo
    if (password_modal_cont) {
        char key = Keyboard::get_key();
        if (key > 0) {
            switch (key) {
                case '\n':
                case '\r': { // Tecla Enter - Conectar
                    const char* password = lv_textarea_get_text(password_textarea);
                    strncpy(entered_password, password, sizeof(entered_password) - 1);
                    entered_password[sizeof(entered_password) - 1] = '\0';
                    LV_LOG_USER("Attempting to connect to %s with password: %s", selected_ssid, password);
                    attempt_wifi_connection(selected_ssid, password, false);
                    close_password_modal();
                    break;
                }
                case KEY_BACKSPACE: // Tecla Backspace
                    lv_textarea_del_char(password_textarea);
                    break;
                case KEY_CLEAR_ENTRY: // Tecla 'C' - Cancelar
                    LV_LOG_USER("Password entry cancelled via key.");
                    close_password_modal();
                    break;
                default: // Qualquer outro caractere
                    if (isprint(key)) {
                        lv_textarea_add_char(password_textarea, key);
                    }
                    break;
            }
        }
        lv_timer_handler();
        return;
    }
    // --- Lógica de conexão Wi-Fi ---
    if (wifi_connect_status == CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            LV_LOG_USER("Successfully connected to %s", selected_ssid);
            // Salva a rede na lista de redes recentes
            save_wifi_network(selected_ssid, entered_password);
            strcpy(entered_password, ""); // Limpa a senha temporária

            wifi_connect_status = CONNECTED;
            display_wifi_connected_ui();
        } else if (millis() - wifi_connect_start_time > 15000) { // 15 segundos de timeout
            LV_LOG_USER("Connection to %s timed out.", selected_ssid);
            WiFi.disconnect();
            lv_obj_clean(wifi_list);
            lv_list_add_text(wifi_list, "Connection Failed.");
            wifi_connect_status = FAILED;
        }
        // Enquanto estiver conectando, pulamos a navegação de fundo
        lv_timer_handler();
        return;
    }

    if (is_display_ui_focused) {
        // --- Lógica de navegação do slider de brilho ---
        int32_t current_value = lv_slider_get_value(brightness_slider);
        if (Trackball::moved_right()) {
            if (current_value < 100) lv_slider_set_value(brightness_slider, current_value + 5, LV_ANIM_ON);
        }
        if (Trackball::moved_left()) {
            if (current_value > 10) lv_slider_set_value(brightness_slider, current_value - 5, LV_ANIM_ON);
        }
        // O clique não faz nada aqui, mas mantemos o loop do LVGL rodando
        lv_timer_handler();
        return;
    }

    if (is_disconnect_ui_focused) {
        // Foca e permite clicar no botão "Disconnect"
        uint32_t child_count = lv_obj_get_child_cnt(wifi_list);
        if (child_count > 0) {
            // O botão de desconectar é o último item da lista
            lv_obj_t* disconnect_btn = lv_obj_get_child(wifi_list, child_count - 1);
            lv_obj_set_style_bg_color(disconnect_btn, lv_color_hex(0x1A5FB4), 0);
            lv_obj_set_style_bg_opa(disconnect_btn, LV_OPA_30, 0);

            if (Trackball::clicked()) {
                lv_event_send(disconnect_btn, LV_EVENT_CLICKED, NULL);
            }
        }
        lv_timer_handler();
        return;
    }

    if (is_wifi_list_focused) {
        // --- Lógica de navegação da lista de Wi-Fi ---
        uint32_t item_count = lv_obj_get_child_cnt(wifi_list);
        bool selection_changed = false;
        if (Trackball::moved_down() && item_count > 0) {
            if (wifi_list_selected_index < (int)item_count - 1) {
                wifi_list_selected_index++;
                selection_changed = true;
            }
        }
        if (Trackball::moved_up() && item_count > 0) {
            if (wifi_list_selected_index > 0) {
                wifi_list_selected_index--;
                selection_changed = true;
            }
        }
        if (selection_changed) {
            update_wifi_list_selection_visuals();
        }
        if (Trackball::clicked() && wifi_list_selected_index != -1) {
            lv_obj_t* selected_btn = lv_obj_get_child(wifi_list, wifi_list_selected_index);
            lv_event_send(selected_btn, LV_EVENT_CLICKED, NULL);
        }
    } else {
        // --- Lógica de navegação do menu principal de configurações ---
        bool selection_changed = false;
        if (Trackball::moved_down()) {
            if (selected_setting_index < SETTING_ITEM_COUNT - 1) {
                selected_setting_index++;
                selection_changed = true;
            }
        }
        if (Trackball::moved_up()) {
            if (selected_setting_index > 0) {
                selected_setting_index--;
                selection_changed = true;
            }
        }

        if (selection_changed) {
            update_selection_visuals();
        }

        if (Trackball::clicked()) {
            lv_event_send(lv_obj_get_child(lv_obj_get_child(main_container, selected_setting_index), 0), LV_EVENT_CLICKED, NULL);
        }
    }

    lv_timer_handler();
}

} // namespace Settings