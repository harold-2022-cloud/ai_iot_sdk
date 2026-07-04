if(COMMAND armino_component_register)
    armino_component_register(
        SRCS "platform_ota.c" "ota_exec_sync.c"
        INCLUDE_DIRS "."
        REQUIRES chip_bk7258 platform_os
    )
else()
# 芯片後端中立的編排核心（所有 IDF 目標都编）。
# 註：platform_ota.c 目前仍 #include "esp_heap_caps.h" 直呼 heap_caps_*，故尚未對非-ESP 芯片
#     完全中立；ESP32S3 目標與 host(mock heap)均可編。徹底 HAL 化留待真正新增別芯片時再做。
set(srcs "platform_ota.c" "ota_exec_sync.c")

# ESP32S3 专属 backend / worker：仅在该芯片目标编（非 ESP32S3 默认不编）
if(CONFIG_IDF_TARGET_ESP32S3)
    list(APPEND srcs "ota_exec_esp32_worker.c" "ota_backend_esp32.c")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
    PRIV_REQUIRES platform_os app_update spi_flash bootloader_support log
)
endif()
