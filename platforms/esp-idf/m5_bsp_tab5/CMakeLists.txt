idf_component_register(
    SRCS
        "src/audio.c"
        "src/board.c"
        "src/display.c"
        "src/esp_lcd_st7121.c"
        "src/esp_lcd_st7123.c"
        "src/imu.c"
        "src/interfaces.c"
        "src/power_monitor.c"
        "src/rtc.c"
        "src/sdcard.c"
        "src/bmi270/bmi2.c"
        "src/bmi270/bmi270.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src" "src/bmi270"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_lcd sdmmc
    PRIV_REQUIRES
        esp_driver_i2s
        esp_driver_ledc
        esp_driver_sdmmc
        esp_driver_tsens
        esp_driver_uart
        esp_hw_support
        esp_rom
        fatfs
        log
    REQUIRED_IDF_TARGETS esp32p4
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
# A program on m5_pocket_app restores the warnings IDF relaxes.
if(COMMAND m5_strict_warnings)
    m5_strict_warnings(${COMPONENT_LIB})
endif()
