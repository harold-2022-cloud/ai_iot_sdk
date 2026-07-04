if(COMMAND armino_component_register)
    # 只有真正中立的 .c 在 BK 编；OS 实作层(bsp_system/sync/dma/isr/log)由 chip_bk7258/os 重写
    armino_component_register(
        SRCS "bsp_network.c" "bsp_timer_countdown.c" "bsp_spsc_ring.c"
        INCLUDE_DIRS "."
        REQUIRES chip_bk7258
        PRIV_REQUIRES psa_mbedtls lwip_intf_v2_1
    )
else()
idf_component_register(
    SRCS
        "bsp_log.c"
        "bsp_network.c"
        "bsp_system.c"
        "bsp_timer_countdown.c"
        "bsp_sync.c"
        "bsp_isr.c"
        "bsp_dma.c"
        "bsp_spsc_ring.c"
    INCLUDE_DIRS "."
    REQUIRES esp_common
    PRIV_REQUIRES
        log
        esp_rom
        esp_system
        esp_timer
        esp_hw_support
        mbedtls
        lwip
)
endif()
