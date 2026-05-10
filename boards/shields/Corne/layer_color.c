#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <dt-bindings/zmk/rgb.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct layer_color {
    uint16_t h;
    uint8_t s;
    uint8_t b;
};

static const struct layer_color layer_colors[] = {
    { .h = 182, .s = 73,  .b = 96 },  /* 0 BASE = cyan (RGB 66,239,245) */
    { .h = 120, .s = 100, .b = 60 },  /* 1 SYM  = green */
    { .h =   0, .s = 100, .b = 60 },  /* 2 NUM  = red */
};

/*
 * Drive the rgb_ug behavior instead of calling zmk_rgb_underglow_set_hsb()
 * directly. The behavior is BEHAVIOR_LOCALITY_GLOBAL, so invoking it via
 * zmk_behavior_invoke_binding() runs on central AND propagates to every
 * peripheral via split BLE. Direct API calls would only affect the local
 * half (this is why the right corne was stuck on the default red).
 */
static void invoke_rgb_ug(uint32_t param1, uint32_t param2) {
    struct zmk_behavior_binding binding = {
        .behavior_dev = "rgb_ug",
        .param1 = param1,
        .param2 = param2,
    };
    struct zmk_behavior_binding_event event = {
        .layer = 0,
        .position = 0,
        .timestamp = k_uptime_get(),
    };
    int ret = zmk_behavior_invoke_binding(&binding, event, true);
    if (ret < 0) {
        LOG_WRN("rgb_ug invoke returned %d", ret);
    }
}

static void apply_layer_color(uint8_t layer) {
    if (layer >= ARRAY_SIZE(layer_colors)) {
        return;
    }
    const struct layer_color *c = &layer_colors[layer];
    /* Force solid effect (effect index 0) then set the HSB. */
    invoke_rgb_ug(RGB_EFS_CMD, 0);
    invoke_rgb_ug(RGB_COLOR_HSB_CMD, RGB_COLOR_HSB_VAL(c->h, c->s, c->b));
}

static int layer_color_listener(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    apply_layer_color(zmk_keymap_highest_layer_active());
    return 0;
}

ZMK_LISTENER(layer_color, layer_color_listener);
ZMK_SUBSCRIPTION(layer_color, zmk_layer_state_changed);

static void layer_color_init_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    apply_layer_color(zmk_keymap_highest_layer_active());
}
K_WORK_DELAYABLE_DEFINE(layer_color_init_work, layer_color_init_work_handler);

static int layer_color_init(const struct device *dev) {
    ARG_UNUSED(dev);
    /* Wait long enough for the peripheral split connection to be up
     * (rgb_ug is BEHAVIOR_LOCALITY_GLOBAL — broadcasts only reach
     * already-connected peripherals). 3s is usually enough. */
    k_work_schedule(&layer_color_init_work, K_MSEC(3000));
    return 0;
}
SYS_INIT(layer_color_init, APPLICATION, 90);
