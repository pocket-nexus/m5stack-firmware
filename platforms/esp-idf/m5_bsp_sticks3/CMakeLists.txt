idf_component_register(
    SRCS "src/board.c" "src/display.c" "src/pm1.c" "src/speaker.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_lcd
    PRIV_REQUIRES esp_driver_i2s esp_driver_ledc esp_driver_spi heap freertos
    REQUIRED_IDF_TARGETS esp32s3
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
