idf_component_register(
    SRCS "src/net.c"
    INCLUDE_DIRS "include"
    EMBED_TXTFILES "src/portal.html"
    PRIV_REQUIRES esp_wifi esp_netif esp_event esp_http_server esp_timer
                  nvs_flash lwip freertos log
    REQUIRED_IDF_TARGETS esp32s3
)
m5_strict_warnings(${COMPONENT_LIB})
