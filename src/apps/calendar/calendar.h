#pragma once
#include <lvgl.h>
#include "input/input.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "utils/utils.h"
#include <ArduinoJson.h>
#include "secrets.h"

namespace Calendar
{
    // --- Modal UI Variables ---
    static lv_obj_t *event_modal_cont = nullptr;
    static lv_obj_t *event_title_textarea = nullptr;
    static lv_obj_t *event_start_textarea = nullptr;
    static lv_obj_t *event_end_textarea = nullptr;
    static lv_obj_t *event_description_textarea = nullptr;
    static lv_obj_t *event_location_textarea = nullptr;
    static lv_obj_t *event_save_btn = nullptr;
    static lv_obj_t *event_cancel_btn = nullptr;
    static lv_obj_t *event_delete_btn = nullptr;

    // --- Variables for operation ---
    static int selected_event_index = -1;
    static bool is_editing_event = false;

    // --- UI Variables ---
    static lv_obj_t *calendar_screen = nullptr;
    static lv_obj_t *back_btn = nullptr;
    static lv_obj_t *title_label = nullptr;
    static lv_obj_t *events_list = nullptr;
    static lv_obj_t *status_label = nullptr;

    // --- Variables for async operation ---
    static TaskHandle_t fetch_event_task_handle = NULL;
    static SemaphoreHandle_t data_mutex = NULL;
    static DynamicJsonDocument events_doc(8192);
    static volatile bool data_ready_for_ui = false;
    static String fetch_status_message = "";

    // --- Function Prototypes ---
    static void back_button_event_cb(lv_event_t *e);
    static void fetch_events();
    static void update_events_ui(JsonVariant doc);
    static void refresh_event_cb(lv_event_t *e);
    static void fetch_events_task(void *parameter);
    static void add_event_btn_event_cb(lv_event_t *e);
    static void event_save_btn_event_cb(lv_event_t *e);
    static void event_cancel_btn_event_cb(lv_event_t *e);
    static void event_delete_btn_event_cb(lv_event_t *e);
    static void event_list_item_click_event_cb(lv_event_t *e);
    static void show_event_modal(const char *title = nullptr, const char *start = nullptr, const char *end = nullptr, const char *description = nullptr, const char *location = nullptr);
    static void close_event_modal();
    static void create_event(const char *title, const char *start, const char *end, const char *description, const char *location);
    static void update_event(const char *id, const char *title, const char *start, const char *end, const char *description, const char *location);
    static void delete_event(const char *id);

    static void back_button_event_cb(lv_event_t *e)
    {
        AppManager::show_app(AppManager::APP_MAIN_MENU);
    }

    static void event_cancel_btn_event_cb(lv_event_t *e)
    {
        close_event_modal();
    }

    static void event_delete_btn_event_cb(lv_event_t *e)
    {
        if (selected_event_index >= 0 && selected_event_index < (int)events_doc["items"].size())
        {
            JsonObject event = events_doc["items"][selected_event_index].as<JsonObject>();
            const char *id = event["id"] | "";
            delete_event(id);
            close_event_modal();
            fetch_events();
        }
        else
        {
            LV_LOG_ERROR("Invalid selected_event_index for deletion: %d", selected_event_index);
        }
    }

    static void event_list_item_click_event_cb(lv_event_t *e)
    {
        lv_obj_t *target = lv_event_get_target(e);
        intptr_t index = (intptr_t)lv_obj_get_user_data(target);

        if (index < 0 || index >= (intptr_t)events_doc["items"].size())
        {
            LV_LOG_ERROR("Invalid event index selected: %d", (int)index);
            return;
        }

        selected_event_index = (int)index;

        JsonObject event = events_doc["items"][selected_event_index].as<JsonObject>();
        const char *title = event["title"] | "";
        const char *start = event["startTime"] | "";
        const char *end = event["endTime"] | "";
        const char *description = event["description"] | "";
        const char *location = event["location"] | "";

        is_editing_event = true;
        show_event_modal(title, start, end, description, location);
    }

