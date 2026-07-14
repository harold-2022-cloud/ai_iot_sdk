set(srcs
    src/ai_rtc_facade.c
    src/ai_rtc_facade_agora_backend.c
    src/agora/ai_rtc_agora_service.c
    src/agora/ai_rtc_agora_control_datastream.c
    src/agora/ai_rtc_agora_control_rtm.c
    src/agora/ai_rtc_agora_datastream.c
)

set(incs
    include
    src
    src/agora
)

if(COMMAND armino_component_register)
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "${incs}"
        PRIV_REQUIRES ai_iot_bk7258_agora_iot_sdk json
    )
endif()

if(COMMAND idf_component_register)
    idf_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "${incs}"
        REQUIRES chip_esp32s3
        PRIV_REQUIRES agora_iot_sdk json
    )
endif()
