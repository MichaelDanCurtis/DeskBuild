#include "ui.h"

#include <lvgl.h>

#include "config.h"

namespace ui {
namespace {

constexpr int kMin = 0;
constexpr int kMax = 100;

lv_obj_t* arc = nullptr;
lv_obj_t* valueLabel = nullptr;
int value = 50;

void show() {
    lv_arc_set_value(arc, value);
    lv_label_set_text_fmt(valueLabel, "%d", value);
}

}  // namespace

void begin() {
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);  // AMOLED black = pixels off
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    arc = lv_arc_create(scr);
    lv_obj_set_size(arc, 420, 420);
    lv_obj_align(arc, LV_ALIGN_CENTER, config::kUiCenterOffsetX, 0);
    lv_arc_set_bg_angles(arc, 135, 45);
    lv_arc_set_range(arc, kMin, kMax);
    lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc, 24, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x262626), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 24, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0xFFB000), LV_PART_INDICATOR);

    valueLabel = lv_label_create(scr);
    lv_obj_set_style_text_font(valueLabel, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(valueLabel, lv_color_white(), 0);
    lv_obj_align(valueLabel, LV_ALIGN_CENTER, config::kUiCenterOffsetX, -8);

    lv_obj_t* caption = lv_label_create(scr);
    lv_label_set_text(caption, "LEVEL");
    lv_obj_set_style_text_color(caption, lv_color_hex(0x8A8A8A), 0);
    lv_obj_set_style_text_letter_space(caption, 4, 0);
    lv_obj_align(caption, LV_ALIGN_CENTER, config::kUiCenterOffsetX, 44);

    show();
}

void addDetents(int detents) {
    if (!arc) return;
    value += detents;
    if (value < kMin) value = kMin;
    if (value > kMax) value = kMax;
    show();
}

}  // namespace ui
