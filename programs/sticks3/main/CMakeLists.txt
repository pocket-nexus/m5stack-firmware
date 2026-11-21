idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES m5_pocket_app m5_bsp_sticks3 m5_sticks3_net
    PRIV_REQUIRES esp_app_format esp_timer freertos heap log
)
m5_strict_warnings(${COMPONENT_LIB})

m5_pocket_app(
    TARGET ${COMPONENT_LIB}
    NAME sticks3_factory
    HOST_CONFIG "../../../apps/sticks3-factory/pocket.host.config.ts")
