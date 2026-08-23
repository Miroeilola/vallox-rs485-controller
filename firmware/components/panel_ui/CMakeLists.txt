idf_component_register(SRCS "font.c" "fonts/font_inter_12.c" "fonts/font_inter_18.c" "fonts/font_inter_36.c"
                            "gfx.c" "icons.c" "texts.c" "texts_en.c" "texts_fi.c"
                            "buttons.c" "vlx_client.c" "pages.c" "panel_ui.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "."
                       REQUIRES vallox_protocol panel_hal)
