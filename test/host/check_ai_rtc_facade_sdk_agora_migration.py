#!/usr/bin/env python3
"""Static checks for SDK-owned Agora RTC facade migration."""

from __future__ import annotations

import os
import re
import subprocess
from pathlib import Path


SDK = Path(__file__).resolve().parents[2]
ESP_PRODUCT = Path(os.environ.get("AI_RTC_ESP32S3_PRODUCT_DIR", ""))

PUBLIC_HEADER = SDK / "media/rtc_facade/include/ai_rtc_facade.h"
RTC_PORTING_CONTRACT = SDK / "docs/rtc-porting-contract.md"
PORTABLE_SDK_BASELINE = SDK / "docs/portable-sdk-baseline.md"
RTC_FACADE = SDK / "media/rtc_facade"
SDK_AGORA_DIR = RTC_FACADE / "src/agora"
SDK_VENDOR_AGORA = SDK / "chip_esp32s3/vendor/agora_iot_sdk"
ESP_PRODUCT_AGORA = ESP_PRODUCT / "ai_components/network_transfer/agora_rtc"
ESP_PRODUCT_NETWORK_CMAKE = ESP_PRODUCT / "ai_components/network_transfer/CMakeLists.txt"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace") if path.exists() else ""


def normalized_contains(text: str, token: str) -> bool:
    return " ".join(token.split()) in " ".join(text.split())


def archive_defined_symbols(path: Path) -> set[str]:
    if not path.exists():
        return set()
    try:
        result = subprocess.run(
            ["nm", "-g", "--defined-only", str(path)],
            check=False,
            capture_output=True,
            text=True,
        )
    except OSError:
        return set()
    symbols: set[str] = set()
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) >= 3:
            symbols.add(fields[-1])
    return symbols


