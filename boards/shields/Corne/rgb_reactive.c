/*
 * Strip controller. See rgb_reactive.h for protocol overview.
 *
 * Owns the led_strip device on both halves. Renders the active layer
 * every 50ms: BASE (layer 0) animates as a swirl, every other layer
 * is solid. Layer state is updated via rgb_reactive_set_layer, called
 * from behavior_rgb_reactive.c on both halves.
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
#define BASE_BRIGHTNESS   60   /* % — matches former CONFIG_..._BRT_START=60 */
/* ZMK's swirl runs at speed*2 hue/tick at 50ms ticks. Default speed=3 →
 * 6 hue/50ms → 360° in 3000ms. Match that. */
#define SWIRL_PERIOD_MS   3000

struct layer_color { uint16_t h; uint8_t s; uint8_t b; };
static const struct layer_color layer_colors[] = {
    { 182, 73,  BASE_BRIGHTNESS },  /* 0 BASE = cyan (RGB 66,239,245) — swirl uses S/V */
    { 120, 100, BASE_BRIGHTNESS },  /* 1 SYM  = green */
    {   0, 100, BASE_BRIGHTNESS },  /* 2 NUM  = red */
    {  30, 100, BASE_BRIGHTNESS },  /* 3 L3   = orange */
};

static const struct device *led_strip = DEVICE_DT_GET(STRIP_NODE);

static struct led_rgb pixels[STRIP_NUM_PIXELS];
static struct k_spinlock state_lock;

static uint8_t current_layer = 0;

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

static void render(void) {
    int64_t now = k_uptime_get();
    k_spinlock_key_t key = k_spin_lock(&state_lock);
    uint8_t layer = current_layer;
    k_spin_unlock(&state_lock, key);

    const struct layer_color *lc = &layer_colors[layer % ARRAY_SIZE(layer_colors)];

    if (layer == 0) {
        /* BASE: swirl using the layer's S/V. */
        uint16_t base_hue = (uint16_t)((now * 360 / SWIRL_PERIOD_MS) % 360);
        for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
            uint16_t h = (base_hue + (i * 360) / STRIP_NUM_PIXELS) % 360;
            pixels[i] = hsb_to_rgb(h, lc->s, lc->b);
        }
    } else {
        struct led_rgb base = hsb_to_rgb(lc->h, lc->s, lc->b);
        for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
            pixels[i] = base;
        }
    }

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
