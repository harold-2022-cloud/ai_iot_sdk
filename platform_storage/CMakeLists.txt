if(COMMAND armino_component_register)
    armino_component_register(
        SRCS "platform_storage.c" "platform_storage_proxy.c" "exec_sync.c"
        INCLUDE_DIRS "."
        REQUIRES chip_bk7258 platform_os
    )
else()
# 芯片後端中立的編排核心（所有 IDF 目標都编）。
# 註：proxy/exec_sync 目前仍 #include "esp_heap_caps.h" 直呼 heap_caps_*，故尚未對非-ESP 芯片
#     完全中立；ESP32S3 目標與 host(mock heap)均可編。徹底 HAL 化留待真正新增別芯片時再做。
set(srcs "platform_storage.c" "platform_storage_proxy.c" "exec_sync.c")

# ESP32S3 专属 backend / worker：仅在该芯片目标编（非 ESP32S3 默认不编）
if(CONFIG_IDF_TARGET_ESP32S3)
    list(APPEND srcs "platform_storage_esp32_nvs.c" "exec_esp32_worker.c")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
    PRIV_REQUIRES heap nvs_flash platform_os
)
endif()
