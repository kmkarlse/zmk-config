if(CONFIG_ZMK_RGB_UNDERGLOW)
    target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/layer_color.c)
endif()