def main() -> int:
    failures: list[str] = []

    required_sdk_files = (
        SDK_AGORA_DIR / "ai_rtc_agora_service.h",
        SDK_AGORA_DIR / "ai_rtc_agora_service.c",
        SDK_AGORA_DIR / "ai_rtc_agora_control_transport.h",
        SDK_AGORA_DIR / "ai_rtc_agora_datastream.h",
        SDK_AGORA_DIR / "ai_rtc_agora_datastream.c",
        SDK_AGORA_DIR / "ai_rtc_agora_control_datastream.c",
        SDK_AGORA_DIR / "ai_rtc_agora_control_rtm.c",
        SDK_AGORA_DIR / "ai_rtc_agora_port.h",
        SDK / "chip_esp32s3/rtc/ai_rtc_agora_port_esp32.c",
        SDK / "chip_esp32s3/rtc/ai_rtc_agora_aosl_psram_wrap.c",
        SDK_VENDOR_AGORA / "CMakeLists.txt",
        SDK_VENDOR_AGORA / "include/agora_rtc_api.h",
        SDK_VENDOR_AGORA / "libs/libagora-rtc-sdk.a",
        SDK_VENDOR_AGORA / "libs/libaosl.a",
    )
    for path in required_sdk_files:
        if not path.exists():
            failures.append(f"missing SDK Agora migration file: {path}")

    vendor_cmake = read(SDK_VENDOR_AGORA / "CMakeLists.txt")
    for token in (
        "add_prebuilt_library(rtsa",
        "libagora-rtc-sdk.a",
        "add_prebuilt_library(aosl",
        "libaosl.a",
        "target_link_libraries(${COMPONENT_LIB} INTERFACE rtsa aosl)",
    ):
        if token not in vendor_cmake:
            failures.append(f"SDK Agora vendor component CMake missing token: {token}")

    vendor_header = read(SDK_VENDOR_AGORA / "include/agora_rtc_api.h")
    for token in (
        "agora_rtm_handler_t",
        "rtm_event_type_e",
        "rtm_msg_state_e",
        "agora_rtc_login_rtm",
        "agora_rtc_logout_rtm",
        "agora_rtc_send_rtm_data",
    ):
        if token not in vendor_header:
            failures.append(f"SDK ESP32S3 Agora vendor header missing RTM-capable token: {token}")
    autoconfig = read(SDK_VENDOR_AGORA / "include/autoconfig.h")
    for token in (
        "#define CONFIG_RTM 1",
        "#define CONFIG_DATA_STREAM 1",
        '#define CONFIG_SDK_VERSION "1.10.0"',
    ):
        if token not in autoconfig:
            failures.append(f"SDK ESP32S3 Agora vendor autoconfig missing token: {token}")
    vendor_symbols = archive_defined_symbols(SDK_VENDOR_AGORA / "libs/libagora-rtc-sdk.a")
    for symbol in (
        "agora_rtc_login_rtm",
        "agora_rtc_logout_rtm",
        "agora_rtc_send_rtm_data",
        "agora_rtc_create_data_stream",
        "agora_rtc_send_stream_message",
    ):
        if symbol not in vendor_symbols:
            failures.append(f"SDK ESP32S3 Agora vendor archive missing symbol: {symbol}")

    public_text = read(PUBLIC_HEADER)
    forbidden_public_tokens = (
        "agora_rtc",
        "agora_iot",
        "connection_id_t",
        "audio_frame_info_t",
        "rtc_port",
        "FreeRTOS",
        "esp_",
        "bk_",
        "network_transfer",
        "dma",
        "resource",
        "heap",
        "lcd",
        "avi",
        "bluetooth",
        "ble_provision",
        "wifi",
        "wi-fi",
        "mqtt",
        "http",
        "button",
        "gpio",
        "codec",
        "ai_rtc_control_transport",
        "control_transport",
        "rtm",
    )
    public_lower = public_text.lower()
    for token in forbidden_public_tokens:
        if token.lower() in public_lower:
            failures.append(f"public facade header exposes forbidden token: {token}")
    forbidden_resource_policy_tokens = (
        "heap_caps",
        "MALLOC_CAP",
        "dma_largest",
        "internal_largest",
        "resource_snapshot",
        "resource_can_use",
        "UPLINK_READY",
        "uplink_ready",
    )
    for token in forbidden_resource_policy_tokens:
        if token in public_text:
            failures.append(f"public facade header leaked resource policy token: {token}")
    if "ERR_UNSUPPORTED" in re.sub(r"AI_RTC_FACADE_ERR_UNSUPPORTED", "", public_text):
        failures.append("public facade header must not describe video as unsupported")

    rtc_contract = read(RTC_PORTING_CONTRACT)
    for token in (
        "RTC audio, RTC video, datastream, session lifecycle",
        "Private backend code may support both Agora datastream and RTM",
        "Product / board | mic, speaker, codec, AEC, DMA, LCD, AVI/JPEG/display policy, buttons, BLE/Wi-Fi provisioning, HTTP/MQTT product flows",
        "RTM/datastream transport selector controls",
        "Lifecycle state meanings",
        "This is the public facade name for the LEAVING phase",
        "`connection_id_t` is the lifecycle key",
        "`uid` is the remote user or media/datastream sender identity",
        "Diagnostic Marker Contract",
        "hot-path logs must be throttled",
        "Beken BK7258 R1 visual workload note",
        "AVI/JPEG decode, DMA2D blit/wait, LCD",
    ):
        if not normalized_contains(rtc_contract, token):
            failures.append(f"RTC porting contract missing token: {token}")

    portable_baseline = read(PORTABLE_SDK_BASELINE)
    for token in (
        "Phase 7 RTC Vendor/AOSL Baseline",
        "SDK owns RTC audio/video/datastream/private RTM backend",
        "Product owns microphone, speaker, codec, AEC, DMA, LCD, AVI, button, BLE/Wi-Fi, HTTP/MQTT",
        "chip_esp32s3/vendor/agora_iot_sdk/libs/libagora-rtc-sdk.a",
        "chip_esp32s3/vendor/agora_iot_sdk/libs/libaosl.a",
        "chip_bk7258/vendor/aosl/libs/libaosl.a",
        "rand_bytes",
        "mbedtls",
    ):
        if not normalized_contains(portable_baseline, token):
            failures.append(f"portable SDK baseline missing RTC vendor token: {token}")

    facade_text = read(RTC_FACADE / "src/ai_rtc_facade.c")
    for token in (
        "AI_RTC_FACADE_CORE",
        "event=%d state=%d detail=%d joined=%d",
        "Ai_Rtc_Facade_State_Cb on_state = s_ctx.callbacks.on_state",
        "Ai_Rtc_Facade_Audio_Rx_Cb on_audio_rx = s_ctx.callbacks.on_audio_rx",
        "Ai_Rtc_Facade_Video_Rx_Cb on_video_rx = s_ctx.callbacks.on_video_rx",
        "Ai_Rtc_Facade_Datastream_Rx_Cb on_datastream_rx = s_ctx.callbacks.on_datastream_rx",
    ):
        if token not in facade_text:
            failures.append(f"RTC facade core missing diagnosability log token: {token}")
    facade_stop_match = re.search(
        r"int\s+Ai_Rtc_Facade_Stop\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
        facade_text,
        re.S,
    )
    if facade_stop_match is None:
        failures.append("RTC facade missing Ai_Rtc_Facade_Stop")
    else:
        facade_stop_body = facade_stop_match.group("body")
        for token in (
            "AI_RTC_FACADE_STATE_STARTING",
            "AI_RTC_FACADE_STATE_JOINING",
            "AI_RTC_FACADE_STATE_STOPPING",
            "AI_RTC_FACADE_ERR_BUSY",
        ):
            if token not in facade_stop_body:
                failures.append(f"RTC facade Stop missing async lifecycle busy gate token: {token}")

    facade_backend = read(RTC_FACADE / "src/ai_rtc_facade_agora_backend.c")
    if '#include "agora/ai_rtc_agora_service.h"' not in facade_backend:
        failures.append("facade Agora backend does not use SDK Agora service wrapper")
    if "agora_rtc_api.h" in facade_backend:
        failures.append("facade Agora backend still includes vendor Agora API directly")

    service_text = read(SDK_AGORA_DIR / "ai_rtc_agora_service.c")
    for token in (
        "AI_RTC_AGORA",
        "create_conn ok",
        "join conn=%d",
        "destroy_conn",
        "send_audio not_ready",
        "callback remote_user_joined",
    ):
        if token not in service_text:
            failures.append(f"SDK Agora service missing diagnosability log token: {token}")
    if "agora_rtc_api.h" not in service_text:
        failures.append("SDK Agora service does not own the vendor Agora API include")
    if "Ai_Rtc_Agora_Port_Get" not in service_text:
        failures.append("SDK Agora service does not use private platform port")
    for token in (
        "handler.on_video_data = on_video_data",
        "handler.on_target_bitrate_changed = on_target_bitrate_changed",
        "handler.on_key_frame_gen_req = on_key_frame_gen_req",
        "agora_rtc_join_channel_with_user_account",
        "agora_rtc_send_video_data",
        "Ai_Rtc_Facade_Backend_Notify_Video_Rx",
    ):
        if token not in service_text:
            failures.append(f"SDK Agora service missing video token: {token}")
    if "config->user_account" not in service_text:
        failures.append("SDK Agora service must branch on config->user_account for string UID join")
    if "service_option.use_string_uid = service_has_text(config->user_account)" not in service_text:
        failures.append("SDK Agora service must enable use_string_uid when joining with user_account")
    if "agora_rtc_join_channel(s_agora.conn_id" not in service_text:
        failures.append("SDK Agora service must preserve numeric UID join fallback")
    for token in (
        "Ai_Rtc_Agora_Session_Phase_t",
        "AI_RTC_AGORA_SESSION_JOINING",
        "AI_RTC_AGORA_SESSION_JOINED",
        "AI_RTC_AGORA_SESSION_LEAVING",
        "AI_RTC_AGORA_SESSION_FAILED",
    ):
        if token not in service_text:
            failures.append(f"SDK Agora service missing async session phase token: {token}")
    stop_session_match = re.search(
        r"static\s+int\s+service_stop_session\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
        service_text,
        re.S,
    )
    if stop_session_match is None:
        failures.append("SDK Agora service missing service_stop_session")
    else:
        stop_session_body = stop_session_match.group("body")
        if (
            "AI_RTC_AGORA_SESSION_JOINING" not in stop_session_body
            or "AI_RTC_FACADE_ERR_BUSY" not in stop_session_body
        ):
            failures.append("SDK Agora service must reject destructive stop while join is in flight")
    for callback in ("on_reconnecting", "on_connection_lost", "on_rejoin_channel_success"):
        callback_match = re.search(
            rf"static\s+void\s+{callback}\s*\([^)]*\)\s*\{{(?P<body>.*?)\n\}}",
            service_text,
            re.S,
        )
        if callback_match is None:
            failures.append(f"SDK Agora service missing callback: {callback}")
        elif "remote_user_joined = false" not in callback_match.group("body"):
            failures.append(
                f"SDK Agora service {callback} must clear remote_user_joined; "
                "reconnect/rejoin must require a fresh remote user joined event before audio TX"
            )
    for token in (
        "channel_options.enable_audio_downlink_aec = false",
        "channel_options.enable_audio_ai_qos = config->enable_audio_ai_qos",
        "channel_options.enable_audio_decode = false",
        "channel_options.enable_audio_jitter_buffer = config->enable_audio",
        "channel_options.audio_codec_opt.pcm_duration = AI_RTC_AGORA_SERVICE_PCM_DURATION_MS",
    ):
        if token not in service_text:
            failures.append(f"SDK Agora service missing legacy parity channel option: {token}")
    for function_name, expected_tokens in (
        (
            "Ai_Rtc_Agora_Service_Send_Audio",
            (
                "const bool active = s_agora.session_active",
                "const bool joined = s_agora.joined",
                "const bool remote_user_joined = s_agora.remote_user_joined",
                "const connection_id_t conn_id = s_agora.conn_id",
                "agora_rtc_send_audio_data(conn_id",
            ),
        ),
        (
            "Ai_Rtc_Agora_Service_Send_Video",
            (
                "const bool active = s_agora.session_active",
                "const bool joined = s_agora.joined",
                "const connection_id_t conn_id = s_agora.conn_id",
                "agora_rtc_send_video_data(conn_id",
            ),
        ),
        (
            "Ai_Rtc_Agora_Service_Send_Datastream",
            (
                "const bool active = s_agora.session_active",
                "const bool joined = s_agora.joined",
                "const bool control_ready = Ai_Rtc_Agora_Control_Is_Ready(&s_agora.control_state)",
                "const connection_id_t conn_id = s_agora.conn_id",
                "Ai_Rtc_Agora_Control_Send(&s_agora.control_state, conn_id, data, len)",
            ),
        ),
    ):
        function_match = re.search(
            rf"int\s+{function_name}\s*\([^)]*\)\s*\{{(?P<body>.*?)\n\}}",
            service_text,
            re.S,
        )
        if function_match is None:
            failures.append(f"SDK Agora service missing send function: {function_name}")
            continue
        body = function_match.group("body")
        for token in expected_tokens:
            if token not in body:
                failures.append(f"SDK Agora service {function_name} missing send snapshot token: {token}")
    if "s_agora.stale_callbacks = 0u" not in service_text:
        failures.append("SDK Agora service must reset stale_callbacks when session state is reset")
    shutdown_match = re.search(
        r"static\s+int\s+service_shutdown_sdk\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
        service_text,
        re.S,
    )
    if shutdown_match is None:
        failures.append("SDK Agora service missing private service_shutdown_sdk teardown helper")
    elif "agora_rtc_fini()" not in shutdown_match.group("body"):
        failures.append("SDK Agora service shutdown helper does not call agora_rtc_fini")
    stop_match = re.search(
        r"int\s+Ai_Rtc_Agora_Service_Stop\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
        service_text,
        re.S,
    )
    if stop_match is None:
        failures.append("SDK Agora service missing Ai_Rtc_Agora_Service_Stop")
    else:
        stop_body = stop_match.group("body")
        if "service_stop_session()" not in stop_body:
            failures.append("SDK Agora service Stop must release connection-level session resources")
        if "service_shutdown_sdk()" in stop_body:
            failures.append("SDK Agora service Stop must not call service_shutdown_sdk per dialog")
        if "agora_rtc_fini" in stop_body:
            failures.append("SDK Agora service Stop must not call agora_rtc_fini per dialog")
    shutdown_api_match = re.search(
        r"int\s+Ai_Rtc_Agora_Service_Shutdown\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
        service_text,
        re.S,
    )
    if shutdown_api_match is None:
        failures.append("SDK Agora service missing Ai_Rtc_Agora_Service_Shutdown")
    elif "service_shutdown_sdk()" not in shutdown_api_match.group("body"):
        failures.append("SDK Agora service Shutdown must release whole SDK")

    control_transport_text = read(SDK_AGORA_DIR / "ai_rtc_agora_control_transport.h")
    for token in (
        "AI_RTC_AGORA_CONTROL_TRANSPORT_H",
        "Ai_Rtc_Agora_Control_Config_t",
        "Ai_Rtc_Agora_Control_State_t",
        "connection_id_t conn_id",
        "const char *rtc_token",
        "const char *user_account",
        "const char *control_peer_id",
        "const char *control_token",
        "int stream_id",
        "bool rtm_logged_in",
        "uint32_t next_rtm_msg_id",
        "void Ai_Rtc_Agora_Control_Reset",
        "int Ai_Rtc_Agora_Control_Start",
        "int Ai_Rtc_Agora_Control_On_Rtc_Joined",
        "int Ai_Rtc_Agora_Control_Stop",
        "bool Ai_Rtc_Agora_Control_Is_Ready",
        "int Ai_Rtc_Agora_Control_Send",
    ):
        if token not in control_transport_text:
            failures.append(f"SDK Agora private control transport header missing token: {token}")
    if "const char *control_peer_id" not in control_transport_text:
        failures.append("SDK Agora private control transport state must preserve control_peer_id for RTM send")
    if "extern \"C\"" not in control_transport_text:
        failures.append("SDK Agora private control transport header must be C++ include-safe")

    control_datastream_text = read(SDK_AGORA_DIR / "ai_rtc_agora_control_datastream.c")
    for token in (
        "Ai_Rtc_Agora_Control_Reset",
        "Ai_Rtc_Agora_Control_Start",
        "Ai_Rtc_Agora_Control_On_Rtc_Joined",
        "Ai_Rtc_Agora_Control_Stop",
        "Ai_Rtc_Agora_Control_Is_Ready",
        "Ai_Rtc_Agora_Control_Send",
        "agora_rtc_create_data_stream",
        "agora_rtc_send_stream_message",
        "state->stream_id = -1",
    ):
        if token not in control_datastream_text:
            failures.append(f"SDK Agora datastream control transport missing token: {token}")
    service_without_includes = re.sub(r"^\s*#\s*include[^\n]*$", "", service_text, flags=re.M)
    for token in ("agora_rtc_create_data_stream", "agora_rtc_send_stream_message"):
        if token in service_without_includes:
            failures.append(f"SDK Agora service must not directly call datastream transport API after W16b Task 4: {token}")
    for token in (
        "Ai_Rtc_Agora_Control_On_Rtc_Joined",
        "Ai_Rtc_Agora_Control_Reset",
        "Ai_Rtc_Agora_Control_Stop",
        "Ai_Rtc_Agora_Control_Is_Ready",
        "Ai_Rtc_Agora_Control_Send",
    ):
        if token not in service_text:
            failures.append(f"SDK Agora service missing private control transport call: {token}")

    control_rtm_text = read(SDK_AGORA_DIR / "ai_rtc_agora_control_rtm.c")
    for token in (
        "AI_RTC_AGORA_CONTROL_TRANSPORT_RTM",
        "agora_rtc_login_rtm",
        "agora_rtc_logout_rtm",
        "agora_rtc_send_rtm_data",
        "RTM_EVENT_TYPE_LOGIN",
        "ERR_RTM_OK",
        "RTM_EVENT_TYPE_KICKOFF",
        "RTM_EVENT_TYPE_EXIT",
        "Ai_Rtc_Facade_Backend_Notify_Datastream_Rx",
        "message.stream_id = -1",
        "message.sender_uid = 0u",
        "state->next_rtm_msg_id++",
    ):
        if token not in control_rtm_text:
            failures.append(f"SDK Agora RTM control transport missing token: {token}")
    for path in RTC_FACADE.rglob("*.[ch]"):
        if path.is_relative_to(SDK_AGORA_DIR):
            continue
        text = read(path)
        for token in ("agora_rtc_login_rtm", "agora_rtc_logout_rtm", "agora_rtc_send_rtm_data"):
            if token in text:
                failures.append(f"RTM vendor API leaked outside SDK Agora private layer: {path}")

    datastream_text = read(SDK_AGORA_DIR / "ai_rtc_agora_datastream.c")
    for token in ("message.state", "message.user", "base64_decode", "cJSON_Parse"):
        if token not in datastream_text:
            failures.append(f"SDK Agora datastream parser missing token: {token}")
    datastream_on_message_match = re.search(
        r"int\s+Ai_Rtc_Agora_Datastream_On_Message\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
        datastream_text,
        re.S,
    )
    if datastream_on_message_match is None:
        failures.append("SDK Agora datastream parser missing Ai_Rtc_Agora_Datastream_On_Message")
    elif re.search(r"\bchar\s+packet\s*\[", datastream_on_message_match.group("body")):
        failures.append(
            "SDK Agora datastream parser must not allocate packet buffer on callback stack; "
            "legacy defers stream parsing and ESP32S3 Agora callback stacks are small"
        )

    stream_message_match = re.search(
        r"static\s+void\s+on_stream_message\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
        service_text,
        re.S,
    )
    if stream_message_match is None:
        failures.append("SDK Agora service missing on_stream_message")
    else:
        stream_body = stream_message_match.group("body")
        if "Ai_Rtc_Facade_Backend_Notify_Datastream_Rx" not in stream_body:
            failures.append("SDK Agora service on_stream_message must relay raw datastream through facade")
        if "Ai_Rtc_Agora_Datastream_On_Message" in stream_body:
            failures.append(
                "SDK Agora service on_stream_message must not parse datastream after raw facade relay; "
                "that duplicates parser state and callback-stack work"
            )

    esp_cmake = read(ESP_PRODUCT_NETWORK_CMAKE)
    if "agora_rtc/*.c" in esp_cmake or re.search(
        r"file\s*\(\s*GLOB\s+C_FILES\s+.*agora_rtc/\*\.c", esp_cmake, re.S
    ):
        failures.append("ESP32S3 product network_transfer still glob-builds agora_rtc/*.c")

    if ESP_PRODUCT.exists() and ESP_PRODUCT_AGORA.exists():
        for path in ESP_PRODUCT_AGORA.glob("*.[ch]"):
            text = read(path)
            if "ai_rtc_facade.h" not in text and "agora_rtc_api.h" in text:
                failures.append(f"ESP32S3 legacy Agora source still directly includes vendor API: {path}")
        esp_compat_text = read(ESP_PRODUCT / "ai_components/network_transfer/rtc_facade_compat.c")
        if "Ai_Rtc_Agora_Datastream_On_Message" not in esp_compat_text:
            failures.append(
                "ESP32S3 RTC facade compat must parse product datastream events outside SDK service; "
                "raw facade datastream is intentional and product semantics belong in compat code"
            )

    if failures:
        print("FAIL: SDK Agora RTC migration boundary check")
        for failure in failures:
            print(f"- {failure}")
        return 1

    print("PASS: SDK Agora RTC migration boundary check")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
