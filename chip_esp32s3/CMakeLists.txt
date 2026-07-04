idf_component_register(
    SRCS
        "chip_esp32s3_bind.c"
        "bind/entity_hal_binding_esp32s3.c"
        "bind/entity_system_bind_esp32s3.c"
        "mqtt/mi_mqtt_client_esp32.c"
        "rtc/ai_rtc_agora_port_esp32.c"
        "rtc/ai_rtc_agora_aosl_psram_wrap.c"
    INCLUDE_DIRS
        "."
        "rtc"
        "../media/rtc_facade/src/agora"
    REQUIRES entity_iot_sdk
    PRIV_REQUIRES lwip esp_common heap mi_mqtt mqtt platform_adapter esp_timer log freertos esp_event esp_netif esp_wifi
    WHOLE_ARCHIVE
)

set(ai_rtc_agora_aosl_wrap_options
    "-Wl,--wrap=aosl_hal_malloc"
    "-Wl,--wrap=aosl_hal_calloc"
    "-Wl,--wrap=aosl_hal_realloc"
    "-Wl,--wrap=aosl_hal_free"
    "-Wl,-u,__wrap_aosl_hal_malloc"
    "-Wl,-u,__wrap_aosl_hal_calloc"
    "-Wl,-u,__wrap_aosl_hal_realloc"
    "-Wl,-u,__wrap_aosl_hal_free"
)

if(DEFINED COMPONENT_LIB AND NOT "${COMPONENT_LIB}" STREQUAL "" AND TARGET "${COMPONENT_LIB}")
    target_link_options(${COMPONENT_LIB} INTERFACE ${ai_rtc_agora_aosl_wrap_options})
elseif(DEFINED COMPONENT_TARGET AND NOT "${COMPONENT_TARGET}" STREQUAL "" AND TARGET "${COMPONENT_TARGET}")
    target_link_options(${COMPONENT_TARGET} INTERFACE ${ai_rtc_agora_aosl_wrap_options})
endif()
