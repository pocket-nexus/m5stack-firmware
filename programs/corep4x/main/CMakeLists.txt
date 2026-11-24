idf_component_register(
    SRCS
        "audio_draw.c"
        "audio_dsp.c"
        "audio_music.c"
        "audio_player.c"
        "audio_rec.c"
        "imu_view.c"
        "ir_nec.c"
        "main.c"
        "net_ping.c"
        "services.c"
        "svc_audio.c"
        "svc_canvas.c"
        "svc_i2c.c"
        "svc_imu.c"
        "svc_ir.c"
        "svc_net.c"
        "svc_power.c"
        "svc_rtc.c"
        "svc_storage.c"
    INCLUDE_DIRS "."
    REQUIRES m5_pocket_app m5_bsp_corep4x
    PRIV_REQUIRES
        esp_app_format
        esp_driver_dma
        esp_driver_gpio
        esp_driver_rmt
        esp_driver_sdspi
        esp_driver_spi
        esp_eth
        esp_event
        esp_lcd
        esp_mm
        esp_netif
        esp_timer
        fatfs
        heap
        log
        lwip
        nvs_flash
        sdmmc
        spiffs
)

m5_strict_warnings(${COMPONENT_LIB})

# Face textures of the IMU window's device model (RGB565, read in place as
# uint16_t, so 4-byte aligned).
foreach(face front back top bottom left right)
    target_add_binary_data(${COMPONENT_LIB}
        "${CMAKE_CURRENT_LIST_DIR}/../assets/imu/corep4x_${face}.rgb565"
        BINARY ALIGN 4)
endforeach()

# The factory's embedded PCM (otool_audio/audio_playback_material, RIFF/WAVE
# 16-bit): the MUSIC track, mono 44.1 kHz 45 s, and the startup sound,
# stereo 16 kHz 4 s. The samples are read in place as int16_t.
target_add_binary_data(${COMPONENT_LIB}
    "${CMAKE_CURRENT_LIST_DIR}/../assets/audio/candy_wind_pcm_1ch_44.1k_16bit_45s.pcm"
    BINARY ALIGN 4 RENAME_TO candy_wind_wav)
target_add_binary_data(${COMPONENT_LIB}
    "${CMAKE_CURRENT_LIST_DIR}/../assets/audio/startup_pcm_2ch_16k_16bit_4s.pcm"
    BINARY ALIGN 4 RENAME_TO startup_wav)

# The application and its host config live in apps/corep4x-factory; the host
# contract is this program's pocket.host.json.
m5_pocket_app(
    TARGET ${COMPONENT_LIB}
    NAME corep4x
    HOST_CONFIG "../../../apps/corep4x-factory/pocket.host.config.ts"
    AOT_CRATE "../aot")
