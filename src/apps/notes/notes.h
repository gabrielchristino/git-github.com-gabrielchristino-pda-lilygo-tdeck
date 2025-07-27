#pragma once
#include <lvgl.h>
#include "input/input.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "utils/utils.h"
#include <ArduinoJson.h>
#include "secrets.h"
#include "Icons/lv_img_sync.c"

// Included from apps.h, which already defines the AppManager.

namespace Notes
{

    // --- Modal UI Variables ---
    static lv_obj_t *task_modal_cont = nullptr;
    static lv_obj_t *task_textarea = nullptr;
    static lv_obj_t *task_save_btn = nullptr;
    static lv_obj_t *task_cancel_btn = nullptr;
    static lv_obj_t *task_delete_btn = nullptr;

    // --- Variables for operation ---
    static int selected_task_index = -1;
    static bool is_editing_task = false;

    // --- UI Variables ---
    static lv_obj_t *notes_screen = nullptr;
    static lv_obj_t *back_btn = nullptr;
    static lv_obj_t *title_label = nullptr;
    static lv_obj_t *tasks_list = nullptr;
    static lv_obj_t *status_label = nullptr;
    static lv_obj_t* no_tasks_img = nullptr; // Ponteiro para a imagem de "sem tarefas"

    // --- Variables for asynchronous operation ---
    static TaskHandle_t fetch_task_handle = NULL;
    static SemaphoreHandle_t data_mutex = NULL;
    // We use a dynamic document to avoid repeated memory allocations in the loop.
    static JsonDocument tasks_doc;
    static volatile bool data_ready_for_ui = false;
    static String fetch_status_message = "";

    // --- Function Prototypes ---
    static void back_button_event_cb(lv_event_t *e);
    static void fetch_tasks();
    static void update_tasks_ui(JsonVariant doc);
    static void refresh_event_cb(lv_event_t *e);
    static void fetch_tasks_task(void *parameter);
    static void add_task_btn_event_cb(lv_event_t *e);
    static void task_save_btn_event_cb(lv_event_t *e);
    static void task_cancel_btn_event_cb(lv_event_t *e);
    static void task_delete_btn_event_cb(lv_event_t *e);
    static void task_list_item_click_event_cb(lv_event_t *e);
    static void show_task_modal(const char *initial_text = nullptr);
    static void close_task_modal();
    static void create_task(const char *title);
    static void update_task(const char *id, const char *title);
    static void delete_task(const char *id);
    /**
 * @brief Callback for the back button. Returns to the main menu.
 */
static void back_button_event_cb(lv_event_t* e) {
    AppManager::show_app(AppManager::APP_MAIN_MENU);
}

    static void task_cancel_btn_event_cb(lv_event_t *e)
    {
        close_task_modal();
    }

    static void task_delete_btn_event_cb(lv_event_t *e)
    {
        if (selected_task_index >= 0 && selected_task_index < (int)tasks_doc["items"].size())
        {
            JsonObject task = tasks_doc["items"][selected_task_index].as<JsonObject>();
            const char *id = task["id"] | "";
            delete_task(id);
            close_task_modal();
            fetch_tasks();
        }
        else
        {
            LV_LOG_ERROR("Invalid selected_task_index for deletion: %d", selected_task_index);
        }
    }

    static void task_list_item_click_event_cb(lv_event_t *e)
    {
        lv_obj_t *target = lv_event_get_target(e);
        // Instead of getting index from parent, get index from the button itself using user data
        intptr_t index = (intptr_t)lv_obj_get_user_data(target);

        // Defensive check for valid index
        if (index < 0 || index >= (intptr_t)tasks_doc["items"].size())
        {
            LV_LOG_ERROR("Invalid task index selected: %d", (int)index);
            return;
        }

        selected_task_index = (int)index;

        // Get task title safely
        JsonObject task = tasks_doc["items"][selected_task_index].as<JsonObject>();
        const char *task_text = task["title"] | "";

        is_editing_task = true;
        show_task_modal(task_text);
    }

