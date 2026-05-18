/*
 * Central-side RGB coordinator.
 *
 * Observes layer / activity / position events (all central-only in ZMK),
 * translates them into rgb_reactive behavior invocations, and lets the
 * BEHAVIOR_LOCALITY_GLOBAL plumbing broadcast each invocation to the
 * peripheral over split BLE. The actual LED writes happen in
 * rgb_reactive.c on whichever half receives the invocation.
 *
 * Per-key reactive flashes are only sent while on the BASE layer (0) and
 * while the keyboard is ACTIVE, to keep BLE chatter down and to avoid
 * fighting the idle swirl.
 */
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

#include <zmk/activity.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>

#include "rgb_reactive.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* Must match the DT `label` on the rgb_reactive behavior node and
 * fit within ZMK_SPLIT_RUN_BEHAVIOR_DEV_LEN (9 incl. NUL = 8 chars
 * max). */
#define BEHAVIOR_NAME "rgb_rx"

/*
 * Position → (half, local key index) lookup. The Corne keymap matrix
 * transform (see Corne.dtsi) gives:
 *   pos 0..5    row 0 left          → local 0..5
 *   pos 6..11   row 0 right         → local 0..5
 *   pos 12..17  row 1 left          → local 6..11
 *   pos 18..23  row 1 right         → local 6..11
 *   pos 24..29  row 2 left          → local 12..17
 *   pos 30..35  row 2 right         → local 12..17
 *   pos 36..38  row 3 left thumbs   → local 18..20
 *   pos 39..41  row 3 right thumbs  → local 18..20
 *
 * `local` here is the per-half index (0..20). The chain LED index is
 * 6 + local (the first 6 LEDs in the chain are underglow).
 *
 * The mapping from local index to physical LED in the daisy chain depends
 * on the PandaKB Corne v3 MX routing — if your LEDs don't light up under
 * the expected key, edit the LOCAL_TO_CHAIN table below.
 */
static const uint8_t LOCAL_TO_CHAIN[21] = {
    6,  7,  8,  9,  10, 11,   /* row 0 */
    12, 13, 14, 15, 16, 17,   /* row 1 */
    18, 19, 20, 21, 22, 23,   /* row 2 */
    24, 25, 26                /* thumbs */
};

static bool resolve_position(uint32_t pos, uint8_t *out_half, uint8_t *out_local) {
    uint8_t half, local;
    if (pos < 6)        { half = RGB_RX_HALF_CENTRAL; local = (uint8_t)(pos); }
    else if (pos < 12)  { half = RGB_RX_HALF_PERIPH;  local = (uint8_t)(pos - 6); }
    else if (pos < 18)  { half = RGB_RX_HALF_CENTRAL; local = (uint8_t)(pos - 6); }
    else if (pos < 24)  { half = RGB_RX_HALF_PERIPH;  local = (uint8_t)(pos - 12); }
    else if (pos < 30)  { half = RGB_RX_HALF_CENTRAL; local = (uint8_t)(pos - 12); }
    else if (pos < 36)  { half = RGB_RX_HALF_PERIPH;  local = (uint8_t)(pos - 18); }
    else if (pos < 39)  { half = RGB_RX_HALF_CENTRAL; local = (uint8_t)(pos - 18); }
    else if (pos < 42)  { half = RGB_RX_HALF_PERIPH;  local = (uint8_t)(pos - 21); }
    else                { return false; }
    if (local >= ARRAY_SIZE(LOCAL_TO_CHAIN)) return false;
    *out_half = half;
    *out_local = LOCAL_TO_CHAIN[local];
    return true;
}

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

/* Layer changes. Always broadcast the new layer — the controller
 * tracks layer independently of mode, so a SET_LAYER queued during
 * IDLE just updates the resting color that will be drawn on the next
 * ACTIVE transition. (Previously this was gated on zmk_activity_get_state,
 * but events can race ahead of the activity tracker and get dropped.) */
static int layer_listener(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    uint8_t layer = (uint8_t)zmk_keymap_highest_layer_active();
    invoke(RGB_RX_CMD_SET_LAYER, layer);
    return 0;
}
ZMK_LISTENER(rgb_reactive_layer, layer_listener);
ZMK_SUBSCRIPTION(rgb_reactive_layer, zmk_layer_state_changed);

/* Activity transitions (ACTIVE ↔ IDLE/SLEEP). */
static int activity_listener(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev == NULL) return 0;
    if (ev->state == ZMK_ACTIVITY_ACTIVE) {
        invoke(RGB_RX_CMD_SET_MODE, RGB_RX_MODE_ACTIVE);
        invoke(RGB_RX_CMD_SET_LAYER, (uint8_t)zmk_keymap_highest_layer_active());
    } else {
        invoke(RGB_RX_CMD_SET_MODE, RGB_RX_MODE_IDLE);
    }
    return 0;
}
ZMK_LISTENER(rgb_reactive_activity, activity_listener);
ZMK_SUBSCRIPTION(rgb_reactive_activity, zmk_activity_state_changed);

/* Key presses → per-key flash, BASE layer only. We deliberately do
 * NOT gate on zmk_activity_get_state(): the position event fires
 * before the activity tracker flips to ACTIVE on wake-from-idle, so
 * the filter dropped every legitimate keypress. The controller
 * itself ignores queued flashes while in IDLE mode (its render
 * function draws the swirl, not flashes), so racing through a stale
 * IDLE state is harmless — by the time the next render runs the
 * mode change has already arrived. */
static int position_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL || !ev->state) return 0;
    if (zmk_keymap_highest_layer_active() != 0) return 0;   /* BASE only */

    uint8_t half, chain_led;
    if (!resolve_position(ev->position, &half, &chain_led)) return 0;
    invoke(RGB_RX_CMD_FLASH, RGB_RX_FLASH_PACK(half, chain_led));
    return 0;
}
ZMK_LISTENER(rgb_reactive_position, position_listener);
ZMK_SUBSCRIPTION(rgb_reactive_position, zmk_position_state_changed);

/* Initial broadcast — wait for the peripheral split link to be up before
 * we start sending behavior invocations (rgb_reactive is GLOBAL-locality,
 * broadcasts only reach already-connected peripherals). 3s matches the
 * delay the previous rgb_ug-based implementation used successfully. */
static void boot_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    invoke(RGB_RX_CMD_SET_MODE, RGB_RX_MODE_ACTIVE);
    invoke(RGB_RX_CMD_SET_LAYER, (uint8_t)zmk_keymap_highest_layer_active());
}
K_WORK_DELAYABLE_DEFINE(boot_work, boot_work_handler);

static int rgb_reactive_central_init(const struct device *dev) {
    ARG_UNUSED(dev);
    k_work_schedule(&boot_work, K_MSEC(3000));
    return 0;
}
SYS_INIT(rgb_reactive_central_init, APPLICATION, 90);
