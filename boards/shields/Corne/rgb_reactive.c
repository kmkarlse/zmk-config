/*
 * Strip controller. See rgb_reactive.h for protocol overview.
 *
 * Owns the led_strip device on both halves. Runs a periodic render that
 * composites a base layer color, per-key flash overlays, and an idle swirl.
 * Public functions are invoked by behavior_rgb_reactive.c on both halves.
 */
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/logging/log.h>

#include "rgb_reactive.h"

LOG_MODULE_REGISTER(rgb_reactive, CONFIG_ZMK_LOG_LEVEL);

#define STRIP_NODE        DT_CHOSEN(zmk_underglow)
#define STRIP_NUM_PIXELS  DT_PROP(STRIP_NODE, chain_length)

#define TICK_MS           50
#define FLASH_MS          300
#define FLASH_SLOTS       16
#define FLASH_MAX_ADD     200  /* additive overlay on each channel at t=0 */
#define BASE_BRIGHTNESS   60   /* % — matches former CONFIG_..._BRT_START=60 */
#define IDLE_HUE_PERIOD_MS 12000

struct layer_color { uint16_t h; uint8_t s; uint8_t b; };
static const struct layer_color layer_colors[] = {
    { 182, 73,  BASE_BRIGHTNESS },  /* 0 BASE = cyan (RGB 66,239,245) */
    { 120, 100, BASE_BRIGHTNESS },  /* 1 SYM  = green */
    {   0, 100, BASE_BRIGHTNESS },  /* 2 NUM  = red */
    {  30, 100, BASE_BRIGHTNESS },  /* 3 L3   = orange */
};

struct flash_slot {
    uint8_t led;
    int64_t start_ms;
    bool active;
};

static const struct device *led_strip = DEVICE_DT_GET(STRIP_NODE);

static struct led_rgb pixels[STRIP_NUM_PIXELS];
static struct flash_slot flashes[FLASH_SLOTS];
static struct k_spinlock state_lock;

static uint8_t current_layer = 0;
static uint8_t current_mode = RGB_RX_MODE_ACTIVE;

/* HSV → RGB. h 0..359, s/v 0..100. Output 0..255 per channel. */
static struct led_rgb hsb_to_rgb(uint16_t h, uint8_t s, uint8_t v) {
    uint8_t r = 0, g = 0, b = 0;
    uint32_t vv = (uint32_t)v * 255 / 100;
    uint32_t sat = (uint32_t)s * 255 / 100;
    if (sat == 0) {
        r = g = b = (uint8_t)vv;
    } else {
        uint16_t hi = (h / 60) % 6;
        uint16_t f = (h % 60) * 1000 / 60;  /* 0..999 */
        uint32_t p = vv * (255 - sat) / 255;
        uint32_t q = vv * (255 - (sat * f) / 1000) / 255;
        uint32_t t = vv * (255 - (sat * (1000 - f)) / 1000) / 255;
        switch (hi) {
            case 0: r = vv; g = t;  b = p;  break;
            case 1: r = q;  g = vv; b = p;  break;
            case 2: r = p;  g = vv; b = t;  break;
            case 3: r = p;  g = q;  b = vv; break;
            case 4: r = t;  g = p;  b = vv; break;
            default: r = vv; g = p; b = q;  break;
        }
    }
    return (struct led_rgb){ .r = r, .g = g, .b = b };
}

static inline uint8_t add_sat(uint8_t a, uint32_t add) {
    uint32_t s = (uint32_t)a + add;
    return s > 255 ? 255 : (uint8_t)s;
}

static void render(void) {
    int64_t now = k_uptime_get();
    k_spinlock_key_t key = k_spin_lock(&state_lock);

    uint8_t mode = current_mode;
    uint8_t layer = current_layer;

    if (mode == RGB_RX_MODE_IDLE) {
        uint16_t base_hue = (uint16_t)((now * 360 / IDLE_HUE_PERIOD_MS) % 360);
        for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
            uint16_t h = (base_hue + (i * 360) / STRIP_NUM_PIXELS) % 360;
            pixels[i] = hsb_to_rgb(h, 100, BASE_BRIGHTNESS);
        }
    } else {
        const struct layer_color *lc = &layer_colors[layer % ARRAY_SIZE(layer_colors)];
        struct led_rgb base = hsb_to_rgb(lc->h, lc->s, lc->b);
        for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
            pixels[i] = base;
        }
        for (int f = 0; f < FLASH_SLOTS; f++) {
            if (!flashes[f].active) continue;
            int64_t elapsed = now - flashes[f].start_ms;
            if (elapsed >= FLASH_MS) {
                flashes[f].active = false;
                continue;
            }
            uint8_t led = flashes[f].led;
            if (led >= STRIP_NUM_PIXELS) continue;
            uint32_t add = ((uint32_t)(FLASH_MS - elapsed) * FLASH_MAX_ADD) / FLASH_MS;
            pixels[led].r = add_sat(pixels[led].r, add);
            pixels[led].g = add_sat(pixels[led].g, add);
            pixels[led].b = add_sat(pixels[led].b, add);
        }
    }

    k_spin_unlock(&state_lock, key);

    int ret = led_strip_update_rgb(led_strip, pixels, STRIP_NUM_PIXELS);
    if (ret < 0) {
        LOG_WRN("led_strip_update_rgb returned %d", ret);
    }
}

static void tick_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(tick_work, tick_work_handler);

static void tick_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    render();
    k_work_schedule(&tick_work, K_MSEC(TICK_MS));
}

void rgb_reactive_set_layer(uint8_t layer) {
    k_spinlock_key_t key = k_spin_lock(&state_lock);
    current_layer = layer;
    k_spin_unlock(&state_lock, key);
}

void rgb_reactive_set_mode(uint8_t mode) {
    k_spinlock_key_t key = k_spin_lock(&state_lock);
    current_mode = mode;
    k_spin_unlock(&state_lock, key);
}

void rgb_reactive_flash(uint8_t led_index) {
    if (led_index >= STRIP_NUM_PIXELS) return;
    int64_t now = k_uptime_get();
    k_spinlock_key_t key = k_spin_lock(&state_lock);
    int target = -1;
    int64_t oldest = INT64_MAX;
    int oldest_idx = 0;
    for (int i = 0; i < FLASH_SLOTS; i++) {
        if (!flashes[i].active) { target = i; break; }
        if (flashes[i].start_ms < oldest) {
            oldest = flashes[i].start_ms;
            oldest_idx = i;
        }
    }
    if (target < 0) target = oldest_idx;
    flashes[target].led = led_index;
    flashes[target].start_ms = now;
    flashes[target].active = true;
    k_spin_unlock(&state_lock, key);
}

static int rgb_reactive_init(const struct device *dev) {
    ARG_UNUSED(dev);
    if (!device_is_ready(led_strip)) {
        LOG_ERR("led_strip device not ready");
        return -ENODEV;
    }
    k_work_schedule(&tick_work, K_MSEC(500));
    return 0;
}
SYS_INIT(rgb_reactive_init, APPLICATION, 90);
