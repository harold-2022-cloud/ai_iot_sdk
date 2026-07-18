#!/usr/bin/env python3
"""Guard low-frequency RTC diagnostic markers used during product porting."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
FACADE_CORE = ROOT / "media/rtc_facade/src/ai_rtc_facade.c"
AGORA_SERVICE = ROOT / "media/rtc_facade/src/agora/ai_rtc_agora_service.c"
PORTING_DOC = ROOT / "docs/rtc-porting-contract.md"


def read(path: Path) -> str:
    if not path.exists():
        raise SystemExit(f"FAIL: missing file: {path}")
    return path.read_text(encoding="utf-8", errors="replace")


def require(path: Path, token: str) -> None:
    text = read(path)
    if token not in text:
        raise SystemExit(f"FAIL: {path.relative_to(ROOT)} missing token: {token}")


def main() -> int:
    for token in (
        "[AI_RTC_FACADE_CORE]",
        "event=%d state=%d detail=%d joined=%d",
        "AI_RTC_FACADE_STATE_TOKEN_READY",
        "AI_RTC_FACADE_EVENT_STOPPED",
        "AI_RTC_FACADE_EVENT_FAILED",
    ):
        require(FACADE_CORE, token)

    for token in (
        "[AI_RTC_AGORA]",
        "create_conn ok conn=%d gen=%lu",
        "join conn=%d mode=user_account",
        "join conn=%d mode=numeric",
        "callback joined conn=%d",
        "callback remote_user_joined conn=%d",
        "callback remote_user_offline conn=%d",
        "callback reconnecting conn=%d",
        "send_audio not_ready active=%d joined=%d remote_user_joined=%d conn=%d gen=%lu",
        "send_datastream not_ready joined=%d control_ready=%d data=%d len=%lu conn=%d gen=%lu",
        "leave conn=%d gen=%lu",
        "destroy_conn conn=%d gen=%lu",
    ):
        require(AGORA_SERVICE, token)

    for token in (
        "token result accepted or rejected",
        "join start",
        "remote user joined",
        "audio/video TX not ready",
        "datastream ready",
        "stop reason",
        "Diagnostic markers must not become public API",
    ):
        require(PORTING_DOC, token)

    print("PASS: ai rtc diagnostic marker guard")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
