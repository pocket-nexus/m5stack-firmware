idf_component_register(
    SRCS
        "src/audio.c"
        "src/board.c"
        "src/cst3530.c"
        "src/display.c"
        "src/ethernet.c"
        "src/imu.c"
        "src/ioe1.c"
        "src/pm1.c"
        "src/power.c"
        "src/rtc.c"
        "src/st7102.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_eth esp_lcd
    PRIV_REQUIRES esp_driver_i2s esp_hw_support esp_system esp_timer hal log
    REQUIRED_IDF_TARGETS esp32p4
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
# A program on m5_pocket_app restores the warnings IDF relaxes.
if(COMMAND m5_strict_warnings)
    m5_strict_warnings(${COMPONENT_LIB})
endif()
