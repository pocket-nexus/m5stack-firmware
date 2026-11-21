idf_component_register(
    SRCS "main.c" "atoms3_device.c"
    INCLUDE_DIRS "."
    REQUIRES m5_pocket_app
    PRIV_REQUIRES
        esp_event
        esp_hw_support
        esp_netif
        esp_timer
        esp_wifi
        heap
        log
        m5_bsp_atoms3
        nvs_flash
)
m5_strict_warnings(${COMPONENT_LIB})

m5_pocket_app(
    TARGET ${COMPONENT_LIB}
    NAME atoms3
    HOST_CONFIG "../../../apps/atoms3-factory/pocket.host.config.ts")
