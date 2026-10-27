#include "ui.h"

#include <lvgl.h>

#include <cstdio>

#include "config.h"

namespace ui {
namespace {

constexpr int kMin = 0;
constexpr int kMax = 100;

lv_obj_t* arc = nullptr;
lv_obj_t* valueLabel = nullptr;
lv_obj_t* caption = nullptr;
lv_obj_t* hint = nullptr;
int value = 50;
Screen current = Screen::Dial;
float shownMm = 0.0f;
bool shown = false;

void showDialValue() {
    lv_arc_set_value(arc, value);
    lv_label_set_text_fmt(valueLabel, "%d", value);
}

void setVisible(lv_obj_t* obj, bool visible) {
    if (visible) lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, uint32_t color, int y) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l, LV_ALIGN_CENTER, config::kUiCenterOffsetX, y);
    return l;
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

    valueLabel = label(scr, &lv_font_montserrat_48, 0xFFFFFF, -8);
    caption = label(scr, &lv_font_montserrat_16, 0x8A8A8A, 44);
    lv_obj_set_style_text_letter_space(caption, 4, 0);
    hint = label(scr, &lv_font_montserrat_16, 0x8A8A8A, 120);
    lv_obj_set_width(hint, 300);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);

    show(Screen::Dial, 0.0f);
}

void addDetents(int detents) {
    if (!arc) return;
    value += detents;
    if (value < kMin) value = kMin;
    if (value > kMax) value = kMax;
    if (current == Screen::Dial) showDialValue();
}

void show(Screen screen, float valueMm) {
    if (!arc) return;
    const bool alignScreen = screen == Screen::AlignFlush || screen == Screen::AlignHeight;
    if (shown && screen == current && (!alignScreen || valueMm == shownMm)) return;
    shown = true;
    current = screen;
    shownMm = valueMm;

    setVisible(arc, screen == Screen::Dial);
    setVisible(valueLabel, screen == Screen::Dial || alignScreen);
    setVisible(hint, screen != Screen::Dial);
    switch (screen) {
        case Screen::Dial:
            lv_label_set_text(caption, "LEVEL");
            showDialValue();
            break;
        case Screen::AlignFlush:
        case Screen::AlignHeight: {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.2f", valueMm);  // LVGL's own printf has no %f
            lv_label_set_text(valueLabel, buf);
            lv_label_set_text(caption, screen == Screen::AlignFlush ? "FLUSH  MM" : "HEIGHT  MM");
            lv_label_set_text(hint, screen == Screen::AlignFlush ? "turn the ring to set flush\npress to set height"
                                                                 : "turn the ring to set height\npress to save");
            break;
        }
        case Screen::Homing:
            lv_label_set_text(caption, "HOMING");
            lv_label_set_text(hint, "");
            break;
        case Screen::Fault:
            lv_label_set_text(caption, "CHECK MECHANISM");
            lv_label_set_text(hint, "see the USB console");
            break;
    }
}

}  // namespace ui
