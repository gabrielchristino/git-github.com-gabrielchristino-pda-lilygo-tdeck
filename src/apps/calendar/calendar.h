#pragma once
#include "utils/utils.h"
#include <lvgl.h>
#include "input/input.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include "lvgl.h"

namespace Utils
{
    void initTimeSync();
    struct tm getCurrentTime();
}

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

    // --- New UI Variables for calendar grid and month navigation ---
    static lv_obj_t *calendar_grid_cont = nullptr;
    static lv_obj_t *month_label = nullptr;
    static lv_obj_t *prev_month_btn = nullptr;
    static lv_obj_t *next_month_btn = nullptr;

    // --- Variables for current displayed month and year ---
    static int current_year = 0;
    static int current_month = 0; // 1-12

    // --- Variables for async operation ---
    static TaskHandle_t fetch_event_task_handle = NULL;
    static SemaphoreHandle_t data_mutex = NULL;
    static JsonDocument events_doc;
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

    // --- New function prototypes for calendar grid and month navigation ---
    static void build_calendar_grid();
    static void update_calendar_grid();
    static void prev_month_btn_event_cb(lv_event_t *e);
    static void next_month_btn_event_cb(lv_event_t *e);

    static void back_button_event_cb(lv_event_t *e)
    {
        if (lv_obj_has_flag(events_list, LV_OBJ_FLAG_HIDDEN))
        {
            AppManager::show_app(AppManager::APP_MAIN_MENU);
        }
        else
        {
            // If events list is visible, hide it and show calendar grid
            lv_obj_clear_flag(calendar_grid_cont, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(events_list, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(status_label, "");
        }
    }

    static void build_calendar_grid()
    {
        if (calendar_grid_cont)
        {
            lv_obj_del(calendar_grid_cont);
            calendar_grid_cont = nullptr;
        }

        calendar_grid_cont = lv_obj_create(calendar_screen);
        lv_obj_set_size(calendar_grid_cont, 300, 180);
        lv_obj_align(calendar_grid_cont, LV_ALIGN_TOP_MID, 0, 50);
        lv_obj_set_style_bg_color(calendar_grid_cont, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_width(calendar_grid_cont, 1, 0);
        lv_obj_set_style_border_color(calendar_grid_cont, lv_color_hex(0xAAAAAA), 0);
        lv_obj_set_style_pad_all(calendar_grid_cont, 5, 0);

        // Month label
        if (!month_label)
        {
            month_label = lv_label_create(calendar_grid_cont);
            lv_obj_set_style_text_font(month_label, &lv_font_montserrat_20, 0);
            lv_obj_align(month_label, LV_ALIGN_TOP_MID, 0, 0);
        }

        // Prev month button
        if (!prev_month_btn)
        {
            prev_month_btn = lv_btn_create(calendar_grid_cont);
            lv_obj_set_size(prev_month_btn, 30, 30);
            lv_obj_align(prev_month_btn, LV_ALIGN_TOP_LEFT, 0, 0);
            lv_obj_t *label = lv_label_create(prev_month_btn);
            lv_label_set_text(label, "<");
            lv_obj_center(label);
            lv_obj_add_event_cb(prev_month_btn, prev_month_btn_event_cb, LV_EVENT_CLICKED, NULL);
        }

        // Next month button
        if (!next_month_btn)
        {
            next_month_btn = lv_btn_create(calendar_grid_cont);
            lv_obj_set_size(next_month_btn, 30, 30);
            lv_obj_align(next_month_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
            lv_obj_t *label = lv_label_create(next_month_btn);
            lv_label_set_text(label, ">");
            lv_obj_center(label);
            lv_obj_add_event_cb(next_month_btn, next_month_btn_event_cb, LV_EVENT_CLICKED, NULL);
        }
    }

    static void prev_month_btn_event_cb(lv_event_t *e)
    {
        current_month--;
        if (current_month < 1)
        {
            current_month = 12;
            current_year--;
        }
        update_calendar_grid();
    }

    static void next_month_btn_event_cb(lv_event_t *e)
    {
        current_month++;
        if (current_month > 12)
        {
            current_month = 1;
            current_year++;
        }
        update_calendar_grid();
    }

    static void day_btn_event_cb(lv_event_t *e)
    {
        lv_obj_t *btn = lv_event_get_target(e);
        int day = (int)(intptr_t)lv_obj_get_user_data(btn);
        // Check if there are events on this day
        bool has_event = false;
        if (xSemaphoreTake(data_mutex, portMAX_DELAY) == pdTRUE)
        {
            auto items = events_doc["items"].as<JsonArray>();
            for (JsonObject event : items)
            {
                const char *start = event["startTime"] | "";
                if (strlen(start) > 0)
                {
                    struct tm event_time = {0};
                    strptime(start, "%Y-%m-%dT%H:%M:%S", &event_time);
                    if (event_time.tm_year + 1900 == current_year && event_time.tm_mon + 1 == current_month && event_time.tm_mday == day)
                    {
                        has_event = true;
                        break;
                    }
                }
            }
            xSemaphoreGive(data_mutex);
        }
        if (has_event)
        {
            // Filter events list to show events for this day
            // For simplicity, just update the events_list UI with events of this day
            lv_obj_clean(events_list);
            lv_obj_add_flag(calendar_grid_cont, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(events_list, LV_OBJ_FLAG_HIDDEN);
            if (xSemaphoreTake(data_mutex, portMAX_DELAY) == pdTRUE)
            {
                auto items = events_doc["items"].as<JsonArray>();
                for (int i = 0; i < (int)items.size(); i++)
                {
                    JsonObject event = items[i].as<JsonObject>();
                    const char *start = event["startTime"] | "";
                    if (strlen(start) > 0)
                    {
                        struct tm event_time = {0};
                        strptime(start, "%Y-%m-%dT%H:%M:%S", &event_time);
                        if (event_time.tm_year + 1900 == current_year && event_time.tm_mon + 1 == current_month && event_time.tm_mday == day)
                        {
                            String title = event["title"] | "Untitled";
                            title = Utils::sanitizeString(title);
                            lv_obj_t *btn = lv_list_add_btn(events_list, NULL, title.c_str());
                            lv_obj_set_user_data(btn, (void *)(intptr_t)i);
                            lv_obj_add_event_cb(btn, event_list_item_click_event_cb, LV_EVENT_CLICKED, NULL);
                        }
                    }
                }
                xSemaphoreGive(data_mutex);
            }
            lv_label_set_text(status_label, "Filtered events for selected day");

            lv_obj_set_style_bg_opa(events_list, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_width(events_list, 0, LV_PART_MAIN);
            lv_obj_set_style_text_color(events_list, lv_color_hex(0x0B3C5D), LV_PART_MAIN);

            // Style each child item in the list to fix background and text color
            uint32_t child_count = lv_obj_get_child_cnt(events_list);
            for (uint32_t i = 0; i < child_count; i++)
            {
                lv_obj_t *child = lv_obj_get_child(events_list, i);
                lv_obj_set_style_bg_opa(child, LV_OPA_TRANSP, LV_PART_MAIN);
                lv_obj_set_style_border_width(child, 0, LV_PART_MAIN);
                lv_obj_set_style_text_color(child, lv_color_hex(0x0B3C5D), LV_PART_MAIN);
            }
        }
        else
        {
            lv_obj_clear_flag(calendar_grid_cont, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(events_list, LV_OBJ_FLAG_HIDDEN);
            // Open modal for new event with date pre-filled and default times 8am-9am
            char start_time[20];
            char end_time[20];
            snprintf(start_time, sizeof(start_time), "%04d-%02d-%02dT08:00:00", current_year, current_month, day);
            snprintf(end_time, sizeof(end_time), "%04d-%02d-%02dT09:00:00", current_year, current_month, day);
            is_editing_event = false;
            show_event_modal(nullptr, start_time, end_time, nullptr, nullptr);
        }
    }

    static void update_calendar_grid()
    {
        if (!calendar_grid_cont)
            return;

        // Update month label text
        static const char *month_names[12] = {"January", "February", "March", "April", "May", "June",
                                              "July", "August", "September", "October", "November", "December"};
        char month_year_str[32];
        snprintf(month_year_str, sizeof(month_year_str), "%s %d", month_names[current_month - 1], current_year);
        lv_label_set_text(month_label, month_year_str);

        // Remove existing day buttons and labels except month label and nav buttons
        uint32_t child_count = lv_obj_get_child_cnt(calendar_grid_cont);
        for (int i = (int)child_count - 1; i >= 0; i--)
        {
            lv_obj_t *child = lv_obj_get_child(calendar_grid_cont, i);
            if (child != month_label && child != prev_month_btn && child != next_month_btn)
            {
                lv_obj_del(child);
            }
        }

        // Day of week labels
        static const char *day_names[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        for (int i = 0; i < 7; i++)
        {
            lv_obj_t *day_label = lv_label_create(calendar_grid_cont);
            lv_label_set_text(day_label, day_names[i]);
            lv_obj_set_style_text_font(day_label, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(day_label, lv_color_hex(0x000000), 0);
            lv_obj_set_pos(day_label, 5 + i * 40, 30);
        }

        // Calculate first day of month weekday and number of days in month
        struct tm timeinfo = {0};
        timeinfo.tm_year = current_year - 1900;
        timeinfo.tm_mon = current_month - 1;
        timeinfo.tm_mday = 1;
        mktime(&timeinfo);
        int first_weekday = timeinfo.tm_wday; // 0=Sun, 1=Mon, ...
        int days_in_month = 31;
        if (current_month == 2)
        {
            // Leap year check
            bool leap = (current_year % 4 == 0 && (current_year % 100 != 0 || current_year % 400 == 0));
            days_in_month = leap ? 29 : 28;
        }
        else if (current_month == 4 || current_month == 6 || current_month == 9 || current_month == 11)
        {
            days_in_month = 30;
        }

        // Create day buttons
        for (int day = 1; day <= days_in_month; day++)
        {
            lv_obj_t *day_btn = lv_btn_create(calendar_grid_cont);
            lv_obj_set_size(day_btn, 35, 35);
            int col = (first_weekday + day - 1) % 7;
            int row = (first_weekday + day - 1) / 7;
            lv_obj_set_pos(day_btn, 5 + col * 40, 50 + row * 40);

            char day_str[4];
            snprintf(day_str, sizeof(day_str), "%d", day);
            lv_obj_t *label = lv_label_create(day_btn);
            lv_label_set_text(label, day_str);
            lv_obj_center(label);

            // TODO: Show bullet if day has events (to be implemented in next step)

            // Store day number in user data for click event
            lv_obj_set_user_data(day_btn, (void *)(intptr_t)day);
            lv_obj_add_event_cb(day_btn, day_btn_event_cb, LV_EVENT_CLICKED, NULL);
        }
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

        // Extract hour and minute from start datetime string
        String start_time_str = "";
        if (start)
        {
            String start_str(start);
            int t_pos = start_str.indexOf('T');
            if (t_pos != -1 && (t_pos + 6) < start_str.length())
            {
                start_time_str = start_str.substring(t_pos + 1, t_pos + 6); // HH:MM
            }
            else
            {
                start_time_str = start_str;
            }
        }

        event_start_textarea = lv_textarea_create(event_modal_cont);
        lv_textarea_set_one_line(event_start_textarea, true);
        lv_obj_set_width(event_start_textarea, lv_pct(39));
        lv_obj_align(event_start_textarea, LV_ALIGN_TOP_LEFT, 30, 80);
        lv_textarea_set_placeholder_text(event_start_textarea, "Start Time (HH:MM)");
        lv_textarea_set_text(event_start_textarea, start_time_str.c_str());

        // Extract hour and minute from end datetime string
        String end_time_str = "";
        if (end)
        {
            String end_str(end);
            int t_pos = end_str.indexOf('T');
            if (t_pos != -1 && (t_pos + 6) < end_str.length())
            {
                end_time_str = end_str.substring(t_pos + 1, t_pos + 6); // HH:MM
            }
            else
            {
                end_time_str = end_str;
            }
        }

        event_end_textarea = lv_textarea_create(event_modal_cont);
        lv_textarea_set_one_line(event_end_textarea, true);
        lv_obj_set_width(event_end_textarea, lv_pct(39));
        lv_obj_align(event_end_textarea, LV_ALIGN_TOP_RIGHT, -30, 80);
        lv_textarea_set_placeholder_text(event_end_textarea, "End Time (HH:MM)");
        lv_textarea_set_text(event_end_textarea, end_time_str.c_str());

        event_description_textarea = lv_textarea_create(event_modal_cont);
        lv_textarea_set_one_line(event_description_textarea, false);
        lv_obj_set_width(event_description_textarea, lv_pct(80));
        lv_obj_align(event_description_textarea, LV_ALIGN_TOP_MID, 0, 120);
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
        http.begin(GOOGLE_SCRIPT_URL_CALENDAR);
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
        http.begin(GOOGLE_SCRIPT_URL_CALENDAR);
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
        http.begin(GOOGLE_SCRIPT_URL_CALENDAR);
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
        // Update current year and month to current time
        struct tm timeinfo = Utils::getCurrentTime();
        int year = timeinfo.tm_year + 1900;
        int month = timeinfo.tm_mon + 1;
        if (year >= 2020)
        {
            current_year = year;
            current_month = month;
        }
        fetch_events();
    }

    static void update_events_ui(JsonVariant doc)
    {
        // Remove "no events" image if present
        static lv_obj_t *no_events_img = nullptr;
        if (no_events_img)
        {
            lv_obj_del(no_events_img);
            no_events_img = nullptr;
        }

        // Ensure events list is visible and clean it
        lv_obj_clear_flag(events_list, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clean(events_list);

        // Apply styling similar to notes list
        lv_obj_set_style_bg_opa(events_list, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(events_list, 0, 0);

        auto &&events_var = doc["items"];
        if (!events_var.is<JsonArray>() || events_var.as<JsonArray>().size() == 0)
        {
            // Hide the empty list so it doesn't interfere
            lv_obj_add_flag(events_list, LV_OBJ_FLAG_HIDDEN);

            // Create a sync image in the center of the main screen (optional, can be removed if not needed)
            no_events_img = lv_img_create(calendar_screen);
            lv_img_set_src(no_events_img, &lv_img_sync);
            lv_obj_add_flag(no_events_img, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_align(no_events_img, LV_ALIGN_CENTER, 0, 0);
            lv_obj_add_event_cb(no_events_img, refresh_event_cb, LV_EVENT_CLICKED, NULL);

            lv_label_set_text(status_label, ""); // Clear status label text
            return;
        }

        auto &&events = events_var.as<JsonArray>();

        for (int i = 0; i < (int)events.size(); i++)
        {
            JsonObject event = events[i].as<JsonObject>();
            String title = event["title"] | "Untitled";
            title = Utils::sanitizeString(title);
            lv_obj_t *btn = lv_list_add_btn(events_list, NULL, title.c_str());
            lv_obj_set_user_data(btn, (void *)(intptr_t)i);
            lv_obj_add_event_cb(btn, event_list_item_click_event_cb, LV_EVENT_CLICKED, NULL);
        }

        // Style each child item in the list to fix background and text color
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
        String url = GOOGLE_SCRIPT_URL_CALENDAR;

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
        LV_LOG_USER("Initializing Google Calendar App");
        data_mutex = xSemaphoreCreateMutex();

        // Initialize current year and month to current date if not set
        if (current_year == 0 || current_month == 0)
        {
            Utils::initTimeSync();
            struct tm timeinfo = Utils::getCurrentTime();
            int year = timeinfo.tm_year + 1900;
            int month = timeinfo.tm_mon + 1;
            if (year < 2020)
            {
                // Fallback default date if system time not set
                current_year = 2023;
                current_month = 1;
            }
            else
            {
                current_year = year;
                current_month = month;
            }
        }
        LV_LOG_USER("Current year: %d, current month: %d", current_year, current_month);

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
        lv_label_set_text(title_label, "Calendar");
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

        build_calendar_grid();

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
            update_calendar_grid();
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
