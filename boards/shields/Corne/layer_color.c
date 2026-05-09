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
    { .h =   0, .s = 100, .b = 60 },  /* 2 NUM  = red */
    /* Add more entries here as you add layers. Index = layer number. */
};

static void apply_layer_color(uint8_t layer) {
    if (layer < ARRAY_SIZE(layer_colors)) {
        zmk_rgb_underglow_select_effect(0); /* solid */
        zmk_rgb_underglow_set_hsb(layer_colors[layer]);
    }
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

static int layer_color_init(void) {
    /* Delay so rgb_underglow has finished its own init + settings load */
    k_work_schedule(&layer_color_init_work, K_MSEC(1000));
    return 0;
}
SYS_INIT(layer_color_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
