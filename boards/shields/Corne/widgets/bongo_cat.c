/*
 * Bongo cat widget (adapted from SamIAm2000/zmk bongo-cat branch).
 *
 * Subscribes to zmk_position_state_changed instead of zmk_keycode_state_changed
 * because this widget is intended to run on the *peripheral* half, where the
 * keymap doesn't run and zmk_keycode_state_changed never fires. Position
 * events fire on each side for that side's own physical keys.
 *
 * SPDX-License-Identifier: MIT
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/display.h>
#include <lvgl.h>

#include "bongo_cat.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static lv_anim_t idle_anim;
static lv_timer_t *idle_check_timer = NULL;

LV_IMG_DECLARE(idle_img1);
LV_IMG_DECLARE(idle_img2);
LV_IMG_DECLARE(idle_img3);
LV_IMG_DECLARE(idle_img4);
LV_IMG_DECLARE(idle_img5);
LV_IMG_DECLARE(fast_img1);
LV_IMG_DECLARE(fast_img2);

static const void *idle_images[] = {&idle_img1, &idle_img2, &idle_img3, &idle_img4, &idle_img5};
static const void *tap_images[] = {&fast_img1, &fast_img2};

#define IDLE_FRAMES 5
#define TAP_FRAMES 2
#define IDLE_ANIM_TIME 1000   /* ms for full idle cycle */
#define IDLE_TIMEOUT_MS 500   /* return to idle after this much keypress silence */
#define IDLE_CHECK_PERIOD 100 /* ms */

struct bongo_cat_state {
    bool key_pressed;
    uint32_t last_tap;
    lv_obj_t *obj;
    bool is_idle;
};

static struct bongo_cat_state current_state = {
    .key_pressed = false, .last_tap = 0, .obj = NULL, .is_idle = true};

static void set_idle_frame(void *var, int32_t val) {
    lv_obj_t *img = (lv_obj_t *)var;
    int frame = val % IDLE_FRAMES;
    lv_img_set_src(img, idle_images[frame]);
}

static void start_idle_animation(lv_obj_t *obj) {
    if (current_state.is_idle) {
        return;
    }
    current_state.is_idle = true;

    lv_anim_init(&idle_anim);
    lv_anim_set_var(&idle_anim, obj);
    lv_anim_set_values(&idle_anim, 0, IDLE_FRAMES - 1);
    lv_anim_set_time(&idle_anim, IDLE_ANIM_TIME);
    lv_anim_set_exec_cb(&idle_anim, set_idle_frame);
    lv_anim_set_repeat_count(&idle_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&idle_anim);
}

static void check_idle_timeout(lv_timer_t *timer) {
    uint32_t now = k_uptime_get_32();
    if ((now - current_state.last_tap) >= IDLE_TIMEOUT_MS && current_state.obj != NULL &&
        !current_state.is_idle) {
        start_idle_animation(current_state.obj);
    }
}

static void play_tap_animation(lv_obj_t *obj) {
    current_state.is_idle = false;
    lv_anim_del(obj, set_idle_frame);

    static uint8_t current_frame = 0;
    current_frame = (current_frame + 1) % TAP_FRAMES;
    lv_img_set_src(obj, tap_images[current_frame]);
}

static void update_bongo_cat_anim(struct zmk_widget_bongo_cat *widget,
                                  struct bongo_cat_state state) {
    if (!widget || !widget->obj) {
        return;
    }
    current_state.obj = widget->obj;
    if (state.key_pressed) {
        play_tap_animation(widget->obj);
    }
}

static struct bongo_cat_state bongo_cat_get_state(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev != NULL && ev->state) {
        current_state.key_pressed = true;
        current_state.last_tap = k_uptime_get_32();
    } else {
        current_state.key_pressed = false;
    }
    return current_state;
}

static void bongo_cat_update_cb(struct bongo_cat_state state) {
    struct zmk_widget_bongo_cat *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { update_bongo_cat_anim(widget, state); }
}

int zmk_widget_bongo_cat_init(struct zmk_widget_bongo_cat *widget, lv_obj_t *parent) {
    if (!widget || !parent) {
        return -1;
    }

    widget->obj = lv_img_create(parent);
    if (widget->obj == NULL) {
        return -1;
    }

    if (idle_check_timer == NULL) {
        idle_check_timer = lv_timer_create(check_idle_timeout, IDLE_CHECK_PERIOD, NULL);
    }

    current_state.obj = widget->obj;
    current_state.is_idle = false; /* so start_idle_animation actually runs */
    start_idle_animation(widget->obj);

    sys_slist_append(&widgets, &widget->node);
    return 0;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_bongo_cat, struct bongo_cat_state, bongo_cat_update_cb,
                            bongo_cat_get_state)
ZMK_SUBSCRIPTION(widget_bongo_cat, zmk_position_state_changed);

lv_obj_t *zmk_widget_bongo_cat_obj(struct zmk_widget_bongo_cat *widget) { return widget->obj; }
