#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/rgb_underglow.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static const struct zmk_led_hsb layer_colors[] = {
    { .h = 182, .s = 73,  .b = 96 },  /* 0 BASE = cyan (RGB 66,239,245) */
    { .h = 120, .s = 100, .b = 60 },  /* 1 SYM  = green */
};

static int layer_color_listener(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    uint8_t layer = zmk_keymap_highest_layer_active();
    if (layer < ARRAY_SIZE(layer_colors)) {
        zmk_rgb_underglow_select_effect(0); /* solid */
        zmk_rgb_underglow_set_hsb(layer_colors[layer]);
    }
    return 0;
}

ZMK_LISTENER(layer_color, layer_color_listener);
ZMK_SUBSCRIPTION(layer_color, zmk_layer_state_changed);
