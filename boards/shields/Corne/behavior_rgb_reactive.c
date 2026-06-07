/*
 * Global behavior shim that dispatches incoming commands to rgb_reactive.c.
 *
 * Declared with locality GLOBAL so any invocation on central is broadcast
 * to every connected peripheral over the split BLE link, and run locally
 * too. Invoked programmatically from layer_color.c, not bound in keymap.
 */
#define DT_DRV_COMPAT zmk_behavior_rgb_reactive

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include "rgb_reactive.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    switch (binding->param1) {
    case RGB_RX_CMD_SET_LAYER:
        /* The dongle (central) has no strip — rgb_reactive.c isn't
         * compiled there, so there's nothing to set locally. The
         * GLOBAL locality still forwards this invocation to both
         * peripherals, which DO have strips and apply it. */
#if IS_ENABLED(CONFIG_WS2812_STRIP)
        rgb_reactive_set_layer((uint8_t)binding->param2);
#endif
        break;
    default:
        LOG_WRN("unknown rgb_reactive cmd %u", binding->param1);
        break;
    }
    return 0;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return 0;
}

static const struct behavior_driver_api behavior_rgb_reactive_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_rgb_reactive_driver_api);
