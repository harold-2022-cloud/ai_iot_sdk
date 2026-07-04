set(incs .
	entity_iot
	entity_protocol 
	entity_common
	entity_app
	entity_ble
	entity_wifi
	entity_mqtt
	entity_uart
	entity_udp
	entity_timer_countdown
	entity_network
	entity_http
	entity_http_ota
	factory_test
	entity_io_control
)
set(srcs "")

#配置RINO SDK源文件路径
SET(ENTITY_IOT_SDK_FILES_DIR
	entity_app
	entity_ble
	entity_common
	entity_iot
	entity_mqtt
	entity_protocol 
	entity_uart
	entity_wifi
	entity_udp
	entity_timer_countdown
	entity_network
	entity_http
	entity_http_ota
	factory_test
	entity_io_control
)


FOREACH(DIR ${ENTITY_IOT_SDK_FILES_DIR})
    # 添加C文件
    FILE(GLOB_RECURSE files ${DIR}/*.c)
    list(APPEND srcs ${files})
ENDFOREACH()
list(APPEND srcs entity_mqtt/entity_report.c)
list(REMOVE_DUPLICATES srcs)

if(COMMAND armino_component_register)
    # armino(BK7258)：中立 core 编同一份源码。
    #
    # 依赖方向铁则（docs/porting/entity_porting_contract.md §Forbidden Dependencies）：
    #   core 绝不 REQUIRES chip_bk7258 / chip_esp32s3 / platform_adapter。
    #   早期为取 ai_dialog_diag.h 曾 REQUIRES chip_bk7258（且 chip_bk7258 反向 REQUIRES
    #   entity_iot_sdk → 成环），已修正：no-op 诊断 facade 默认实现内置于 core 自身
    #   compat_armino/ai_dialog_diag.h（仅本 armino 分支加入 INCLUDE_DIRS，不污染 esp32 分支
    #   —— esp32 仍由 platform_adapter 取真实 ai_dialog_diag.h）。
    #   连结期 core 引用的 Entity_* 符号由 app 引入的 chip_bk7258 提供（镜像 esp32：
    #   chip 后端 REQUIRES core，app 同时引入两者；core 不反向依赖芯片后端）。
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "${incs}" "compat_armino"
        REQUIRES mi_mqtt json psa_mbedtls
    )
else()
# 依赖边界（P6.1 — 记录，不移除）：
#   json / mbedtls       —— 中立第三方。
#   mi_mqtt              —— 仅 facade header（无 SRCS），中立。
#   platform_adapter     —— 【唯一平台耦合】保留仅为 entity_mqtt 两个文件 include 的
#                           "ai_dialog_diag.h"（诊断头）。core 不调用任何 Bsp_*，不含其它平台头。
#       为何不拔：alarm 经 platform_adapter→ai_session_diag 取该头；xiaozhi 由其 vendored
#       platform_adapter 自带副本——两产品提供方式分叉，直接换成 REQUIRES ai_session_diag
#       会让 xiaozhi 编译失败。故 P6.1 延期移除，改由 test/rino_portability_guard.sh 的
#       「platform_adapter 桥接头」检查冻结此耦合面（ai_dialog_diag.h 仅限那两个文件，
#       其它 platform_adapter 头一律禁止），防止扩散。
#   依赖方向铁则仍满足：core 绝不反向 REQUIRES chip_esp32s3（芯片后端）。
idf_component_register(
	SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
    REQUIRES json mi_mqtt mbedtls platform_adapter
)
endif()
