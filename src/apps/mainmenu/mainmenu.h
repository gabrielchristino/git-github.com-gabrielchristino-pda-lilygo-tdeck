#pragma once
#include <lvgl.h>
#include "input/input.h"

// Este arquivo é incluído a partir de `apps.h`, que já define o AppManager.
// Portanto, não são necessárias declarações prévias ou includes adicionais aqui.

// Inclua todos os arquivos .c dos ícones aqui.
// Certifique-se de que esses arquivos foram gerados pelo LVGL Image Converter
// e estão no caminho correto do seu projeto.
#include "Icons/lv_img_calculator.c"
#include "Icons/lv_img_notes.c"
#include "Icons/lv_img_sketch.c"
#include "Icons/lv_img_weather.c"
#include "Icons/lv_img_calendar.c"
#include "Icons/lv_img_map.c"
#include "Icons/lv_img_settings.c"
#include "Icons/lv_img_audio.c"
#include "Icons/lv_img_about.c"



/**
 * PDA Launcher - Menu de Aplicativos com Grade
 *
 * Exibe uma grade 3x3 de aplicativos, recriando a interface da imagem.
 * A navegação é feita pelo trackball, e o item selecionado é destacado
 * com uma borda azul e uma animação sutil de pulso no ícone.
 *
 * 📦 Exemplo de uso:
 *
 * #include "ui/mainmenu.h"
 *
 * void setup() {
 * Serial.begin(115200);
 * DisplayTouch::init();
 * Trackball::init();
 * MainMenu::init();
 * }
 *
 * void loop() {
 * MainMenu::handle();
 * }
 */
namespace MainMenu {

#define APP_ROWS 3
#define APP_COLS 4
#define MAX_APPS (APP_ROWS * APP_COLS)

// Nomes dos aplicativos, conforme a imagem
const char* app_names[MAX_APPS] = {
    "Calculator", "Notes", "Sketch",
    "Weather", "Calendar", "Map",
    "Settings", "Audio", "About"
};

// Array de ponteiros para as estruturas lv_img_dsc_t dos ícones.
// O LVGL Image Converter gera variáveis com o prefixo 'lv_img_'.
const lv_img_dsc_t* app_icons[MAX_APPS] = {
    &lv_img_calculator, // Referência ao ícone da calculadora
    &lv_img_notes,      // Referência ao ícone de notas
    &lv_img_sketch,     // Referência ao ícone de esboço/pintura
    &lv_img_weather,    // Referência ao ícone de clima/sol
    &lv_img_calendar,   // Referência ao ícone de calendário
    &lv_img_map,        // Referência ao ícone de mapa
    &lv_img_settings,   // Referência ao ícone de configurações
    &lv_img_audio,      // Referência ao ícone de áudio
    &lv_img_about       // Referência ao ícone de informações
};

// --- Variáveis de UI ---
static lv_obj_t* main_menu_screen; // Tela própria para o menu
lv_obj_t* app_containers[MAX_APPS];
lv_obj_t* app_icons_img[MAX_APPS];
lv_style_t style_container_normal;
lv_style_t style_container_selected;
int8_t selected_app_index = 4; // Começa com o "Calendar" selecionado (índice 4)

/**
 * Anima o ícone selecionado com um efeito de "pulso" para dar feedback visual.
 */
inline void animate_selected_icon(lv_obj_t* icon) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, icon);
    lv_anim_set_values(&a, LV_IMG_ZOOM_NONE, LV_IMG_ZOOM_NONE + 20); // Zoom sutil
    lv_anim_set_time(&a, 150);
    lv_anim_set_playback_time(&a, 150);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) {
        lv_img_set_zoom((lv_obj_t*)obj, v);
    });
    lv_anim_start(&a);
}

/**
 * Atualiza o estado visual dos contêineres dos aplicativos, aplicando o
 * estilo de seleção ao item correto e o estilo normal aos demais.
 */
inline void update_selection_visuals() {
    for (int i = 0; i < MAX_APPS; ++i) {
        if (i == selected_app_index) {
            // Aplica o estilo de seleção (borda azul)
            lv_obj_add_style(app_containers[i], &style_container_selected, LV_PART_MAIN);
            animate_selected_icon(app_icons_img[i]);
        } else {
            // Garante que o estilo de seleção foi removido e aplica o normal
            lv_obj_remove_style(app_containers[i], &style_container_selected, LV_PART_MAIN);
            lv_obj_add_style(app_containers[i], &style_container_normal, LV_PART_MAIN);
            // Reseta o zoom de ícones não selecionados
            lv_img_set_zoom(app_icons_img[i], LV_IMG_ZOOM_NONE-50);
        }
    }
}

// Forward declaration for the launch function used in the touch callback
inline void launch_selected_app();

/**
 * Callback de evento para quando um ícone de aplicativo é clicado via touch.
 * Seleciona e inicia o aplicativo correspondente.
 * @param e O evento LVGL.
 */
static void app_click_event_cb(lv_event_t* e) {
    // Obtém o índice do aplicativo a partir dos dados do usuário.
    intptr_t app_index = (intptr_t)lv_event_get_user_data(e);

    // Atualiza o índice do aplicativo selecionado.
    selected_app_index = app_index;

    // Atualiza a interface para destacar o novo ícone e animá-lo.
    update_selection_visuals();

    // Abre o aplicativo selecionado.
    launch_selected_app();
}

/**
 * Inicializa a tela principal, os estilos e a grade de aplicativos.
 */
