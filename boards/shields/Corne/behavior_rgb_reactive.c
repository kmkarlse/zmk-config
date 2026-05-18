/*
 * Global behavior shim that dispatches incoming commands to rgb_reactive.c.
 *
 * Declared with locality GLOBAL so any invocation on central is broadcast
 * to every connected peripheral over the split BLE link, and run locally
 * too. The behavior is intentionally not bound in the keymap — it's invoked
 * programmatically from layer_color.c (central) via
 * zmk_behavior_invoke_binding().
 */
#define DT_DRV_COMPAT zmk_behavior_rgb_reactive

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include "rgb_reactive.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#define LOCAL_HALF RGB_RX_HALF_PERIPH
#else
#define LOCAL_HALF RGB_RX_HALF_CENTRAL
#endif

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    uint32_t cmd = binding->param1;
    uint32_t arg = binding->param2;

    switch (cmd) {
    case RGB_RX_CMD_SET_LAYER:
        rgb_reactive_set_layer((uint8_t)arg);
        break;
    case RGB_RX_CMD_SET_MODE:
        rgb_reactive_set_mode((uint8_t)arg);
        break;
    case RGB_RX_CMD_FLASH:
        if (RGB_RX_FLASH_HALF(arg) == LOCAL_HALF) {
            rgb_reactive_flash((uint8_t)RGB_RX_FLASH_LED(arg));
        }
        break;
    default:
        LOG_WRN("unknown rgb_reactive cmd %u", cmd);
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