    static void show_event_modal(const char *title, const char *start, const char *end, const char *description, const char *location)
    {
        static lv_group_t *event_modal_group = nullptr;

        if (event_modal_cont)
        {
            lv_obj_del(event_modal_cont);
            event_modal_cont = nullptr;
        }
        event_modal_cont = lv_obj_create(lv_scr_act());
        lv_obj_set_size(event_modal_cont, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(event_modal_cont, lv_color_hex(0xFDF5E6), 0);
        lv_obj_set_style_bg_opa(event_modal_cont, LV_OPA_80, 0);
        lv_obj_set_style_border_width(event_modal_cont, 0, 0);
        lv_obj_set_style_radius(event_modal_cont, 0, 0);
        lv_obj_clear_flag(event_modal_cont, LV_OBJ_FLAG_SCROLLABLE);

        event_cancel_btn = lv_btn_create(event_modal_cont);
        lv_obj_align(event_cancel_btn, LV_ALIGN_TOP_RIGHT, -5, 5);
        lv_obj_set_size(event_cancel_btn, 40, 40);
        lv_obj_set_style_radius(event_cancel_btn, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(event_cancel_btn, LV_OPA_50, 0);
        lv_obj_set_style_bg_color(event_cancel_btn, lv_color_hex(0x1A5FB4), 0);
        lv_obj_add_event_cb(event_cancel_btn, event_cancel_btn_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *cancel_label = lv_label_create(event_cancel_btn);
        lv_label_set_text(cancel_label, LV_SYMBOL_CLOSE);
        lv_obj_center(cancel_label);

        event_title_textarea = lv_textarea_create(event_modal_cont);
        lv_textarea_set_one_line(event_title_textarea, true);
        lv_obj_set_width(event_title_textarea, lv_pct(80));
        lv_obj_align(event_title_textarea, LV_ALIGN_TOP_MID, 0, 40);
        lv_textarea_set_placeholder_text(event_title_textarea, "Title");
        if (title)
            lv_textarea_set_text(event_title_textarea, title);
        else
            lv_textarea_set_text(event_title_textarea, "");

        event_start_textarea = lv_textarea_create(event_modal_cont);
        lv_textarea_set_one_line(event_start_textarea, true);
        lv_obj_set_width(event_start_textarea, lv_pct(80));
        lv_obj_align(event_start_textarea, LV_ALIGN_TOP_MID, 0, 80);
        lv_textarea_set_placeholder_text(event_start_textarea, "Start Time (ISO8601)");
        if (start)
            lv_textarea_set_text(event_start_textarea, start);
        else
            lv_textarea_set_text(event_start_textarea, "");

        event_end_textarea = lv_textarea_create(event_modal_cont);
        lv_textarea_set_one_line(event_end_textarea, true);
        lv_obj_set_width(event_end_textarea, lv_pct(80));
        lv_obj_align(event_end_textarea, LV_ALIGN_TOP_MID, 0, 120);
        lv_textarea_set_placeholder_text(event_end_textarea, "End Time (ISO8601)");
        if (end)
            lv_textarea_set_text(event_end_textarea, end);
        else
            lv_textarea_set_text(event_end_textarea, "");

        event_description_textarea = lv_textarea_create(event_modal_cont);
        lv_textarea_set_one_line(event_description_textarea, false);
        lv_obj_set_width(event_description_textarea, lv_pct(80));
        lv_obj_align(event_description_textarea, LV_ALIGN_TOP_MID, 0, 160);
        lv_textarea_set_placeholder_text(event_description_textarea, "Description");
        if (description)
            lv_textarea_set_text(event_description_textarea, description);
        else
            lv_textarea_set_text(event_description_textarea, "");

        event_location_textarea = lv_textarea_create(event_modal_cont);
        lv_textarea_set_one_line(event_location_textarea, true);
        lv_obj_set_width(event_location_textarea, lv_pct(80));
        lv_obj_align(event_location_textarea, LV_ALIGN_TOP_MID, 0, 200);
        lv_textarea_set_placeholder_text(event_location_textarea, "Location");
        if (location)
            lv_textarea_set_text(event_location_textarea, location);
        else
            lv_textarea_set_text(event_location_textarea, "");

        if (!event_modal_group)
        {
            event_modal_group = lv_group_create();
        }
        lv_group_add_obj(event_modal_group, event_title_textarea);
        lv_group_add_obj(event_modal_group, event_start_textarea);
        lv_group_add_obj(event_modal_group, event_end_textarea);
        lv_group_add_obj(event_modal_group, event_description_textarea);
        lv_group_add_obj(event_modal_group, event_location_textarea);

        lv_indev_t *indev = lv_indev_get_next(nullptr);
        if (indev)
        {
            lv_indev_set_group(indev, event_modal_group);
        }

        event_save_btn = lv_btn_create(event_modal_cont);
        lv_obj_set_size(event_save_btn, 40, 40);
        lv_obj_align(event_save_btn, LV_ALIGN_BOTTOM_MID, -25, 5);
        lv_obj_set_style_radius(event_save_btn, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(event_save_btn, LV_OPA_50, 0);
        lv_obj_set_style_bg_color(event_save_btn, lv_color_hex(0x1A5FB4), 0);
        lv_obj_add_event_cb(event_save_btn, event_save_btn_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *save_label = lv_label_create(event_save_btn);
        lv_label_set_text(save_label, LV_SYMBOL_SAVE);
        lv_obj_center(save_label);

        if (is_editing_event)
        {
            event_delete_btn = lv_btn_create(event_modal_cont);
            lv_obj_set_size(event_delete_btn, 40, 40);
            lv_obj_align(event_delete_btn, LV_ALIGN_BOTTOM_MID, 25, 5);
            lv_obj_set_style_radius(event_delete_btn, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_opa(event_delete_btn, LV_OPA_50, 0);
            lv_obj_set_style_bg_color(event_delete_btn, lv_color_hex(0xB00020), 0);
            lv_obj_t *delete_label = lv_label_create(event_delete_btn);
            lv_label_set_text(delete_label, LV_SYMBOL_CLOSE);
            lv_obj_center(delete_label);
            lv_obj_add_event_cb(event_delete_btn, event_delete_btn_event_cb, LV_EVENT_CLICKED, NULL);
        }
    }

    static void close_event_modal()
    {
        if (event_modal_cont)
        {
            lv_obj_del(event_modal_cont);
            event_modal_cont = nullptr;
            event_title_textarea = nullptr;
            event_start_textarea = nullptr;
            event_end_textarea = nullptr;
            event_description_textarea = nullptr;
            event_location_textarea = nullptr;
            event_save_btn = nullptr;
            event_cancel_btn = nullptr;
            event_delete_btn = nullptr;
        }
    }

    static void create_event(const char *title, const char *start, const char *end, const char *description, const char *location)
    {
        HTTPClient http;
        http.begin(GOOGLE_SCRIPT_URL_TASKS);
        http.addHeader("Content-Type", "application/json");
        String payload = "{\"title\":\"" + String(title) + "\",\"startTime\":\"" + String(start) + "\",\"endTime\":\"" + String(end) + "\",\"description\":\"" + String(description) + "\",\"location\":\"" + String(location) + "\"}";
        int http_code = http.POST(payload);
        if (http_code != HTTP_CODE_OK && http_code != HTTP_CODE_CREATED)
        {
            LV_LOG_ERROR("Failed to create event, HTTP code: %d", http_code);
        }
        http.end();
    }

    static void update_event(const char *id, const char *title, const char *start, const char *end, const char *description, const char *location)
    {
        HTTPClient http;
        http.begin(GOOGLE_SCRIPT_URL_TASKS);
        http.addHeader("Content-Type", "application/json");
        String postPayload = "{\"id\":\"" + String(id) + "\", \"title\":\"" + String(title) + "\", \"startTime\":\"" + String(start) + "\", \"endTime\":\"" + String(end) + "\", \"description\":\"" + String(description) + "\", \"location\":\"" + String(location) + "\", \"update\": true}";
        int http_code = http.POST(postPayload);
        if (http_code != HTTP_CODE_OK)
        {
            LV_LOG_ERROR("Failed to update event (POST), HTTP code: %d", http_code);
        }
        http.end();
    }

    static void delete_event(const char *id)
    {
        HTTPClient http;
        http.begin(GOOGLE_SCRIPT_URL_TASKS);
        http.addHeader("Content-Type", "application/json");
        String postPayload = "{\"id\":\"" + String(id) + "\", \"delete\": true}";
        int http_code = http.POST(postPayload);
        if (http_code != HTTP_CODE_OK)
        {
            LV_LOG_ERROR("Failed to delete event (POST), HTTP code: %d", http_code);
        }
        http.end();
    }

    static void add_event_btn_event_cb(lv_event_t *e)
    {
        is_editing_event = false;
        show_event_modal();
    }

    static void event_save_btn_event_cb(lv_event_t *e)
    {
        if (!event_title_textarea || !event_start_textarea || !event_end_textarea)
            return;

        const char *title = lv_textarea_get_text(event_title_textarea);
        const char *start = lv_textarea_get_text(event_start_textarea);
        const char *end = lv_textarea_get_text(event_end_textarea);
        const char *description = lv_textarea_get_text(event_description_textarea);
        const char *location = lv_textarea_get_text(event_location_textarea);

        if (strlen(title) == 0 || strlen(start) == 0 || strlen(end) == 0)
        {
            LV_LOG_USER("Missing required event fields, ignoring save.");
            close_event_modal();
            return;
        }

        if (is_editing_event && selected_event_index >= 0)
        {
            JsonObject event = events_doc["items"][selected_event_index].as<JsonObject>();
            const char *id = event["id"] | "";
            update_event(id, title, start, end, description, location);
        }
        else
        {
            create_event(title, start, end, description, location);
        }

        close_event_modal();
        fetch_events();
    }

    static void refresh_event_cb(lv_event_t *e)
    {
        fetch_events();
    }

    static void update_events_ui(JsonVariant doc)
    {
        lv_obj_clean(events_list);

        lv_obj_set_style_bg_opa(events_list, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(events_list, 0, LV_PART_MAIN);
        lv_obj_set_style_text_color(events_list, lv_color_hex(0x0B3C5D), LV_PART_MAIN);

        auto&& events_var = doc["items"];
        if (!events_var.is<JsonArray>())
        {
            lv_list_add_text(events_list, "Nenhum evento encontrado.");
            lv_label_set_text(status_label, "Nenhum evento.");
            return;
        }

        auto&& events = events_var.as<JsonArray>();

        if (events.size() == 0)
        {
            lv_list_add_text(events_list, "Nenhum evento encontrado.");
            lv_label_set_text(status_label, "Nenhum evento.");
            return;
        }

        for (int i = 0; i < (int)events.size(); i++)
        {
            JsonObject event = events[i].as<JsonObject>();
            String title = event["title"] | "Sem título";
            title = Utils::sanitizeString(title);
            lv_obj_t *btn = lv_list_add_btn(events_list, NULL, title.c_str());
            lv_obj_set_user_data(btn, (void *)(intptr_t)i);
            lv_obj_add_event_cb(btn, event_list_item_click_event_cb, LV_EVENT_CLICKED, NULL);
        }

        uint32_t child_count = lv_obj_get_child_cnt(events_list);
        for (uint32_t i = 0; i < child_count; i++)
        {
            lv_obj_t *child = lv_obj_get_child(events_list, i);
            lv_obj_set_style_bg_opa(child, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_width(child, 0, LV_PART_MAIN);
            lv_obj_set_style_text_color(child, lv_color_hex(0x0B3C5D), LV_PART_MAIN);
        }
    }

    static void set_fetch_error_status(const String &message)
    {
        if (xSemaphoreTake(data_mutex, portMAX_DELAY) == pdTRUE)
        {
            events_doc.clear();
            fetch_status_message = message;
            data_ready_for_ui = true;
            xSemaphoreGive(data_mutex);
        }
    }

    static void process_and_store_payload(const String &payload)
    {
        if (xSemaphoreTake(data_mutex, portMAX_DELAY) == pdTRUE)
        {
            events_doc.clear();
            DeserializationError error = deserializeJson(events_doc, payload);
            if (error)
            {
                LV_LOG_ERROR("deserializeJson() failed: %s", error.c_str());
                fetch_status_message = "Erro ao analisar JSON.";
            }
            else
            {
                fetch_status_message = "";
            }
            data_ready_for_ui = true;
            xSemaphoreGive(data_mutex);
        }
    }

    static void fetch_events_task(void *parameter)
    {
        HTTPClient http;
        String url = GOOGLE_SCRIPT_URL_TASKS;

        http.begin(url);
        http.setConnectTimeout(5000);
        http.setTimeout(5000);

        const char *headerKeys[] = {"location"};
        http.collectHeaders(headerKeys, 1);

        int http_code = http.GET();

        if (http_code == HTTP_CODE_MOVED_PERMANENTLY || http_code == HTTP_CODE_FOUND)
        {
            url = http.header("location");
            LV_LOG_USER("Redirected to: %s", url.c_str());
            http.end();

            if (url.length() > 0)
            {
                http.begin(url);
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
            String error_msg = "HTTP error: " + String(http_code);
            LV_LOG_ERROR("HTTP GET failed, code: %d, message: %s", http_code, http.errorToString(http_code).c_str());
            set_fetch_error_status(error_msg);
        }

        http.end();

        fetch_event_task_handle = NULL;
        vTaskDelete(NULL);
    }

    static void fetch_events()
    {
        if (fetch_event_task_handle != NULL)
        {
            LV_LOG_USER("Event fetch already in progress.");
            return;
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            lv_label_set_text(status_label, "Updating events...");
            xTaskCreate(
                fetch_events_task,
                "FetchEvents",
                8192,
                NULL,
                1,
                &fetch_event_task_handle);

            vTaskDelay(pdMS_TO_TICKS(2));
        }
        else
        {
            lv_label_set_text(status_label, "No Wi-Fi connection");
        }
    }

    inline void init()
    {
        data_mutex = xSemaphoreCreateMutex();

        calendar_screen = lv_obj_create(NULL);
        lv_obj_add_flag(calendar_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(calendar_screen, 320, 240);
        lv_obj_set_style_bg_color(calendar_screen, lv_color_hex(0xFDF5E6), LV_PART_MAIN);

        lv_obj_add_flag(calendar_screen, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(calendar_screen, refresh_event_cb, LV_EVENT_CLICKED, NULL);

        back_btn = lv_btn_create(calendar_screen);
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

        title_label = lv_label_create(calendar_screen);
        lv_label_set_text(title_label, "Google Calendar");
        lv_obj_set_style_text_font(title_label, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(title_label, lv_color_hex(0x0B3C5D), 0);
        lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

        events_list = lv_list_create(calendar_screen);
        lv_obj_set_size(events_list, 300, 180);
        lv_obj_align(events_list, LV_ALIGN_CENTER, 0, 20);
        lv_obj_set_style_bg_opa(events_list, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(events_list, 0, 0);

        lv_obj_t *add_event_btn = lv_btn_create(calendar_screen);
        lv_obj_set_size(add_event_btn, 40, 40);
        lv_obj_align(add_event_btn, LV_ALIGN_BOTTOM_LEFT, 10, -10);
        lv_obj_set_style_bg_color(add_event_btn, lv_color_hex(0x1A5FB4), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(add_event_btn, LV_OPA_70, LV_PART_MAIN);
        lv_obj_set_style_radius(add_event_btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_border_width(add_event_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(add_event_btn, 0, LV_PART_MAIN);

        lv_obj_t *add_event_label = lv_label_create(add_event_btn);
        lv_label_set_text(add_event_label, "+");
        lv_obj_set_style_text_color(add_event_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(add_event_label, &lv_font_montserrat_28, 0);
        lv_obj_center(add_event_label);

        lv_obj_add_event_cb(add_event_btn, add_event_btn_event_cb, LV_EVENT_CLICKED, NULL);

        status_label = lv_label_create(calendar_screen);
        lv_label_set_text(status_label, "");
        lv_obj_set_style_text_font(status_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x324A5F), 0);
        lv_obj_align(status_label, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    }

    inline void show()
    {
        if (calendar_screen)
        {
            fetch_events();
            lv_obj_clear_flag(calendar_screen, LV_OBJ_FLAG_HIDDEN);
            lv_scr_load(calendar_screen);
        }
    }

    inline void handle()
    {
        if (event_modal_cont && event_title_textarea)
        {
            char key = Keyboard::get_key();
            if (key > 0)
            {
                switch (key)
                {
                case '\n':
                case '\r':
                    event_save_btn_event_cb(nullptr);
                    break;
                case 8:
                    lv_textarea_del_char(event_title_textarea);
                    break;
                case 2:
                    event_cancel_btn_event_cb(nullptr);
                    break;
                default:
                    if (isprint(key))
                    {
                        lv_textarea_add_char(event_title_textarea, key);
                    }
                    break;
                }
            }
        }

        if (data_ready_for_ui)
        {
            if (xSemaphoreTake(data_mutex, (TickType_t)10) == pdTRUE)
            {
                if (!fetch_status_message.isEmpty())
                {
                    lv_label_set_text(status_label, fetch_status_message.c_str());
                    lv_obj_clean(events_list);
                }
                else
                {
                    update_events_ui(events_doc.as<JsonObject>());
                    lv_label_set_text(status_label, "");
                }
                data_ready_for_ui = false;
                xSemaphoreGive(data_mutex);
            }
        }
        lv_timer_handler();
    }
} // namespace Calendar
