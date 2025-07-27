#pragma once
#include <lvgl.h>
#include "input/input.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "utils/utils.h"
#include <ArduinoJson.h>

// Incluído a partir de apps.h, que já define o AppManager.

// --- Inclusão dos Ícones de Clima ---
// Presume-se que os arquivos .c dos ícones estão em uma subpasta 'Icons'
#include "Icons/weather/lv_img_weather_sun.c"
#include "Icons/weather/lv_img_weather_moon.c"
#include "Icons/weather/lv_img_weather_cloud_sun.c"
#include "Icons/weather/lv_img_weather_cloud_moon.c"
#include "Icons/weather/lv_img_weather_cloud.c"
#include "Icons/weather/lv_img_weather_rain.c"
#include "Icons/weather/lv_img_weather_thunderstorm.c"
#include "Icons/weather/lv_img_weather_snow.c"
#include "Icons/weather/lv_img_weather_mist.c"
#include "Icons/weather/lv_img_weather_unknown.c"

namespace Weather {

// --- API Configuration ---
// Obtenha sua chave de API gratuita em: https://openweathermap.org/appid
const char* API_KEY = "5de1f71c808c69aa34c5edd6c7d66cc9"; // Substitua pela sua chave
const char* CITY_ID = "3448439"; // ID da cidade para São Paulo
const char* UNITS = "metric"; // metric = Celsius, imperial = Fahrenheit

// --- UI Variables ---
static lv_obj_t* weather_screen = nullptr;
static lv_obj_t* city_label = nullptr;
static lv_obj_t* weather_icon_img = nullptr; // Widget de imagem para o ícone
static lv_obj_t* temp_label = nullptr;
static lv_obj_t* desc_label = nullptr;
static lv_obj_t* forecast_label = nullptr;

// --- Function Prototypes ---
static void back_button_event_cb(lv_event_t* e);
static const lv_img_dsc_t* get_weather_image_from_code(const char* icon_code);
static void update_weather_ui(const char* city, float temp, const char* desc, const lv_img_dsc_t* img_src);
static void refresh_event_cb(lv_event_t* e);
static void fetch_weather_data();

/**
 * @brief Callback para o botão de voltar. Retorna ao menu principal.
 */
static void back_button_event_cb(lv_event_t* e) {
    AppManager::show_app(AppManager::APP_MAIN_MENU);
}

/**
 * @brief Callback para o clique na tela, para atualizar os dados do clima.
 */
static void refresh_event_cb(lv_event_t* e) {
    // O evento de clique é anexado à tela principal. O botão "Voltar" tem seu próprio
    // callback, que consome o evento e impede que ele chegue aqui.
    // Portanto, qualquer clique que chega aqui é para um refresh.
    if (WiFi.status() == WL_CONNECTED) {
        LV_LOG_USER("Screen clicked, refreshing weather data...");
        lv_label_set_text(city_label, "Updating...");
        lv_img_set_src(weather_icon_img, &lv_img_weather_unknown);
        lv_label_set_text(temp_label, "--°C");
        lv_label_set_text(desc_label, "");
        fetch_weather_data();
    }
}

/**
 * @brief Mapeia o código do ícone da API OpenWeatherMap para uma imagem LVGL.
 * @param icon_code O código do ícone da API (ex: "01d", "10n").
 * @return Um ponteiro para a estrutura da imagem LVGL.
 */
static const lv_img_dsc_t* get_weather_image_from_code(const char* icon_code) {
    if (strcmp(icon_code, "01d") == 0) return &lv_img_weather_sun;
    if (strcmp(icon_code, "01n") == 0) return &lv_img_weather_moon;
    if (strcmp(icon_code, "02d") == 0) return &lv_img_weather_cloud_sun;
    if (strcmp(icon_code, "02n") == 0) return &lv_img_weather_cloud_moon;
    if (strcmp(icon_code, "03d") == 0 || strcmp(icon_code, "03n") == 0) return &lv_img_weather_cloud;
    if (strcmp(icon_code, "04d") == 0 || strcmp(icon_code, "04n") == 0) return &lv_img_weather_cloud; // Usando o mesmo para nuvens quebradas
    if (strcmp(icon_code, "09d") == 0 || strcmp(icon_code, "09n") == 0) return &lv_img_weather_rain;
    if (strcmp(icon_code, "10d") == 0 || strcmp(icon_code, "10n") == 0) return &lv_img_weather_rain;
    if (strcmp(icon_code, "11d") == 0 || strcmp(icon_code, "11n") == 0) return &lv_img_weather_thunderstorm;
    if (strcmp(icon_code, "13d") == 0 || strcmp(icon_code, "13n") == 0) return &lv_img_weather_snow;
    if (strcmp(icon_code, "50d") == 0 || strcmp(icon_code, "50n") == 0) return &lv_img_weather_mist;
    return &lv_img_weather_unknown; // Imagem padrão
}

/**
 * @brief Atualiza os labels da UI com os novos dados de clima.
 */
static void update_weather_ui(const char* city, float temp, const char* desc, const lv_img_dsc_t* img_src) {
    lv_label_set_text(city_label, city);
    lv_img_set_src(weather_icon_img, img_src);
    lv_label_set_text_fmt(temp_label, "%.0f°C", temp);
    lv_label_set_text(desc_label, desc);
    lv_label_set_text(forecast_label, "Updated successfully.");
}

/**
 * @brief Busca os dados de clima da API OpenWeatherMap.
 */
static void fetch_weather_data() {
    // Use WiFiClient client; se quiser passar um client específico
    HTTPClient http;
    String api_url = "https://api.openweathermap.org/data/2.5/weather?id="; // Usa HTTPS para maior confiabilidade
    api_url += CITY_ID;
    api_url += "&appid=";
    api_url += API_KEY;
    api_url += "&units=";
    api_url += UNITS;

    LV_LOG_USER("Fetching weather data from: %s", api_url.c_str());

    // O HTTPClient do ESP32 geralmente consegue lidar com HTTPS básico sem um certificado CA raiz
    http.begin(api_url);
    http.setConnectTimeout(5000); // Timeout de conexão de 5 segundos
    http.setTimeout(5000);        // Timeout de resposta de 5 segundos

    int http_code = http.GET();

    if (http_code > 0) {
        if (http_code == HTTP_CODE_OK) {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (error) {
                LV_LOG_ERROR("deserializeJson() failed: %s", error.c_str());
                lv_label_set_text(forecast_label, "JSON parsing failed.");
            } else {
                // Usa a classe String do Arduino e a função utilitária para limpar o nome da cidade
                String city_name_str = doc["name"];
                city_name_str = Utils::sanitizeString(city_name_str);

                float temperature = doc["main"]["temp"];
                const char* description = doc["weather"][0]["description"];
                const char* icon_code = doc["weather"][0]["icon"];

                update_weather_ui(city_name_str.c_str(), temperature, description, get_weather_image_from_code(icon_code));
            }
        } else {
            LV_LOG_ERROR("HTTP GET failed, error code: %d, message: %s", http_code, http.errorToString(http_code).c_str());
            char error_msg[64];
            snprintf(error_msg, sizeof(error_msg), "HTTP Error: %d", http_code);
            lv_label_set_text(forecast_label, error_msg);
        }
    } else {
        LV_LOG_ERROR("HTTP GET failed, error: %s", http.errorToString(http_code).c_str());
        lv_label_set_text(forecast_label, "Connection error.");
    }
    http.end();
}


/**
 * @brief Inicializa a tela de clima e seus elementos.
 */
inline void init() {
    weather_screen = lv_obj_create(NULL);
    lv_obj_add_flag(weather_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(weather_screen, 320, 240);
    lv_obj_set_style_bg_color(weather_screen, lv_color_hex(0xFDF5E6), LV_PART_MAIN);
    // Adiciona o evento de clique à tela para permitir o refresh
    lv_obj_add_flag(weather_screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(weather_screen, refresh_event_cb, LV_EVENT_CLICKED, NULL);

    // --- Botão de Voltar ---
    lv_obj_t* back_btn = lv_btn_create(weather_screen);
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
    lv_obj_t* screen_title = lv_label_create(weather_screen);
    lv_label_set_text(screen_title, "Weather");
    lv_obj_set_style_text_font(screen_title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(screen_title, lv_color_hex(0x0B3C5D), 0);
    lv_obj_align(screen_title, LV_ALIGN_TOP_MID, 0, 10);

    // --- Contêiner Principal de Informações ---
    lv_obj_t* main_container = lv_obj_create(weather_screen);
    lv_obj_set_size(main_container, 300, 180);
    lv_obj_align(main_container, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_opa(main_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_container, 0, 0);
    lv_obj_add_event_cb(main_container, refresh_event_cb, LV_EVENT_CLICKED, NULL);

    // --- Nome da Cidade ---
    city_label = lv_label_create(main_container);
    lv_label_set_text(city_label, "Loading...");
    lv_obj_set_style_text_font(city_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(city_label, lv_color_hex(0x0B3C5D), 0);
    lv_obj_align(city_label, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_event_cb(city_label, refresh_event_cb, LV_EVENT_CLICKED, NULL);

    // --- Ícone do Clima (agora como imagem) ---
    weather_icon_img = lv_img_create(main_container);
    lv_img_set_src(weather_icon_img, &lv_img_weather_unknown); // Imagem inicial
    lv_obj_align(weather_icon_img, LV_ALIGN_LEFT_MID, 15, 0);
    lv_img_set_zoom(weather_icon_img, LV_IMG_ZOOM_NONE + 64); // Ajuste o zoom conforme necessário
    lv_obj_add_event_cb(weather_icon_img, refresh_event_cb, LV_EVENT_CLICKED, NULL);

    // --- Temperatura Atual ---
    temp_label = lv_label_create(main_container);
    lv_label_set_text(temp_label, "--°C");
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(temp_label, lv_color_hex(0x0B3C5D), 0);
    lv_obj_align(temp_label, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_add_event_cb(temp_label, refresh_event_cb, LV_EVENT_CLICKED, NULL);

    // --- Descrição do Clima ---
    desc_label = lv_label_create(main_container);
    lv_label_set_text(desc_label, "No data");
    lv_obj_set_style_text_font(desc_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(desc_label, lv_color_hex(0x324A5F), 0); // Um azul um pouco mais claro
    lv_obj_set_width(desc_label, lv_pct(60)); // Largura reduzida para a direita
    lv_label_set_long_mode(desc_label, LV_LABEL_LONG_WRAP); // Habilita a quebra de linha
    lv_obj_set_style_text_align(desc_label, LV_TEXT_ALIGN_RIGHT, 0); // Alinha o texto à direita
    lv_obj_align(desc_label, LV_ALIGN_BOTTOM_RIGHT, -20, -20); // Alinha o widget à direita
    lv_obj_add_event_cb(desc_label, refresh_event_cb, LV_EVENT_CLICKED, NULL);

    // --- Placeholder para a Previsão ---
    // Movido para ser filho da tela principal para um posicionamento de canto mais fácil
    forecast_label = lv_label_create(weather_screen);
    lv_label_set_text(forecast_label, "");
    lv_obj_set_style_text_font(forecast_label, &lv_font_montserrat_12, 0); // Fonte menor
    lv_obj_set_style_text_color(forecast_label, lv_color_hex(0x324A5F), 0);
    lv_obj_align(forecast_label, LV_ALIGN_BOTTOM_RIGHT, -10, -10); // Alinha no canto inferior direito da tela
    lv_obj_add_event_cb(forecast_label, refresh_event_cb, LV_EVENT_CLICKED, NULL);
}

/**
 * @brief Mostra a tela de clima.
 */
inline void show() {
    if (weather_screen) {
        if (WiFi.status() == WL_CONNECTED) {
            lv_label_set_text(city_label, "Updating...");
            lv_img_set_src(weather_icon_img, &lv_img_weather_unknown);
            lv_label_set_text(temp_label, "--°C");
            lv_label_set_text(desc_label, "");
            fetch_weather_data();
        } else {
            lv_label_set_text(city_label, "No Connection");
            lv_img_set_src(weather_icon_img, &lv_img_weather_unknown);
            lv_label_set_text(temp_label, "N/A");
            lv_label_set_text(desc_label, "Connect to Wi-Fi in Settings");
        }
        lv_obj_clear_flag(weather_screen, LV_OBJ_FLAG_HIDDEN);
        lv_scr_load(weather_screen);
    }
}

/**
 * @brief Gerencia a entrada do usuário e atualiza a interface.
 */
inline void handle() {
    // Por enquanto, apenas o timer do LVGL é necessário.
    lv_timer_handler();
}

} // namespace Weather