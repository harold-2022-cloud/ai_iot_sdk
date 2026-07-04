# chip_bk7258 — BK7258 chip backend (armino component)
#
# 顯式 bind → 不需 WHOLE_ARCHIVE：app 主動呼叫 Entity_Chip_Bk7258_Bind()，
# 連結器不會丟棄被引用的賦值函式。
#
# 注：以下 SRCS 前向列出全部 10 個規劃檔案，但它們橫跨 Phase 1-6 才逐步建立；
#     armino 直到 Phase 8 才實際建構，故此處前向列出無妨。Task 1.1 只落地
#     os/bsp_system_bk7258.c，其餘檔案於後續 Phase 補上。
set(srcs
    "os/bsp_system_bk7258.c"          # Task 1.1（本次）：記憶體 + 時間/雜項
    "os/bsp_sync_bk7258.c"            # Task 1.2/Phase2：mutex/sema/queue/critical
    "os/bsp_dma_bk7258.c"             # Phase2：DMA
    "os/bsp_log_bk7258.c"             # Phase2：log
    "chip_bk7258_bind.c"             # 顯式 bind 入口實作
    "bind/entity_hal_binding_bk7258.c"  # HAL 函數指針賦值
    "bind/entity_system_bind_bk7258.c"  # system/cJSON hooks 賦值
    "storage/storage_ops_bk7258.c"   # Phase：NVS/flash storage
    "ota/ota_ops_bk7258.c"           # Phase：OTA
    # coreMQTT engine (Task 5.1a): vendored verbatim from rino mi_mqtt.
    # Lives under chip_bk7258 because ESP32 uses esp-mqtt instead — not chip-neutral.
    "mqtt/mi_mqtt.c"                 # coreMQTT engine core
    "mqtt/mi_mqtt_serializer.c"      # coreMQTT packet serializer
    "mqtt/mi_mqtt_state.c"           # coreMQTT QoS state machine
    "mqtt/mi_mqtt_client_bk7258.c"   # Phase：mi_mqtt client (Task 5.1b)
)

if(COMMAND armino_component_register)
    # ../platform_os|storage|ota：chip_bk7258 的 .c 直接 #include 中立層的契約頭
    # （bsp_system.h / bsp_sync.h / bsp_dma.h / bsp_log.h / bsp_status.h /
    #  bsp_network.h / bsp_timer_countdown.h / bsp_atomic.h / platform_storage.h /
    #  platform_ota.h）。不能用 REQUIRES platform_os —— platform_os 的 armino 分支
    #  REQUIRES chip_bk7258（取 Bsp_* 符號連結），互相 REQUIRES 會成環。故改以
    #  INCLUDE_DIRS 直接給 chip_bk7258 中立層的標頭可見性（只給編譯期 -I,無 REQUIRES 邊,不成環）。
    #
    # 注：原 "compat"（ai_dialog_diag.h shim）已移除 —— chip_bk7258 自身無檔 #include 該頭，
    #     它僅為餵 entity core 而存在，現已內置於 entity_iot_sdk/compat_armino/（見該 CMakeLists 註解）。
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "." "include" "mqtt" "os"
                     "../platform_os" "../platform_storage" "../platform_ota"
        REQUIRES entity_iot_sdk mi_mqtt
        PRIV_REQUIRES bk_common easy_flash psa_mbedtls bk_system lwip_intf_v2_1 ota json
    )
endif()
