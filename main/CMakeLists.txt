idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES hx711 nextion weight nvs_flash
)
