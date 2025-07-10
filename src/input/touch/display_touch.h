#pragma once
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <Arduino.h>
#include "TouchDrvGT911.hpp"
#include "hardware/TDECK_PINS.h"

/**
 * Driver de Display + Touch + LVGL para LilyGO T-Deck
 *
 * Este módulo inicializa a tela TFT e o sensor de toque GT911,
 * configura o LVGL para renderização gráfica e fornece funções utilitárias
 * para ajuste de brilho e leitura de eventos de toque.
 *
 * Exemplo de uso:
 *
 * #include "input/display_touch.h"
 *
 * void setup() {
 *     Serial.begin(115200);
 *     DisplayTouch::init();
 *     DisplayTouch::create_slider_example();
 * }
 *
 * void loop() {
 *     DisplayTouch::handle();
 * }
 */

namespace DisplayTouch {

#define LVGL_BUFFER_SIZE (TFT_WIDTH * TFT_HEIGHT * sizeof(lv_color_t))
#define BRIGHTNESS_STEPS 16

TFT_eSPI tft;
TouchDrvGT911 touch;
static lv_obj_t* slider_label;

/**
 * Ajusta o brilho do display TFT.
 * @param value Brilho entre 0 (mínimo) e 16 (máximo).
 */
inline void set_brightness(uint8_t value) {
    static uint8_t level = 0;
    if (value == 0) {
        digitalWrite(TDECK_TFT_BACKLIGHT, LOW);
        delay(3);
        level = 0;
        return;
    }
    if (level == 0) {
        digitalWrite(TDECK_TFT_BACKLIGHT, HIGH);
        level = BRIGHTNESS_STEPS;
        delayMicroseconds(30);
    }
    int from = BRIGHTNESS_STEPS - level;
    int to = BRIGHTNESS_STEPS - value;
    int num = (BRIGHTNESS_STEPS + to - from) % BRIGHTNESS_STEPS;
    for (int i = 0; i < num; ++i) {
        digitalWrite(TDECK_TFT_BACKLIGHT, LOW);
        digitalWrite(TDECK_TFT_BACKLIGHT, HIGH);
    }
    level = value;
}

/**
 * Função de flush de tela, chamada pelo LVGL.
 */
static void disp_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)&color_p->full, w * h, false);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

/**
 * Função de leitura do sensor de toque GT911.
 */
static void touchpad_read(lv_indev_drv_t* indev_driver, lv_indev_data_t* data) {
    data->state = LV_INDEV_STATE_REL;
    int16_t x[5], y[5];

    if (touch.isPressed()) {
        uint8_t touched = touch.getPoint(x, y, touch.getSupportTouchPoint());
        if (touched > 0) {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = x[0];
            data->point.y = y[0];
        }
    }
}

/**
 * Inicializa display, sensor de toque e o LVGL.
 */
inline void init() {
    Serial.println("Inicializando DisplayTouch...");

    // Habilita periféricos e pinos
    pinMode(TDECK_PERI_POWERON, OUTPUT);
    digitalWrite(TDECK_PERI_POWERON, HIGH);

    pinMode(TDECK_SDCARD_CS, OUTPUT);
    pinMode(TDECK_RADIO_CS, OUTPUT);
    pinMode(TDECK_TFT_CS, OUTPUT);
    digitalWrite(TDECK_SDCARD_CS, HIGH);
    digitalWrite(TDECK_RADIO_CS, HIGH);
    digitalWrite(TDECK_TFT_CS, HIGH);

    pinMode(TDECK_SPI_MISO, INPUT_PULLUP);
    SPI.begin(TDECK_SPI_SCK, TDECK_SPI_MISO, TDECK_SPI_MOSI);

    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    pinMode(TDECK_TOUCH_INT, INPUT);
    delay(20);

    Wire.begin(TDECK_I2C_SDA, TDECK_I2C_SCL);
    touch.setPins(-1, TDECK_TOUCH_INT);
    if (!touch.begin(Wire, GT911_SLAVE_ADDRESS_L)) {
        Serial.println("Erro ao inicializar sensor GT911!");
        while (true) delay(1000);
    }
    Serial.println("Sensor GT911 OK!");

    touch.setMaxCoordinates(320, 240);
    touch.setSwapXY(true);
    touch.setMirrorXY(false, true);

    // Inicialização do LVGL
    lv_init();
    static lv_color_t* buf = (lv_color_t*)ps_malloc(LVGL_BUFFER_SIZE);
    assert(buf && "Falha na alocação do buffer LVGL!");

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, LVGL_BUFFER_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TFT_HEIGHT;
    disp_drv.ver_res = TFT_WIDTH;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.full_refresh = 1;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // Ativa luz de fundo
    pinMode(TDECK_TFT_BACKLIGHT, OUTPUT);
    set_brightness(16);
}

/**
 * Cria um slider de exemplo com evento de callback.
 */
inline void create_slider_example() {
    lv_obj_t* slider = lv_slider_create(lv_scr_act());
    lv_obj_center(slider);
    lv_obj_add_event_cb(slider, [](lv_event_t* e) {
        lv_obj_t* slider = lv_event_get_target(e);
        char buf[8];
        lv_snprintf(buf, sizeof(buf), "%d%%", (int)lv_slider_get_value(slider));
        lv_label_set_text(slider_label, buf);
        lv_obj_align_to(slider_label, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    }, LV_EVENT_VALUE_CHANGED, NULL);

    slider_label = lv_label_create(lv_scr_act());
    lv_label_set_text(slider_label, "0%");
    lv_obj_align_to(slider_label, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
}

/**
 * Atualiza eventos e renderizações do LVGL.
 * Deve ser chamada regularmente dentro do loop().
 */
inline void handle() {
    lv_timer_handler();
    delay(5);
}

} // namespace DisplayTouch