    static void show_task_modal(const char *initial_text)
    {
        static lv_group_t *task_modal_group = nullptr;

        if (task_modal_cont)
        {
            lv_obj_del(task_modal_cont);
            task_modal_cont = nullptr;
        }
        task_modal_cont = lv_obj_create(lv_scr_act());
        lv_obj_set_size(task_modal_cont, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(task_modal_cont, lv_color_hex(0xFDF5E6), 0);
        lv_obj_set_style_bg_opa(task_modal_cont, LV_OPA_80, 0);
        lv_obj_set_style_border_width(task_modal_cont, 0, 0);
        lv_obj_set_style_radius(task_modal_cont, 0, 0);
        lv_obj_clear_flag(task_modal_cont, LV_OBJ_FLAG_SCROLLABLE);

        // Close button similar to settings modal
        task_cancel_btn = lv_btn_create(task_modal_cont);
        lv_obj_align(task_cancel_btn, LV_ALIGN_TOP_RIGHT, -5, 5);
        lv_obj_set_size(task_cancel_btn, 40, 40);
        lv_obj_set_style_radius(task_cancel_btn, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(task_cancel_btn, LV_OPA_50, 0);
        lv_obj_set_style_bg_color(task_cancel_btn, lv_color_hex(0x1A5FB4), 0);
        lv_obj_add_event_cb(task_cancel_btn, task_cancel_btn_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *cancel_label = lv_label_create(task_cancel_btn);
        lv_label_set_text(cancel_label, LV_SYMBOL_CLOSE);
        lv_obj_center(cancel_label);

        task_textarea = lv_textarea_create(task_modal_cont);
        lv_textarea_set_one_line(task_textarea, false);
        lv_obj_set_width(task_textarea, lv_pct(80));
        lv_obj_align(task_textarea, LV_ALIGN_CENTER, 0, 0);
        if (initial_text)
        {
            lv_textarea_set_text(task_textarea, initial_text);
        }
        else
        {
            lv_textarea_set_text(task_textarea, "");
        }
        lv_textarea_set_placeholder_text(task_textarea, "Enter task title...");
        lv_obj_add_flag(task_textarea, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(task_textarea, LV_STATE_FOCUSED);
        lv_textarea_set_cursor_pos(task_textarea, 0);

        if (!task_modal_group)
        {
            task_modal_group = lv_group_create();
        }
        lv_group_add_obj(task_modal_group, task_textarea);

        lv_indev_t *indev = lv_indev_get_next(nullptr);
        if (indev)
        {
            lv_indev_set_group(indev, task_modal_group);
        }

        // Save button with icon similar to settings modal
        task_save_btn = lv_btn_create(task_modal_cont);
        lv_obj_set_size(task_save_btn, 40, 40);
        lv_obj_align(task_save_btn, LV_ALIGN_BOTTOM_MID, -25, 5);
        lv_obj_set_style_radius(task_save_btn, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(task_save_btn, LV_OPA_50, 0);
        lv_obj_set_style_bg_color(task_save_btn, lv_color_hex(0x1A5FB4), 0);
        lv_obj_add_event_cb(task_save_btn, task_save_btn_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *save_label = lv_label_create(task_save_btn);
        lv_label_set_text(save_label, LV_SYMBOL_SAVE);
        lv_obj_center(save_label);

        if (is_editing_task)
        {
            task_delete_btn = lv_btn_create(task_modal_cont);
            lv_obj_set_size(task_delete_btn, 40, 40);
            lv_obj_align(task_delete_btn, LV_ALIGN_BOTTOM_MID, 25, 5);
            lv_obj_set_style_radius(task_delete_btn, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_opa(task_delete_btn, LV_OPA_50, 0);
            lv_obj_set_style_bg_color(task_delete_btn, lv_color_hex(0xB00020), 0);
            lv_obj_t *delete_label = lv_label_create(task_delete_btn);
            lv_label_set_text(delete_label, LV_SYMBOL_CLOSE);
            lv_obj_center(delete_label);
            lv_obj_add_event_cb(task_delete_btn, task_delete_btn_event_cb, LV_EVENT_CLICKED, NULL);
        }
    }

    static void close_task_modal()
    {
        if (task_modal_cont)
        {
            lv_obj_del(task_modal_cont);
            task_modal_cont = nullptr;
            task_textarea = nullptr;
            task_save_btn = nullptr;
            task_cancel_btn = nullptr;
            task_delete_btn = nullptr;
        }
    }

     static void create_task(const char *title)
     {
         HTTPClient http;
         http.begin(GOOGLE_SCRIPT_URL_TASKS);
         http.addHeader("Content-Type", "application/json");
         String payload = "{\"title\":\"" + String(title) + "\"}";
         int http_code = http.POST(payload);
         if (http_code != HTTP_CODE_OK && http_code != HTTP_CODE_CREATED)
         {
             LV_LOG_ERROR("Failed to create task, HTTP code: %d", http_code);
         }
         http.end();
     }

     static void update_task(const char *id, const char *title)
     {
         HTTPClient http;
         http.begin(GOOGLE_SCRIPT_URL_TASKS);
         http.addHeader("Content-Type", "application/json");
         String postPayload = "{\"id\":\"" + String(id) + "\", \"title\":\"" + String(title) + "\", \"update\": true}";
         int http_code = http.POST(postPayload);
         if (http_code != HTTP_CODE_OK)
         {
             LV_LOG_ERROR("Failed to update task (POST), HTTP code: %d", http_code);
         }
         http.end();
     }

     static void delete_task(const char *id)
     {
         HTTPClient http;
         http.begin(GOOGLE_SCRIPT_URL_TASKS);
         http.addHeader("Content-Type", "application/json");
        // Use POST with delete flag due to Google Script limitations
        String postPayload = "{\"id\":\"" + String(id) + "\", \"delete\": true}";
        int http_code = http.POST(postPayload);
        if (http_code != HTTP_CODE_OK)
        {
            LV_LOG_ERROR("Failed to delete task (POST), HTTP code: %d", http_code);
        }
        http.end();
     }

    static void add_task_btn_event_cb(lv_event_t *e)
    {
        is_editing_task = false;
        show_task_modal();
    }

    static void task_save_btn_event_cb(lv_event_t *e)
    {
        if (!task_textarea)
            return;

        const char *text = lv_textarea_get_text(task_textarea);
        if (strlen(text) == 0)
        {
            LV_LOG_USER("Empty task title, ignoring save.");
            close_task_modal();
            return;
        }

        if (is_editing_task && selected_task_index >= 0)
        {
            JsonObject task = tasks_doc["items"][selected_task_index].as<JsonObject>();
            const char *id = task["id"] | "";
            update_task(id, text);
        }
        else
        {
            create_task(text);
        }

        close_task_modal();
        fetch_tasks();
    }

    /**
     * @brief Callback to refresh the task list when clicking the screen.
     */
    static void refresh_event_cb(lv_event_t *e)
    {
        // The fetch_tasks function already checks the connection and updates the status
        fetch_tasks();
    }

    /**
     * @brief Updates the UI with the received task list.
     * @param tasks JSON array with the tasks.
     */
    static void update_tasks_ui(JsonVariant doc)
    {
        // Se a imagem "sem tarefas" existir, apague-a antes de atualizar a UI.
        if (no_tasks_img) {
            lv_obj_del(no_tasks_img);
            no_tasks_img = nullptr;
        }
        // Garante que a lista de tarefas esteja visível para o caso de uma atualização
        lv_obj_clear_flag(tasks_list, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clean(tasks_list);

        // Apply styling similar to WiFi list in settings
        lv_obj_set_style_bg_opa(tasks_list, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(tasks_list, 0, LV_PART_MAIN);
        lv_obj_set_style_text_color(tasks_list, lv_color_hex(0x0B3C5D), LV_PART_MAIN);

        // Use auto&& to avoid copying and binding issues
        auto&& tasks_var = doc["items"];
        if (!tasks_var.is<JsonArray>() || tasks_var.as<JsonArray>().size() == 0)
        {
            // Esconde a lista vazia para não interferir
            lv_obj_add_flag(tasks_list, LV_OBJ_FLAG_HIDDEN);

            // Cria a imagem de sincronização no centro da tela principal
            no_tasks_img = lv_img_create(notes_screen);
            lv_img_set_src(no_tasks_img, &lv_img_sync);
            lv_obj_add_flag(no_tasks_img, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_align(no_tasks_img, LV_ALIGN_CENTER, 0, 0); // Centraliza na tela
            lv_obj_add_event_cb(no_tasks_img, refresh_event_cb, LV_EVENT_CLICKED, NULL);

            lv_label_set_text(status_label, ""); // Remove o texto do label de status
            return;
        }

        auto&& tasks = tasks_var.as<JsonArray>();

        for (int i = 0; i < (int)tasks.size(); i++)
        {
            JsonObject task = tasks[i].as<JsonObject>();
            String title = task["title"] | "Untitled";
            title = Utils::sanitizeString(title);
            lv_obj_t *btn = lv_list_add_btn(tasks_list, NULL, title.c_str());
            lv_obj_set_user_data(btn, (void *)(intptr_t)i);
            lv_obj_add_event_cb(btn, task_list_item_click_event_cb, LV_EVENT_CLICKED, NULL);
        }

        // Style each child item in the list to fix background and text color
        uint32_t child_count = lv_obj_get_child_cnt(tasks_list);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t* child = lv_obj_get_child(tasks_list, i);
            lv_obj_set_style_bg_opa(child, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_width(child, 0, LV_PART_MAIN);
            lv_obj_set_style_text_color(child, lv_color_hex(0x0B3C5D), LV_PART_MAIN);
        }
    }

    /**
     * @brief Sets an error message to be displayed in the UI.
     * This function is safe to be called from any task.
     */
    static void set_fetch_error_status(const String &message)
    {
        if (xSemaphoreTake(data_mutex, portMAX_DELAY) == pdTRUE)
        {
            tasks_doc.clear();
            fetch_status_message = message;
            data_ready_for_ui = true;
            xSemaphoreGive(data_mutex);
        }
    }

    /**
     * @brief Processes the JSON payload and stores it for the UI.
     * This function is safe to be called from any task.
     */
    static void process_and_store_payload(const String &payload)
    {
        if (xSemaphoreTake(data_mutex, portMAX_DELAY) == pdTRUE)
        {
            tasks_doc.clear();
            DeserializationError error = deserializeJson(tasks_doc, payload);
            if (error)
            {
                LV_LOG_ERROR("deserializeJson() failed: %s", error.c_str());
                fetch_status_message = "Error parsing JSON.";
            }
            else
            {
                fetch_status_message = ""; // Success
            }
            data_ready_for_ui = true;
            xSemaphoreGive(data_mutex);
        }
    }

    /**
     * @brief Background task to fetch tasks from the network.
     */
    static void fetch_tasks_task(void *parameter)
    {
        HTTPClient http;
        String url = GOOGLE_SCRIPT_URL_TASKS;

        http.begin(url);
        http.setConnectTimeout(5000);
        http.setTimeout(5000);

        // It is necessary to inform the client which headers to collect.
        const char *headerKeys[] = {"location"};
        http.collectHeaders(headerKeys, 1);

        int http_code = http.GET();

        if (http_code == HTTP_CODE_MOVED_PERMANENTLY || http_code == HTTP_CODE_FOUND)
        {
            url = http.header("location");
            LV_LOG_USER("Redirected to: %s", url.c_str());
            http.end(); // Ends the first request

            if (url.length() > 0)
            {
                http.begin(url); // Starts the second request
                http.addHeader("User-Agent", "Mozilla/5.0 (compatible; ESP32)");
                http_code = http.GET();
                LV_LOG_USER("Second GET HTTP code: %d", http_code);
            }
        }

        if (http_code == HTTP_CODE_OK)
        {
            String payload = http.getString();
            LV_LOG_USER("Payload received: %s", payload.c_str());
            process_and_store_payload(payload);
        }
        else
        {
            String error_msg = "HTTP Error: " + String(http_code);
            LV_LOG_ERROR("HTTP GET failed, code: %d, message: %s", http_code, http.errorToString(http_code).c_str());
            set_fetch_error_status(error_msg);
        }

        http.end();

        // The task finished, so we clear the handle and delete the task.
        fetch_task_handle = NULL;
        vTaskDelete(NULL);
    }

    /**
     * @brief Starts the task fetching process in a background task.
     */
    static void fetch_tasks()
    {
        if (fetch_task_handle != NULL)
        {
            LV_LOG_USER("Task fetch already in progress.");
            return;
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            lv_label_set_text(status_label, "Updating tasks...");
            // Creates the task that will run on the other ESP32 core
            xTaskCreate(
                fetch_tasks_task,   /* Function that implements the task */
                "FetchTasks",       /* Task name */
                8192,               /* Stack size in words */
                NULL,               /* Task input parameter */
                1,                  /* Task priority */
                &fetch_task_handle  /* Task handle for control */
            );

            // Delay to reduce memory pressure before weather update
            vTaskDelay(pdMS_TO_TICKS(2));
        }
        else
        {
            lv_label_set_text(status_label, "No Wi-Fi connection");
        }
    }

    /**
     * @brief Initializes the notes screen and its elements.
     */
    inline void init()
    {
        // Creates the mutex to protect access to shared data between tasks
        data_mutex = xSemaphoreCreateMutex();

        notes_screen = lv_obj_create(NULL);
        lv_obj_add_flag(notes_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(notes_screen, 320, 240);
        lv_obj_set_style_bg_color(notes_screen, lv_color_hex(0xFDF5E6), LV_PART_MAIN);

        // Adds the click event to the screen to allow refresh
        lv_obj_add_flag(notes_screen, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(notes_screen, refresh_event_cb, LV_EVENT_CLICKED, NULL);

        // --- Back Button ---
        back_btn = lv_btn_create(notes_screen);
        lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
        lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x1A5FB4), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(back_btn, LV_OPA_50, LV_PART_MAIN);
        lv_obj_set_style_radius(back_btn, 12, LV_PART_MAIN);
        lv_obj_set_style_border_width(back_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(back_btn, 0, LV_PART_MAIN);

        lv_obj_t *back_label = lv_label_create(back_btn);
        lv_label_set_text(back_label, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(back_label, &lv_font_montserrat_16, 0);
        lv_obj_center(back_label);

        // --- Screen Title ---
        title_label = lv_label_create(notes_screen);
        lv_label_set_text(title_label, "Notes");
        lv_obj_set_style_text_font(title_label, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(title_label, lv_color_hex(0x0B3C5D), 0);
        lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

        // --- Task List ---
        tasks_list = lv_list_create(notes_screen);
        lv_obj_set_size(tasks_list, 300, 180);
        lv_obj_align(tasks_list, LV_ALIGN_CENTER, 0, 20);
        lv_obj_set_style_bg_opa(tasks_list, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(tasks_list, 0, 0);

        // --- Add Task Button ---
        lv_obj_t *add_task_btn = lv_btn_create(notes_screen);
        lv_obj_set_size(add_task_btn, 40, 40);
        lv_obj_align(add_task_btn, LV_ALIGN_BOTTOM_LEFT, 10, -10);
        lv_obj_set_style_bg_color(add_task_btn, lv_color_hex(0x1A5FB4), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(add_task_btn, LV_OPA_70, LV_PART_MAIN);
        lv_obj_set_style_radius(add_task_btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_border_width(add_task_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(add_task_btn, 0, LV_PART_MAIN);

        lv_obj_t *add_task_label = lv_label_create(add_task_btn);
        lv_label_set_text(add_task_label, "+");
        lv_obj_set_style_text_color(add_task_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(add_task_label, &lv_font_montserrat_28, 0);
        lv_obj_center(add_task_label);

        lv_obj_add_event_cb(add_task_btn, add_task_btn_event_cb, LV_EVENT_CLICKED, NULL);

        // --- Status Label ---
        status_label = lv_label_create(notes_screen);
        lv_label_set_text(status_label, "");
        lv_obj_set_style_text_font(status_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x324A5F), 0);
        lv_obj_align(status_label, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    }

    /**
     * @brief Shows the notes screen.
     */
    inline void show()
    {
        if (notes_screen)
        {
            fetch_tasks();
            lv_obj_clear_flag(notes_screen, LV_OBJ_FLAG_HIDDEN);
            lv_scr_load(notes_screen);
        }
    }

    /**
     * @brief Manages user input and updates the interface.
     */
    inline void handle()
    {
        // Handle keyboard input for task modal
        if (task_modal_cont && task_textarea)
        {
            char key = Keyboard::get_key();
            if (key > 0)
            {
                switch (key)
                {
                case '\n':
                case '\r': // Enter key - save task
                    task_save_btn_event_cb(nullptr);
                    break;
                case 8: // Backspace
                    lv_textarea_del_char(task_textarea);
                    break;
                case 2: // Clear entry (custom key)
                    task_cancel_btn_event_cb(nullptr);
                    break;
                default:
                    if (isprint(key))
                    {
                        lv_textarea_add_char(task_textarea, key);
                    }
                    break;
                }
            }
        }

        // Checks if the background task signaled that there is new data
        if (data_ready_for_ui)
        {
            // Locks access to data to safely read
            if (xSemaphoreTake(data_mutex, (TickType_t)10) == pdTRUE)
            {
                if (!fetch_status_message.isEmpty())
                {
                    lv_label_set_text(status_label, fetch_status_message.c_str());
                    lv_obj_clean(tasks_list); // Clears the list in case of error
                }
                else
                {
                    update_tasks_ui(tasks_doc.as<JsonObject>());
                    lv_label_set_text(status_label, ""); // Clear status message on success
                }
                data_ready_for_ui = false; // Reset the flag
                xSemaphoreGive(data_mutex); // Release access
            }
        }
        lv_timer_handler();
    }

} // namespace Notes
