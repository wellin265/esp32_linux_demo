#include "ksdiy_example_display.h"

#include <string.h>

#include "ksdiy_lvgl_port.h"
#include "lvgl.h"

static lv_obj_t *s_title;
static lv_obj_t *s_subtitle;
static lv_obj_t *s_line1;
static lv_obj_t *s_line2;
static lv_obj_t *s_line3;

static void set_label_text(lv_obj_t *label, const char *text)
{
    if (label == NULL) {
        return;
    }
    lv_label_set_text(label, text != NULL ? text : "");
}

void ksdiy_example_display_bootstrap(const char *title, const char *subtitle)
{
    ksdiy_lvgl_port_init();
    if (!ksdiy_lvgl_lock(1000)) {
        return;
    }

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101418), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    s_title = lv_label_create(screen);
    lv_obj_set_width(s_title, 220);
    lv_obj_align(s_title, LV_ALIGN_TOP_LEFT, 12, 16);
    lv_obj_set_style_text_color(s_title, lv_color_hex(0xF2F5F8), 0);
    lv_obj_set_style_text_font(s_title, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(s_title, LV_LABEL_LONG_WRAP);

    s_subtitle = lv_label_create(screen);
    lv_obj_set_width(s_subtitle, 220);
    lv_obj_align(s_subtitle, LV_ALIGN_TOP_LEFT, 12, 48);
    lv_obj_set_style_text_color(s_subtitle, lv_color_hex(0x8BA0B3), 0);
    lv_label_set_long_mode(s_subtitle, LV_LABEL_LONG_WRAP);

    s_line1 = lv_label_create(screen);
    lv_obj_set_width(s_line1, 220);
    lv_obj_align(s_line1, LV_ALIGN_TOP_LEFT, 12, 96);
    lv_obj_set_style_text_color(s_line1, lv_color_hex(0xD7E0E8), 0);
    lv_label_set_long_mode(s_line1, LV_LABEL_LONG_WRAP);

    s_line2 = lv_label_create(screen);
    lv_obj_set_width(s_line2, 220);
    lv_obj_align(s_line2, LV_ALIGN_TOP_LEFT, 12, 126);
    lv_obj_set_style_text_color(s_line2, lv_color_hex(0xD7E0E8), 0);
    lv_label_set_long_mode(s_line2, LV_LABEL_LONG_WRAP);

    s_line3 = lv_label_create(screen);
    lv_obj_set_width(s_line3, 220);
    lv_obj_align(s_line3, LV_ALIGN_TOP_LEFT, 12, 156);
    lv_obj_set_style_text_color(s_line3, lv_color_hex(0xD7E0E8), 0);
    lv_label_set_long_mode(s_line3, LV_LABEL_LONG_WRAP);

    set_label_text(s_title, title);
    set_label_text(s_subtitle, subtitle);
    set_label_text(s_line1, "Display ready");
    set_label_text(s_line2, "");
    set_label_text(s_line3, "");

    ksdiy_lvgl_unlock();
}

void ksdiy_example_display_set_lines(const char *line1, const char *line2, const char *line3)
{
    if (!ksdiy_lvgl_lock(1000)) {
        return;
    }

    set_label_text(s_line1, line1);
    set_label_text(s_line2, line2);
    set_label_text(s_line3, line3);

    ksdiy_lvgl_unlock();
}
