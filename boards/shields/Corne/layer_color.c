/*
 * Central-side RGB coordinator.
 *
 * Observes layer changes (central-only in ZMK) and broadcasts the new
 * layer to both halves via a BEHAVIOR_LOCALITY_GLOBAL behavior. The
 * controller in rgb_reactive.c decides what to draw based on layer.
 */
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

#include "rgb_reactive.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* Must match the DT `label` on the rgb_reactive behavior node and
 * fit within ZMK_SPLIT_RUN_BEHAVIOR_DEV_LEN (9 incl. NUL = 8 chars
 * max). */
#define BEHAVIOR_NAME "rgb_rx"

static void invoke(uint32_t cmd, uint32_t arg) {
    struct zmk_behavior_binding binding = {
        .behavior_dev = BEHAVIOR_NAME,
        .param1 = cmd,
        .param2 = arg,
    };
    struct zmk_behavior_binding_event event = {
        .layer = 0,
        .position = 0,
        .timestamp = k_uptime_get(),
    };
    int ret = zmk_behavior_invoke_binding(&binding, event, true);
    if (ret < 0) {
        LOG_WRN("rgb_reactive invoke (cmd=%u arg=%u) returned %d", cmd, arg, ret);
    }
}

static int layer_listener(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    invoke(RGB_RX_CMD_SET_LAYER, (uint8_t)zmk_keymap_highest_layer_active());
    return 0;
}
ZMK_LISTENER(rgb_reactive_layer, layer_listener);
ZMK_SUBSCRIPTION(rgb_reactive_layer, zmk_layer_state_changed);

/* Initial broadcast — wait for the peripheral split link to be up before
 * sending behavior invocations (rgb_reactive is GLOBAL-locality, broadcasts
 * only reach already-connected peripherals). 3s matches what the previous
 * rgb_ug-based implementation used successfully. */
static void boot_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    invoke(RGB_RX_CMD_SET_LAYER, (uint8_t)zmk_keymap_highest_layer_active());
}
K_WORK_DELAYABLE_DEFINE(boot_work, boot_work_handler);

static int rgb_reactive_central_init(const struct device *dev) {
    ARG_UNUSED(dev);
    k_work_schedule(&boot_work, K_MSEC(3000));
    return 0;
}
SYS_INIT(rgb_reactive_central_init, APPLICATION, 90);
