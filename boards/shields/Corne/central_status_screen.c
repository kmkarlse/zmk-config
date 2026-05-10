/*
 * Central-only status screen.
 *
 * Layout on the 128x32 OLED:
 *   top:    layer name (e.g. " BASE", " SYM", " NUM"), centered
 *   bottom: output status (USB / BLE profile), centered
 *
 * Compiled only on the central half via CMakeLists.txt; the peripheral has
 * its own custom_status_screen.c (bongo cat). Both override ZMK's built-in
 * status_screen.c, which is gated out by CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/display/widgets/layer_status.h>
#include <zmk/display/widgets/output_status.h>

static struct zmk_widget_layer_status layer_status_widget;
static struct zmk_widget_output_status output_status_widget;

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    zmk_widget_layer_status_init(&layer_status_widget, screen);
    lv_obj_align(zmk_widget_layer_status_obj(&layer_status_widget), LV_ALIGN_TOP_MID, 0, 0);

    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_BOTTOM_MID, 0, 0);

    return screen;
}
