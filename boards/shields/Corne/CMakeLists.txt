# Custom RGB controller. Replaces the built-in rgb_underglow on this
# shield. Split across three pieces with different build gates so the
# same source set serves the halves (peripherals, with strips) and the
# strip-less dongle (central).

# rgb_reactive.c — the strip controller; provides rgb_reactive_set_layer().
# Only where a physical strip exists, i.e. both halves.
if(CONFIG_WS2812_STRIP)
    target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/rgb_reactive.c)
endif()

# behavior_rgb_reactive.c — the global-locality behavior shim. Needed:
#   - on each strip-bearing half, to RECEIVE the broadcast and drive its
#     strip (calls rgb_reactive_set_layer);
#   - on the central/dongle, to FORWARD the broadcast to the peripherals
#     over split BLE, even though the dongle has no strip of its own.
# On the strip-less dongle the local set-layer call compiles out (see the
# CONFIG_WS2812_STRIP guard in the .c); the forward to peripherals still
# happens.
if(CONFIG_WS2812_STRIP OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL OR NOT CONFIG_ZMK_SPLIT)
    target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/behavior_rgb_reactive.c)
endif()

# layer_color.c — central-side coordinator. Uses
# zmk_keymap_highest_layer_active() / zmk_layer_state_changed, which ZMK
# only compiles into the central (see zmk/app/CMakeLists.txt). On the
# dongle this watches layer changes and fires the broadcast to both
# halves; the peripherals never run it.
if(CONFIG_ZMK_SPLIT_ROLE_CENTRAL OR NOT CONFIG_ZMK_SPLIT)
    target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/layer_color.c)
endif()
