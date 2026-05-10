if(CONFIG_ZMK_RGB_UNDERGLOW)
    # layer_color.c uses zmk_keymap_highest_layer_active() and the
    # zmk_layer_state_changed event, both of which ZMK only compiles
    # into the central half (see zmk/app/CMakeLists.txt). Build only
    # on central; peripheral keeps its local default RGB.
    if(CONFIG_ZMK_SPLIT_ROLE_CENTRAL OR NOT CONFIG_ZMK_SPLIT)
        target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/layer_color.c)
    endif()
endif()

# Bongo cat status screen — peripheral half only.
# Central keeps the default ZMK status screen (layer + output widgets).
if(CONFIG_ZMK_DISPLAY AND CONFIG_ZMK_SPLIT AND NOT CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    target_sources(app PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/custom_status_screen.c
        ${CMAKE_CURRENT_SOURCE_DIR}/widgets/bongo_cat.c
        ${CMAKE_CURRENT_SOURCE_DIR}/widgets/bongo_cat_images.c
    )
    target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
