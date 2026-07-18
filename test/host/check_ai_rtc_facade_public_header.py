#!/usr/bin/env python3
from pathlib import Path
import os
import re
import subprocess
import tempfile
import textwrap


ROOT = Path(__file__).resolve().parents[2]
HEADER_DIR = ROOT / "media" / "rtc_facade" / "include"
HEADER = HEADER_DIR / "ai_rtc_facade.h"


FORBIDDEN_PUBLIC_TOKENS = [
    "agora_rtc_api.h",
    "audio_frame_info_t",
    "video_frame_info_t",
    "connection_id_t",
    "agora_rtc_",
    "bk_",
    "beken",
    "esp_",
    "freertos",
    "driver/",
    "components/",
    "audio_codec",
    "codec",
    "microphone",
    "mic",
    "speaker",
    "dma",
    "resource",
    "heap",
    "lcd",
    "avi",
    "camera",
    "bluetooth",
    "ble_provision",
    "wifi",
    "wi-fi",
    "mqtt",
    "http",
    "nfc",
    "4g",
    "modem",
    "ppp",
    "button",
    "gpio",
    "pa_gpio",
    "frame_buffer_t",
    "ntwk_trans",
    "network_transfer",
    "product_app",
    "rtm",
    "ai_rtc_control_transport",
    "ai_rtc_control_transport_t",
    "control_transport",
]

REQUIRED_TOKENS = [
    "AI_RTC_FACADE_PUBLIC_API_VERSION",
    "Ai_Rtc_Facade_Result_t",
    "Ai_Rtc_Facade_State_t",
    "Ai_Rtc_Facade_Event_t",
    "Ai_Rtc_Facade_Token_Result_t",
    "user_account",
    "control_peer_id",
    "control_token",
    "enable_audio_ai_qos",
    "Ai_Rtc_Facade_Audio_Format_t",
    "Ai_Rtc_Facade_Audio_Frame_t",
    "Ai_Rtc_Facade_Video_Format_t",
    "Ai_Rtc_Facade_Video_Frame_t",
    "Ai_Rtc_Facade_Datastream_Message_t",
    "Ai_Rtc_Facade_State_Cb",
    "Ai_Rtc_Facade_Audio_Rx_Cb",
    "Ai_Rtc_Facade_Video_Rx_Cb",
    "Ai_Rtc_Facade_Datastream_Rx_Cb",
    "Ai_Rtc_Facade_Callbacks_t",
    "Ai_Rtc_Facade_Config_t",
    "Ai_Rtc_Facade_Init",
    "Ai_Rtc_Facade_Deinit",
    "Ai_Rtc_Facade_On_Token_Result",
    "Ai_Rtc_Facade_Stop",
    "Ai_Rtc_Facade_Get_State",
    "Ai_Rtc_Facade_Is_Joined",
    "Ai_Rtc_Facade_Wait_Joined",
    "Ai_Rtc_Facade_Send_Audio",
    "Ai_Rtc_Facade_Send_Datastream",
    "Ai_Rtc_Facade_Send_Video",
]

REQUIRED_INCLUDES = [
    "#include <stdbool.h>",
    "#include <stddef.h>",
    "#include <stdint.h>",
]
INCLUDE_RE = re.compile(r"^\s*#\s*include\s+(?P<target><[^>\n]+>|\"[^\"\n]+\")\s*(?://.*|/\*.*\*/\s*)?$")


def fail(message: str) -> None:
    print(f"FAIL: {message}")
    raise SystemExit(1)


def read(path: Path) -> str:
    if not path.exists():
        fail(f"missing file: {path}")
    return path.read_text(encoding="utf-8")


def require_tokens(label: str, text: str, tokens: list[str]) -> None:
    for token in tokens:
        if token not in text:
            fail(f"{label} missing token: {token}")


def reject_public_tokens(text: str) -> None:
    lowered = text.lower()
    for token in FORBIDDEN_PUBLIC_TOKENS:
        if token in lowered:
            fail(f"public RTC facade header contains forbidden token: {token}")


