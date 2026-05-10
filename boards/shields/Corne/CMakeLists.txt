if(CONFIG_ZMK_RGB_UNDERGLOW)
    # layer_color.c uses zmk_keymap_highest_layer_active() and the
    # zmk_layer_state_changed event, both of which ZMK only compiles
    # into the central half (see zmk/app/CMakeLists.txt). Build only
    # on central; layer color is propagated to the peripheral via the
    # rgb_ug global behavior over split BLE (see layer_color.c).
    if(CONFIG_ZMK_SPLIT_ROLE_CENTRAL OR NOT CONFIG_ZMK_SPLIT)
        target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/layer_color.c)
    endif()
endif()
