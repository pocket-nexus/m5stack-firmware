idf_component_register(
  SRCS
    "src/audio.c"
    "src/bmi270.c"
    "src/bmi270_config.c"
    "src/board.c"
    "src/co5300.c"
    "src/cst820.c"
    "src/ioe1.c"
    "src/pm1.c"
    "src/refresh.c"
    "src/rx8130.c"
    "src/vibrate.c"
  INCLUDE_DIRS "include"
  PRIV_INCLUDE_DIRS "src"
  REQUIRES esp_driver_i2c esp_driver_gpio esp_lcd esp_driver_spi
  PRIV_REQUIRES esp_timer esp_driver_i2s
  REQUIRED_IDF_TARGETS esp32s3
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
# IDF relaxes -Wextra; m5_pocket_app's helper restores the strict set when
# the program uses the shared application host.
if(COMMAND m5_strict_warnings)
  m5_strict_warnings(${COMPONENT_LIB})
endif()