inline void init() {
    // Cria uma tela dedicada para o menu principal.
    main_menu_screen = lv_obj_create(NULL);

    // --- Estilo da Tela Principal ---
    lv_obj_set_style_bg_color(main_menu_screen, lv_color_hex(0xFDF5E6), LV_PART_MAIN);


    // --- Estilo para Contêineres de App (Normal) ---
    // Transparente, sem borda, para que apenas o ícone e o texto apareçam
    lv_style_init(&style_container_normal);
    lv_style_set_bg_opa(&style_container_normal, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_container_normal, 0);
    lv_style_set_radius(&style_container_normal, 12); // Cantos arredondados
    lv_style_set_pad_all(&style_container_normal, 0);
    // lv_style_set_pad_ver(&style_container_normal, 4); // Espaçamento vertical para o efeito de pulso

    // --- Estilo para Contêiner de App (Selecionado) ---
    lv_style_init(&style_container_selected);
    lv_style_set_bg_color(&style_container_selected, lv_color_hex(0x1A5FB4));
    lv_style_set_bg_opa(&style_container_selected, LV_OPA_50);
    lv_style_set_border_color(&style_container_selected, lv_color_hex(0x0B3C5D)); // Azul escuro
    lv_style_set_border_width(&style_container_selected, 0);
    lv_style_set_radius(&style_container_selected, 12);
    lv_style_set_pad_all(&style_container_selected, 0); // Ajusta o padding para compensar a borda
    // lv_style_set_pad_ver(&style_container_selected, 4); // Espaçamento vertical para o efeito de pulso

    // --- Cria a Grade de Aplicativos ---
    lv_obj_t* grid = lv_obj_create(main_menu_screen);
    lv_obj_set_size(grid, 320, 240);
    lv_obj_align(grid, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0); // Torna o fundo da grade transparente
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_style_pad_gap(grid, 0, 0);
    

    static lv_coord_t col_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

    for (int i = 0; i < MAX_APPS; ++i) {
        int col = i % APP_COLS;
        int row = i / APP_COLS;

        // Contêiner para cada app (ícone + texto)
        app_containers[i] = lv_obj_create(grid);
        lv_obj_set_grid_cell(app_containers[i], LV_GRID_ALIGN_START, col, 1, LV_GRID_ALIGN_CENTER, row, 1);
        lv_obj_set_size(app_containers[i], 80, 80);
        lv_obj_add_style(app_containers[i], &style_container_normal, LV_PART_MAIN);
        lv_obj_clear_flag(app_containers[i], LV_OBJ_FLAG_SCROLLABLE);

        // Habilita a interatividade por toque e adiciona o callback de clique.
        lv_obj_add_flag(app_containers[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(app_containers[i], app_click_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        // Ícone do aplicativo
        app_icons_img[i] = lv_img_create(app_containers[i]);
        lv_img_set_src(app_icons_img[i], app_icons[i]);
        lv_obj_align(app_icons_img[i], LV_ALIGN_TOP_MID, 0, -15);
        lv_img_set_zoom(app_icons_img[i], LV_IMG_ZOOM_NONE - 20);

        // Rótulo do aplicativo
        lv_obj_t* label = lv_label_create(app_containers[i]);
        lv_obj_set_style_text_color(label, lv_color_hex(0x0B3C5D), 0);
        lv_label_set_text(label, app_names[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
        lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, 0);
    }

    update_selection_visuals();
}
/**
 * Mostra a tela do menu principal.
 * Esta função é necessária para que o AppManager possa alternar de volta para o menu.
 */
inline void show() {
    if (main_menu_screen) {
        lv_scr_load(main_menu_screen);
    }
}
/**
 * Simula a abertura de um aplicativo.
 */
inline void launch_selected_app() {
    const char* selected_app_name = app_names[selected_app_index];
    Serial.printf("Abrindo aplicativo: %s\n", selected_app_name);

    // Compara o nome do aplicativo selecionado e chama a função 'show' correspondente.
    if (strcmp(selected_app_name, "Calculator") == 0) {
        // Pede ao AppManager para trocar para a Calculadora.
        AppManager::show_app(AppManager::APP_CALCULATOR);
    } else if (strcmp(selected_app_name, "Settings") == 0) {
        AppManager::show_app(AppManager::APP_SETTINGS);
    } else if (strcmp(selected_app_name, "Weather") == 0) {
        AppManager::show_app(AppManager::APP_WEATHER);
    } else if (strcmp(selected_app_name, "Notes") == 0) {
        AppManager::show_app(AppManager::APP_NOTES);
    }
}

/**
 * Gerencia a entrada do trackball e atualiza a interface.
 */
inline void handle() {
    int8_t col = selected_app_index % APP_COLS;
    int8_t row = selected_app_index / APP_COLS;

    bool selection_changed = false;

    if (Trackball::moved_left() && col > 0) {
        selected_app_index--;
        selection_changed = true;
    }
    if (Trackball::moved_right() && col < APP_COLS - 1) {
        selected_app_index++;
        selection_changed = true;
    }
    if (Trackball::moved_up() && row > 0) {
        selected_app_index -= APP_COLS;
        selection_changed = true;
    }
    if (Trackball::moved_down() && row < APP_ROWS - 1) {
        selected_app_index += APP_COLS;
        selection_changed = true;
    }

    if (selection_changed) {
        update_selection_visuals();
    }

    if (Trackball::clicked()) {
        launch_selected_app();
    }

    // Chama o handler do LVGL para processar eventos e redesenhar
    lv_timer_handler();
}

} // namespace MainMenu