def require_only_public_includes(text: str) -> None:
    allowed_lines = set(REQUIRED_INCLUDES)
    seen_lines = set()
    for lineno, line in enumerate(text.splitlines(), start=1):
        if not re.match(r"^\s*#\s*include\b", line):
            continue
        match = INCLUDE_RE.match(line)
        if not match:
            fail(f"unexpected include directive on line {lineno}: {line.strip()}")
        include_line = f"#include {match.group('target')}"
        if include_line not in allowed_lines:
            fail(f"unexpected include directive on line {lineno}: {line.strip()}")
        if include_line in seen_lines:
            fail(f"duplicate include directive on line {lineno}: {line.strip()}")
        seen_lines.add(include_line)

    for include_line in REQUIRED_INCLUDES:
        if include_line not in seen_lines:
            fail(f"ai_rtc_facade.h missing include: {include_line}")


def compile_second_product_sample() -> None:
    cc = os.environ.get("CC", "cc")
    sample = r'''#include "ai_rtc_facade.h"
        #include <stdbool.h>
        #include <stddef.h>
        #include <stdint.h>

        static void on_state(Ai_Rtc_Facade_Event_t event,
                             Ai_Rtc_Facade_State_t state,
                             int detail,
                             void *user)
        {
            (void)event;
            (void)state;
            (void)detail;
            (void)user;
        }

        static int on_audio_rx(const Ai_Rtc_Facade_Audio_Frame_t *frame, void *user)
        {
            (void)frame;
            (void)user;
            return AI_RTC_FACADE_OK;
        }

        static int on_video_rx(const Ai_Rtc_Facade_Video_Frame_t *frame, void *user)
        {
            (void)frame;
            (void)user;
            return AI_RTC_FACADE_ERR_UNSUPPORTED;
        }

        static int on_datastream_rx(const Ai_Rtc_Facade_Datastream_Message_t *message, void *user)
        {
            (void)message;
            (void)user;
            return AI_RTC_FACADE_OK;
        }

        int main(void)
        {
            uint8_t audio_buf[320] = {0};
            const Ai_Rtc_Facade_Config_t config = {
                .join_timeout_ms = 8000,
                .enable_audio = true,
                .enable_video = false,
            };
            const Ai_Rtc_Facade_Callbacks_t callbacks = {
                .on_state = on_state,
                .on_audio_rx = on_audio_rx,
                .on_video_rx = on_video_rx,
                .on_datastream_rx = on_datastream_rx,
                .user = 0,
            };
            const Ai_Rtc_Facade_Token_Result_t token = {
                .result = 0,
                .rtc_token = "token",
                .channel_name = "channel",
                .app_id = "app",
                .uid = 7,
                .user_account = "local-user",
                .control_peer_id = "agent-user",
                .control_token = "control-token",
                .enable_audio_ai_qos = true,
            };
            const Ai_Rtc_Facade_Audio_Frame_t audio = {
                .data = audio_buf,
                .len = sizeof(audio_buf),
                .format = AI_RTC_FACADE_AUDIO_FORMAT_PCM16,
                .sample_rate_hz = 16000,
                .channels = 1,
                .duration_ms = 20,
                .timestamp_ms = 0,
            };
            const Ai_Rtc_Facade_Video_Frame_t video = {
                .data = audio_buf,
                .len = sizeof(audio_buf),
                .format = AI_RTC_FACADE_VIDEO_FORMAT_H264,
                .width = 320,
                .height = 240,
                .frame_rate_hz = 15,
                .timestamp_ms = 0,
                .flags = AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME,
            };

            int rc = Ai_Rtc_Facade_Init(&config, &callbacks);
            rc += Ai_Rtc_Facade_On_Token_Result(&token);
            rc += Ai_Rtc_Facade_Send_Audio(&audio);
            rc += Ai_Rtc_Facade_Send_Datastream(audio_buf, sizeof(audio_buf));
            rc += Ai_Rtc_Facade_Send_Video(&video);
            rc += Ai_Rtc_Facade_Wait_Joined(1);
            rc += Ai_Rtc_Facade_Stop();

            Ai_Rtc_Facade_State_t state = Ai_Rtc_Facade_Get_State();
            bool joined = Ai_Rtc_Facade_Is_Joined();
            Ai_Rtc_Facade_Deinit();

            return (rc == 123456 && state == AI_RTC_FACADE_STATE_JOINED && joined) ? 1 : 0;
        }
    '''
    with tempfile.TemporaryDirectory(prefix="ai_rtc_facade_header_") as tmp:
        src = Path(tmp) / "second_product_sample.c"
        obj = Path(tmp) / "second_product_sample.o"
        src.write_text(textwrap.dedent(sample), encoding="utf-8")
        cmd = [
            cc,
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I",
            str(HEADER_DIR),
            "-c",
            str(src),
            "-o",
            str(obj),
        ]
        result = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode != 0:
            print(result.stdout, end="")
            fail("second-product sample does not compile with public RTC facade header")
        print("compile:", " ".join(cmd))


