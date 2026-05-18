# Custom RGB controller. Replaces the built-in rgb_underglow on this
# shield — built whenever WS2812 is present, on BOTH halves (peripheral
# also owns its local strip and renders its own pixels).
if(CONFIG_WS2812_STRIP)
    target_sources(app PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/rgb_reactive.c
        ${CMAKE_CURRENT_SOURCE_DIR}/behavior_rgb_reactive.c
    )

    # The central-side coordinator (layer_color.c) uses
    # zmk_keymap_highest_layer_active(), zmk_layer_state_changed, and
    # zmk_position_state_changed — all of which ZMK only compiles into
    # the central half (see zmk/app/CMakeLists.txt). The peripheral
    # learns about layer/mode/flash purely via the global-locality
    # behavior broadcast from central.
    if(CONFIG_ZMK_SPLIT_ROLE_CENTRAL OR NOT CONFIG_ZMK_SPLIT)
        target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/layer_color.c)
    endif()
endif()
