#pragma once
#include <lvgl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include "input/input.h"

// Font declarations for all used fonts
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_18);
LV_FONT_DECLARE(lv_font_montserrat_28);
LV_FONT_DECLARE(lv_font_montserrat_36);

namespace Calculator {

// --- Constants ---
const char KEY_BACKSPACE = 8;
const char KEY_CLEAR_ENTRY = 2; // Corresponds to a special key from the firmware (e.g., 'C')

// --- UI Variables ---
static lv_obj_t* calculator_screen = nullptr;
static lv_obj_t* expression_label = nullptr; // Secondary display for expression
static lv_obj_t* display_label = nullptr;    // Main display for numbers and results
static lv_obj_t* history_list = nullptr;     // List for calculation history
static lv_obj_t* display_container = nullptr;

// --- Calculator State Variables ---
static char display_buffer[32] = "0";
static char expression_buffer[64] = ""; // Buffer for the full expression string
static double stored_value = 0.0;       // Stores the first operand of an operation
static char active_operator = '\0';     // Stores the pending operator (+, -, *, /)
static bool should_clear_display = true; // If true, next digit press clears the display
static bool error_state = false;
static bool just_calculated = false; // Flag to check if equals was just pressed

// --- History ---
#define MAX_HISTORY 20
static char calculation_history[MAX_HISTORY][64];
static int history_count = 0;

// --- Function Prototypes ---
inline void update_display();
static void handle_digit_press(const char* txt);
static void handle_decimal_press();
static void handle_operator_press(char op);
static void handle_clear_press();
static void handle_equals_press();
static void handle_factorial_press();
static void handle_backspace_press();
static void back_button_event_cb(lv_event_t* e);
static void show_history();
static void perform_calculation();
static void update_expression_and_display();

/**
 * @brief Updates both the main display and the expression label.
 */
inline void update_display() {
    if (strlen(expression_buffer) > 0) {
        lv_obj_clear_flag(expression_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(expression_label, expression_buffer);
    } else {
        lv_obj_add_flag(expression_label, LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text(display_label, display_buffer);
}

/**
 * @brief Rebuilds the expression string based on the current state and updates the UI.
 */
static void update_expression_and_display() {
    if (active_operator != '\0') {
        snprintf(expression_buffer, sizeof(expression_buffer), "%.6g %c %s", stored_value, active_operator, display_buffer);
    } else {
        strcpy(expression_buffer, display_buffer);
    }
    update_display();
}

/**
 * @brief Adds a completed calculation to the history log.
 */
static void add_to_history(const char* entry) {
    if (history_count >= MAX_HISTORY) {
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            strcpy(calculation_history[i], calculation_history[i + 1]);
        }
        history_count = MAX_HISTORY - 1;
    }
    strncpy(calculation_history[history_count], entry, sizeof(calculation_history[0]) - 1);
    calculation_history[history_count][sizeof(calculation_history[0]) - 1] = '\0';
    history_count++;
}

/**
 * @brief Displays the history list UI element.
 */
static void show_history() {
    if (!history_list) return;

    lv_obj_clean(history_list);
    for (int i = 0; i < history_count; i++) {
        lv_list_add_text(history_list, calculation_history[i]);
    }

    uint32_t child_count = lv_obj_get_child_cnt(history_list);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t* child = lv_obj_get_child(history_list, i);
        lv_obj_set_style_bg_opa(child, 0, LV_PART_MAIN);
        lv_obj_set_style_text_color(child, lv_color_hex(0x0B3C5D), LV_PART_MAIN);
    }
}

/**
 * @brief Resets the calculator to its initial state.
 */
static void handle_clear_press() {
    strcpy(display_buffer, "0");
    strcpy(expression_buffer, "");
    stored_value = 0.0;
    active_operator = '\0';
    should_clear_display = true;
    error_state = false;
    just_calculated = false;
    update_display();
}

/**
 * @brief Handles a digit button press.
 */
static void handle_digit_press(const char* txt) {
    if (error_state) return;

    if (just_calculated) {
        handle_clear_press();
    }

    if (should_clear_display) {
        strcpy(display_buffer, txt);
        should_clear_display = false;
    } else {
        if (strcmp(display_buffer, "0") == 0) {
            strcpy(display_buffer, txt);
        } else if (strlen(display_buffer) < sizeof(display_buffer) - 2) {
            strcat(display_buffer, txt);
        }
    }
    update_expression_and_display();
}

/**
 * @brief Handles the backspace button press.
 */
static void handle_backspace_press() {
    if (should_clear_display || error_state || just_calculated) {
        handle_clear_press();
        return;
    }

    int len = strlen(display_buffer);
    if (len > 1) {
        display_buffer[len - 1] = '\0';
    } else if (len == 1 && display_buffer[0] != '0') {
        strcpy(display_buffer, "0");
    }

    update_expression_and_display();
}

/**
 * @brief Handles the decimal point button press.
 */
static void handle_decimal_press() {
    if (error_state) return;
    if (strchr(display_buffer, '.') != NULL) return;

    if (just_calculated) {
        handle_clear_press();
    }

    if (should_clear_display) {
        strcpy(display_buffer, "0.");
        should_clear_display = false;
    } else {
        if (strlen(display_buffer) < sizeof(display_buffer) - 2) {
            strcat(display_buffer, ".");
        }
    }

    update_expression_and_display();
}

/**
 * @brief Performs the actual calculation based on the stored value, operator, and current display value.
 */
static void perform_calculation() {
    if (error_state || active_operator == '\0') return;

    double current_value = atof(display_buffer);
    switch (active_operator) {
        case '+': stored_value += current_value; break;
        case '-': stored_value -= current_value; break;
        case '*': stored_value *= current_value; break;
        case '/':
            if (current_value != 0) {
                stored_value /= current_value;
            } else {
                strcpy(display_buffer, "Error");
                error_state = true;
            }
            break;
        case '^':
            stored_value = pow(stored_value, current_value);
            break;
    }

    if (isinf(stored_value) || isnan(stored_value)) {
        strcpy(display_buffer, "Error");
        error_state = true;
    }

    if (!error_state) {
        snprintf(display_buffer, sizeof(display_buffer), "%.6g", stored_value);
    }
}

/**
 * @brief Handles the equals button press.
 */
static void handle_equals_press() {
    if (active_operator == '\0' || error_state) return;

    // A expression_buffer já contém a expressão completa (ex: "3 * 2").
    // Vamos usá-la diretamente para o histórico.
    char final_expression[64];
    strcpy(final_expression, expression_buffer);

    perform_calculation();

    if (!error_state) {
        char history_entry[64];
        // Agora, criamos a entrada do histórico corretamente: "expressão = resultado"
        snprintf(history_entry, sizeof(history_entry), "%s = %s",
                 final_expression, display_buffer);
        add_to_history(history_entry);
        show_history();
    }

    strcpy(expression_buffer, ""); // Limpa a expressão para a próxima conta
    should_clear_display = true;
    just_calculated = true;
    active_operator = '\0';
    update_display();
}

/**
 * @brief Handles an operator button press.
 */
static void handle_operator_press(char op) {
    if (error_state) return;

    if (!should_clear_display && active_operator != '\0') {
        // É um cálculo encadeado (ex: 3 + 2 [+])
        // O expression_buffer contém "3 + 2" neste momento.
        // Precisamos salvar isso no histórico antes que seja sobrescrito.
        char intermediate_expression[64];
        strcpy(intermediate_expression, expression_buffer);

        perform_calculation();

        if (!error_state) {
            char history_entry[64];
            // O display_buffer agora contém o resultado (ex: "5")
            snprintf(history_entry, sizeof(history_entry), "%s = %s", intermediate_expression, display_buffer);
            add_to_history(history_entry);
            show_history();
        }
    }

    if (error_state) {
        update_display();
        return;
    }

    stored_value = atof(display_buffer);
    active_operator = op;
    // Constrói a string da expressão para mostrar o primeiro número e o operador
    snprintf(expression_buffer, sizeof(expression_buffer), "%.6g %c", stored_value, op);

    should_clear_display = true;
    just_calculated = false;
    update_display();
}

/**
 * @brief Handles the factorial button press.
 */
static void handle_factorial_press() {
    if (error_state) return;

    double current_value = atof(display_buffer);
    snprintf(expression_buffer, sizeof(expression_buffer), "fact(%.6g)", current_value);

    if (current_value < 0 || fmod(current_value, 1.0) != 0.0 || current_value > 170) {
        strcpy(display_buffer, "Error");
        error_state = true;
    } else {
        double result = 1.0;
        for (long long i = 1; i <= (long long)current_value; ++i) {
            result *= i;
        }
        snprintf(display_buffer, sizeof(display_buffer), "%.6g", result);
    }
    
    char history_entry[64];
    snprintf(history_entry, sizeof(history_entry), "%s = %s", expression_buffer, display_buffer);
    add_to_history(history_entry);
    show_history();

    strcpy(expression_buffer, "");
    should_clear_display = true;
    just_calculated = true;
    update_display();
}

static void back_button_event_cb(lv_event_t* e) {
    AppManager::show_app(AppManager::APP_MAIN_MENU);
}

inline void hide() {
    if (calculator_screen) {
        lv_obj_add_flag(calculator_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

inline void init() {
    calculator_screen = lv_obj_create(NULL);
    lv_obj_add_flag(calculator_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(calculator_screen, 320, 240);
    lv_obj_set_style_bg_color(calculator_screen, lv_color_hex(0xFDF5E6), LV_PART_MAIN);

    lv_obj_t* back_btn = lv_btn_create(calculator_screen);
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
    lv_obj_t* screen_title = lv_label_create(calculator_screen);
    lv_label_set_text(screen_title, "Calculator");
    lv_obj_set_style_text_font(screen_title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(screen_title, lv_color_hex(0x0B3C5D), 0);
    lv_obj_align(screen_title, LV_ALIGN_TOP_MID, 0, 10);

    display_container = lv_obj_create(calculator_screen);
    lv_obj_set_size(display_container, 310, 180);
    lv_obj_align(display_container, LV_ALIGN_TOP_LEFT, 5, 40);
    lv_obj_set_style_bg_color(display_container, lv_color_hex(0xFDF5E6), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(display_container, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_radius(display_container, 12, LV_PART_MAIN);
    lv_obj_set_style_border_color(display_container, lv_color_hex(0x1A5FB4), LV_PART_MAIN);
    lv_obj_set_style_border_width(display_container, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(display_container, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(display_container, 0, LV_PART_MAIN);

    history_list = lv_list_create(display_container);
    lv_obj_set_size(history_list, 290, 100);
    lv_obj_align(history_list, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(history_list, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(history_list, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(history_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(history_list, 8, LV_PART_MAIN);
    lv_obj_set_style_text_color(history_list, lv_color_hex(0x0B3C5D), LV_PART_MAIN);

    expression_label = lv_label_create(display_container);
    lv_obj_set_width(expression_label, 290);
    lv_obj_set_style_text_align(expression_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(expression_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(expression_label, lv_color_hex(0x0B3C5D), 0);
    lv_obj_align(expression_label, LV_ALIGN_BOTTOM_RIGHT, 0, -35);

    display_label = lv_label_create(display_container);
    lv_obj_set_width(display_label, 290);
    lv_obj_set_style_text_align(display_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(display_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(display_label, lv_color_hex(0x0B3C5D), 0);
    lv_obj_align_to(display_label, expression_label, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 5);

    update_display();
}

inline void show() {
    if (calculator_screen) {
        handle_clear_press();
        lv_obj_clear_flag(calculator_screen, LV_OBJ_FLAG_HIDDEN);
        lv_scr_load(calculator_screen);
    }
}

inline void handle() {
    char key = Keyboard::get_key();
    if (key > 0) {
        switch (key) {
            case 'w': handle_digit_press("1"); break;
            case 'e': handle_digit_press("2"); break;
            case 'r': handle_digit_press("3"); break;
            case 's': handle_digit_press("4"); break;
            case 'd': handle_digit_press("5"); break;
            case 'f': handle_digit_press("6"); break;
            case 'z': handle_digit_press("7"); break;
            case 'x': handle_digit_press("8"); break;
            case 'c': handle_digit_press("9"); break;
            case 'h': handle_digit_press("0"); break;

            case 'g': handle_operator_press('/'); break;
            case 'a': handle_operator_press('*'); break;
            case 'o': handle_operator_press('+'); break;
            case 'i': handle_operator_press('-'); break;
            case 'q': handle_operator_press('^'); break;

            case 'm': handle_decimal_press(); break;
            case 'b': handle_factorial_press(); break;

            case '\n':
            case '\r':
                handle_equals_press();
                break;
            case KEY_BACKSPACE:
                handle_backspace_press();
                break;
            case KEY_CLEAR_ENTRY:
                handle_clear_press();
                break;
        }
    }
    lv_timer_handler();
}

} // namespace Calculator
