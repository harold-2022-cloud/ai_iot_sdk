#!/usr/bin/env python3
"""Guard RTC control transport as a build-time private backend choice."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PUBLIC_HEADER = ROOT / "media/rtc_facade/include/ai_rtc_facade.h"
CONTROL_HEADER = ROOT / "media/rtc_facade/src/agora/ai_rtc_agora_control_transport.h"
DATASTREAM_IMPL = ROOT / "media/rtc_facade/src/agora/ai_rtc_agora_control_datastream.c"
RTM_IMPL = ROOT / "media/rtc_facade/src/agora/ai_rtc_agora_control_rtm.c"
RTC_API_DOC = ROOT / "docs/rtc-facade-api.md"
AI_DIALOG_DOC = ROOT / "docs/rtc-ai-dialog-and-emotion.md"
PORTING_DOC = ROOT / "docs/porting-guide.md"


def read(path: Path) -> str:
    if not path.exists():
        raise SystemExit(f"FAIL: missing file: {path}")
    return path.read_text(encoding="utf-8", errors="replace")


def require(path: Path, token: str) -> None:
    text = read(path)
    if token not in text:
        raise SystemExit(f"FAIL: {path.relative_to(ROOT)} missing token: {token}")


def reject(path: Path, token: str) -> None:
    text = read(path)
    if token in text:
        raise SystemExit(f"FAIL: {path.relative_to(ROOT)} contains forbidden token: {token}")


def main() -> int:
    public_forbidden = (
        "AI_RTC_AGORA_CONTROL_TRANSPORT",
        "AI_RTC_FACADE_CONTROL_TRANSPORT",
        "Ai_Rtc_Facade_Set_Control_Transport",
        "Ai_Rtc_Facade_Send_Rtm",
        "Ai_Rtc_Facade_Send_RTM",
    )
    for token in public_forbidden:
        reject(PUBLIC_HEADER, token)

    for token in (
        "#define AI_RTC_AGORA_CONTROL_TRANSPORT_DATASTREAM 1",
        "#define AI_RTC_AGORA_CONTROL_TRANSPORT_RTM 2",
        "#define AI_RTC_AGORA_CONTROL_TRANSPORT AI_RTC_AGORA_CONTROL_TRANSPORT_DATASTREAM",
    ):
        require(CONTROL_HEADER, token)

    require(DATASTREAM_IMPL, "#if AI_RTC_AGORA_CONTROL_TRANSPORT == AI_RTC_AGORA_CONTROL_TRANSPORT_DATASTREAM")
    require(RTM_IMPL, "#if AI_RTC_AGORA_CONTROL_TRANSPORT == AI_RTC_AGORA_CONTROL_TRANSPORT_RTM")

    for doc in (RTC_API_DOC, AI_DIALOG_DOC, PORTING_DOC):
        require(doc, "build-time")
        require(doc, "AI_RTC_AGORA_CONTROL_TRANSPORT_RTM")
        require(doc, "Ai_Rtc_Facade_Send_Datastream")

    for token in ("user_account", "control_peer_id", "control_token"):
        require(RTC_API_DOC, token)
        require(AI_DIALOG_DOC, token)

    print("PASS: ai rtc control transport build-time guard")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
