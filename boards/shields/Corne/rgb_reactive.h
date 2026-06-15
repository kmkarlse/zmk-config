/*
 * Custom RGB controller for PandaKB Corne v3 MX.
 *
 * Replaces ZMK's built-in rgb_underglow on this shield. Both halves run an
 * identical 50ms render loop over the local 27-LED chain (6 underglow then
 * 21 per-key). The active layer drives what's drawn:
 *   layer 0 (BASE) → continuous swirl
 *   other layers   → solid layer color
 *
 * Layer state is broadcast from central via a BEHAVIOR_LOCALITY_GLOBAL
 * behavior (see behavior_rgb_reactive.c) so changes auto-propagate over
 * split BLE.
 */
#pragma once

#include <stdint.h>

/* Behavior commands, packed into binding param1 / param2. */
#define RGB_RX_CMD_SET_LAYER 0
#define RGB_RX_CMD_TOGGLE    1   /* param2 ignored; flips strip on/off */

void rgb_reactive_set_layer(uint8_t layer);

/* Flip the strip on/off. Off = blanked + tick stopped until toggled back on
 * (independent of the idle/sleep blanking). Called on both halves via the
 * GLOBAL behavior so one keypress toggles both. */
void rgb_reactive_toggle(void);
