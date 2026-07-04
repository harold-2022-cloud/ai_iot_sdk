#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
RTC_DIR = ROOT / "media" / "rtc_facade"

CORE_FILES = [
    RTC_DIR / "include" / "ai_rtc_facade.h",
    RTC_DIR / "src" / "ai_rtc_facade.c",
    RTC_DIR / "src" / "ai_rtc_facade_backend.h",
]

AGORA_BACKEND = RTC_DIR / "src" / "ai_rtc_facade_agora_backend.c"
AGORA_SERVICE = RTC_DIR / "src" / "agora" / "ai_rtc_agora_service.c"

FORBIDDEN_CORE_TOKENS = [
    "agora_rtc_api.h",
    "connection_id_t",
    "audio_frame_info_t",
    "video_frame_info_t",
    "bk_",
    "beken",
    "esp_",
    "freertos",
    "driver/",
    "components/",
    "network_transfer",
    "ntwk_trans",
    "product_app",
    "frame_buffer_t",
]


def fail(message: str) -> None:
    print(f"FAIL: {message}")
    raise SystemExit(1)


def read(path: Path) -> str:
    if not path.exists():
        fail(f"missing file: {path}")
    return path.read_text(encoding="utf-8")


def main() -> int:
    for path in CORE_FILES:
        text = read(path).lower()
        for token in FORBIDDEN_CORE_TOKENS:
            if token in text:
                fail(f"{path} contains forbidden RTC core token: {token}")

    backend_text = read(AGORA_BACKEND)
    if '#include "agora/ai_rtc_agora_service.h"' not in backend_text:
        fail("real Agora backend must delegate to SDK Agora service privately")
    if "agora_rtc_api.h" in backend_text:
        fail("real Agora backend must not include vendor Agora API directly")

    service_text = read(AGORA_SERVICE)
    if '#include "agora_rtc_api.h"' not in service_text:
        fail("SDK Agora service must own the vendor Agora API include")

    for path in (RTC_DIR / "src").glob("*.c"):
        text = read(path)
        if path != AGORA_BACKEND and "agora_rtc_api.h" in text:
            fail(f"top-level facade source must not include agora_rtc_api.h: {path}")

    print("PASS: ai rtc facade core boundary")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
