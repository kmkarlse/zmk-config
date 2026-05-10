if(CONFIG_ZMK_RGB_UNDERGLOW)
    target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/layer_color.c)
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
