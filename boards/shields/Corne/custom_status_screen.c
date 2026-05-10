/*
 * Peripheral-only status screen: shows the bongo cat full-screen.
 *
 * Compiled only for the peripheral half via CMakeLists.txt. ZMK provides
 * a __weak default for zmk_display_status_screen() that this overrides.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <lvgl.h>

#include <zmk/display.h>
#include "widgets/bongo_cat.h"

static struct zmk_widget_bongo_cat bongo_cat_widget;

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    zmk_widget_bongo_cat_init(&bongo_cat_widget, screen);
    lv_obj_align(zmk_widget_bongo_cat_obj(&bongo_cat_widget), LV_ALIGN_CENTER, 0, 0);

    return screen;
}
