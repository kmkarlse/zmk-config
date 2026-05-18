/*
 * Custom RGB controller for PandaKB Corne v3 MX.
 *
 * Replaces ZMK's built-in rgb_underglow on this shield. Both halves run an
 * identical 50ms render loop over the local 27-LED chain (6 underglow then
 * 21 per-key). State is driven from central via a BEHAVIOR_LOCALITY_GLOBAL
 * behavior (see behavior_rgb_reactive.c) so changes auto-propagate over the
 * split BLE link.
 */
#pragma once

#include <stdint.h>

/* Behavior command codes, packed into binding param1. */
#define RGB_RX_CMD_SET_LAYER 0
#define RGB_RX_CMD_SET_MODE  1
#define RGB_RX_CMD_FLASH     2

/* Modes for RGB_RX_CMD_SET_MODE (param2). */
#define RGB_RX_MODE_ACTIVE 0
#define RGB_RX_MODE_IDLE   1

/* Half identifiers for RGB_RX_CMD_FLASH payload. */
#define RGB_RX_HALF_CENTRAL 0
#define RGB_RX_HALF_PERIPH  1

/* Pack/unpack flash payload: high byte = half, low byte = local LED index. */
#define RGB_RX_FLASH_PACK(half, led) (((uint32_t)(half) << 8) | (uint8_t)(led))
#define RGB_RX_FLASH_HALF(payload)   (((payload) >> 8) & 0xFF)
#define RGB_RX_FLASH_LED(payload)    ((payload) & 0xFF)

void rgb_reactive_set_layer(uint8_t layer);
void rgb_reactive_set_mode(uint8_t mode);
void rgb_reactive_flash(uint8_t chain_led_index);