def compile_cpp_second_product_sample() -> None:
    cxx = os.environ.get("CXX", "c++")
    sample = r'''#include "ai_rtc_facade.h"

        static void on_state(Ai_Rtc_Facade_Event_t event,
                             Ai_Rtc_Facade_State_t state,
                             int detail,
                             void *user)
        {
            (void)event;
            (void)state;
            (void)detail;
            (void)user;
        }

        static int on_audio_rx(const Ai_Rtc_Facade_Audio_Frame_t *frame, void *user)
        {
            (void)frame;
            (void)user;
            return AI_RTC_FACADE_OK;
        }

        int main()
        {
            unsigned char payload[8] = {};
            Ai_Rtc_Facade_Config_t config = {};
            config.join_timeout_ms = 1000U;
            config.enable_audio = true;
            config.enable_video = false;

            Ai_Rtc_Facade_Callbacks_t callbacks = {};
            callbacks.on_state = on_state;
            callbacks.on_audio_rx = on_audio_rx;

            Ai_Rtc_Facade_Token_Result_t token = {};
            token.result = AI_RTC_FACADE_OK;
            token.rtc_token = "token";
            token.channel_name = "channel";
            token.app_id = "app";
            token.uid = 42;

            Ai_Rtc_Facade_Audio_Frame_t audio = {};
            audio.data = payload;
            audio.len = sizeof(payload);
            audio.format = AI_RTC_FACADE_AUDIO_FORMAT_PCM16;
            audio.sample_rate_hz = 16000U;
            audio.channels = 1U;
            audio.duration_ms = 20U;

            const int rc = Ai_Rtc_Facade_Init(&config, &callbacks) +
                           Ai_Rtc_Facade_On_Token_Result(&token) +
                           Ai_Rtc_Facade_Send_Audio(&audio) +
                           Ai_Rtc_Facade_Send_Datastream(payload, sizeof(payload)) +
                           Ai_Rtc_Facade_Stop();
            const Ai_Rtc_Facade_State_t state = Ai_Rtc_Facade_Get_State();
            const bool joined = Ai_Rtc_Facade_Is_Joined();
            Ai_Rtc_Facade_Deinit();

            return (rc == 123456 && state == AI_RTC_FACADE_STATE_JOINED && joined) ? 1 : 0;
        }
    '''
    with tempfile.TemporaryDirectory(prefix="ai_rtc_facade_header_cpp_") as tmp:
        src = Path(tmp) / "second_product_sample.cpp"
        obj = Path(tmp) / "second_product_sample.o"
        src.write_text(textwrap.dedent(sample), encoding="utf-8")
        cmd = [
            cxx,
            "-std=c++11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I",
            str(HEADER_DIR),
            "-c",
            str(src),
            "-o",
            str(obj),
        ]
        result = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode != 0:
            print(result.stdout, end="")
            fail("second-product C++ sample does not compile with public RTC facade header")
        print("compile:", " ".join(cmd))


def main() -> int:
    header = read(HEADER)
    require_only_public_includes(header)
    require_tokens("ai_rtc_facade.h", header, REQUIRED_TOKENS)
    reject_public_tokens(header)
    compile_second_product_sample()
    compile_cpp_second_product_sample()
    print("PASS: ai rtc facade public header")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
