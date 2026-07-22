CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Wno-type-limits -Wno-unused-function
CFLAGS += -DBSP_SYSTEM_BK7258_ALLOCATOR_ONLY
CFLAGS += -I../../platform_os -Istubs -I../../media/rtc_facade/include -I../../media/rtc_facade/src -I../../media/rtc_facade/src/agora -I../../base64 -Ifakes
CFLAGS += -I../../entity_iot_sdk/entity_mqtt -I../../entity_iot_sdk/entity_iot -I../../library/mi_mqtt
CFLAGS += $(CFLAGS_EXTRA)

TARGET := test_bk7258_allocator_guard
SRCS := test_bk7258_allocator_guard.c ../../chip_bk7258/os/bsp_system_bk7258.c
RTC_CORE_TARGET := test_ai_rtc_facade_core
RTC_CORE_SRCS := test_ai_rtc_facade_core.c ../../media/rtc_facade/src/ai_rtc_facade.c fakes/ai_rtc_facade_fake_backend.c
RTC_AGORA_TARGET := test_ai_rtc_facade_agora_backend
RTC_AGORA_SRCS := test_ai_rtc_facade_agora_backend.c ../../media/rtc_facade/src/ai_rtc_facade.c ../../media/rtc_facade/src/ai_rtc_facade_agora_backend.c ../../media/rtc_facade/src/agora/ai_rtc_agora_service.c ../../media/rtc_facade/src/agora/ai_rtc_agora_control_datastream.c ../../media/rtc_facade/src/agora/ai_rtc_agora_datastream.c stubs/agora_rtc_stub.c stubs/ai_rtc_agora_port_stub.c stubs/cJSON_stub.c stubs/base_64_stub.c
RTC_AGORA_RTM_TARGET := test_ai_rtc_facade_agora_backend_rtm
RTC_AGORA_RTM_SRCS := test_ai_rtc_facade_agora_backend_rtm.c ../../media/rtc_facade/src/ai_rtc_facade.c ../../media/rtc_facade/src/ai_rtc_facade_agora_backend.c ../../media/rtc_facade/src/agora/ai_rtc_agora_service.c ../../media/rtc_facade/src/agora/ai_rtc_agora_control_datastream.c ../../media/rtc_facade/src/agora/ai_rtc_agora_control_rtm.c ../../media/rtc_facade/src/agora/ai_rtc_agora_datastream.c stubs/agora_rtc_stub.c stubs/ai_rtc_agora_port_stub.c stubs/cJSON_stub.c stubs/base_64_stub.c
RTC_AGORA_DATASTREAM_TARGET := test_ai_rtc_agora_datastream
RTC_AGORA_DATASTREAM_SRCS := test_ai_rtc_agora_datastream.c ../../media/rtc_facade/src/agora/ai_rtc_agora_datastream.c stubs/cJSON_stub.c stubs/base_64_stub.c
ENTITY_MQTT_EFFECTIVE_TARGET := test_entity_mqtt_effective_state
ENTITY_MQTT_EFFECTIVE_SRCS := test_entity_mqtt_effective_state.c ../../entity_iot_sdk/entity_mqtt/entity_mqtt_effective_state.c

.PHONY: all test check_entity_interface_reference_template check_ai_rtc_facade_public_header check_ai_rtc_facade_core_boundary check_ai_rtc_facade_sdk_agora_migration check_ai_rtc_control_transport_buildtime check_ai_rtc_diagnostic_markers check_entity_mqtt_effective_state_policy check_entity_mqtt_ai_deferred_publish check_entity_mqtt_allocator_pairing clean

all: $(TARGET) $(RTC_CORE_TARGET) $(RTC_AGORA_TARGET) $(RTC_AGORA_RTM_TARGET) $(RTC_AGORA_DATASTREAM_TARGET) $(ENTITY_MQTT_EFFECTIVE_TARGET)

test: $(TARGET) $(RTC_CORE_TARGET) $(RTC_AGORA_TARGET) $(RTC_AGORA_RTM_TARGET) $(RTC_AGORA_DATASTREAM_TARGET) $(ENTITY_MQTT_EFFECTIVE_TARGET) check_entity_interface_reference_template check_ai_rtc_facade_public_header check_ai_rtc_facade_core_boundary check_ai_rtc_facade_sdk_agora_migration check_ai_rtc_control_transport_buildtime check_ai_rtc_diagnostic_markers check_entity_mqtt_effective_state_policy check_entity_mqtt_ai_deferred_publish check_entity_mqtt_allocator_pairing
	./$(TARGET)
	./$(RTC_CORE_TARGET)
	./$(RTC_AGORA_TARGET)
	./$(RTC_AGORA_RTM_TARGET)
	./$(RTC_AGORA_DATASTREAM_TARGET)
	./$(ENTITY_MQTT_EFFECTIVE_TARGET)

check_entity_interface_reference_template:
	python3 check_entity_interface_reference_template.py

check_ai_rtc_facade_public_header:
	python3 check_ai_rtc_facade_public_header.py

check_ai_rtc_facade_core_boundary:
	python3 check_ai_rtc_facade_core_boundary.py

check_ai_rtc_facade_sdk_agora_migration:
	python3 check_ai_rtc_facade_sdk_agora_migration.py

check_ai_rtc_control_transport_buildtime:
	python3 check_ai_rtc_control_transport_buildtime.py

check_ai_rtc_diagnostic_markers:
	python3 check_ai_rtc_diagnostic_markers.py

check_entity_mqtt_effective_state_policy:
	python3 check_entity_mqtt_effective_state_policy.py

check_entity_mqtt_ai_deferred_publish:
	python3 check_entity_mqtt_ai_deferred_publish.py

check_entity_mqtt_allocator_pairing:
	python3 check_entity_mqtt_allocator_pairing.py

$(TARGET): $(SRCS) stubs/os/os.h stubs/os/mem.h
	$(CC) $(CFLAGS) $(SRCS) -o $@

$(RTC_CORE_TARGET): $(RTC_CORE_SRCS)
	$(CC) $(CFLAGS) $(RTC_CORE_SRCS) -o $@

$(RTC_AGORA_TARGET): $(RTC_AGORA_SRCS)
	$(CC) $(CFLAGS) $(RTC_AGORA_SRCS) -o $@

$(RTC_AGORA_RTM_TARGET): $(RTC_AGORA_RTM_SRCS)
	$(CC) $(CFLAGS) -DAI_RTC_AGORA_CONTROL_TRANSPORT=AI_RTC_AGORA_CONTROL_TRANSPORT_RTM $(RTC_AGORA_RTM_SRCS) -o $@

$(RTC_AGORA_DATASTREAM_TARGET): $(RTC_AGORA_DATASTREAM_SRCS)
	$(CC) $(CFLAGS) $(RTC_AGORA_DATASTREAM_SRCS) -o $@

$(ENTITY_MQTT_EFFECTIVE_TARGET): $(ENTITY_MQTT_EFFECTIVE_SRCS)
	$(CC) $(CFLAGS) $(ENTITY_MQTT_EFFECTIVE_SRCS) -o $@

clean:
	rm -f $(TARGET) $(RTC_CORE_TARGET) $(RTC_AGORA_TARGET) $(RTC_AGORA_RTM_TARGET) $(RTC_AGORA_DATASTREAM_TARGET) $(ENTITY_MQTT_EFFECTIVE_TARGET)
