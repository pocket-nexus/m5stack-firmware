idf_component_register(
    SRCS
        "src/button.c"
        "src/display.c"
        "src/imu_mpu6886.c"
        "src/ir_nec.c"
        "src/panel_gc9107.c"
        "src/port_a.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_lcd
    PRIV_REQUIRES
        esp_adc
        esp_driver_ledc
        esp_driver_rmt
        esp_driver_spi
        esp_driver_uart
        esp_driver_usb_serial_jtag
        freertos
        log
    REQUIRED_IDF_TARGETS esp32s3
)
# The repository warning rule (AGENTS.md). m5_strict_warnings comes from
# m5_pocket_app's project_include.cmake; a program without it gets the plain
# flags, which IDF partly relaxes.
if(COMMAND m5_strict_warnings)
    m5_strict_warnings(${COMPONENT_LIB})
else()
    target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
endif()
