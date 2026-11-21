idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES m5_pocket_app m5_bsp_sticks3
    PRIV_REQUIRES esp_hw_support esp_timer freertos heap log nvs_flash
)
m5_strict_warnings(${COMPONENT_LIB})

m5_pocket_app(
    TARGET ${COMPONENT_LIB}
    NAME sticks3_demos
    HOST_CONFIG "../../../apps/sticks3-demos/pocket.host.config.ts")
