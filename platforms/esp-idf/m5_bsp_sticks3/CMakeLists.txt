idf_component_register(
    SRCS "src/board.c" "src/display.c" "src/pm1.c" "src/speaker.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_lcd
    PRIV_REQUIRES esp_driver_i2s esp_driver_ledc esp_driver_spi heap freertos
    REQUIRED_IDF_TARGETS esp32s3
)
# The repository warning rule. m5_strict_warnings comes from m5_pocket_app when
# the program uses it; the BSP itself does not depend on PocketJS.
if(COMMAND m5_strict_warnings)
    m5_strict_warnings(${COMPONENT_LIB})
else()
    target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror
        -Wunused-parameter -Wsign-compare -Wenum-conversion)
endif()
