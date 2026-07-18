#!/usr/bin/env python3
"""Guard SDK-owned AI MQTT deferred publish policy.

W22b keeps product code simple: a product issues one AI token request. If MQTT
broker liveness is briefly PROBING, the SDK accepts and stores that AI publish,
then drains it when the effective state returns READY. It must not expose this
as an RTC facade API or defer generic MQTT publishes.
"""
from __future__ import annotations

import os
import re
from pathlib import Path


ROOT = Path(os.environ.get("ENTITY_MQTT_POLICY_ROOT", Path(__file__).resolve().parents[2]))
APP_C = ROOT / "entity_iot_sdk" / "entity_mqtt" / "entity_mqtt_app.c"
APP_H = ROOT / "entity_iot_sdk" / "entity_mqtt" / "entity_mqtt_app.h"
RTC_PUBLIC = ROOT / "media" / "rtc_facade" / "include" / "ai_rtc_facade.h"


def fail(message: str) -> None:
    raise SystemExit(f"FAIL: {message}")


def read(path: Path) -> str:
    if not path.exists():
        fail(f"missing file: {path.relative_to(ROOT)}")
    return path.read_text(encoding="utf-8", errors="replace")


def scrub_comments_and_strings(text: str) -> str:
    pattern = re.compile(
        r"""
        //[^\n]*               |
        /\*.*?\*/              |
        "(?:\\.|[^"\\])*"     |
        '(?:\\.|[^'\\])*'
        """,
        re.DOTALL | re.VERBOSE,
    )
    return pattern.sub(lambda match: "\n" * match.group(0).count("\n"), text)


def find_function(text: str, name: str) -> str:
    match = re.search(r"\b" + re.escape(name) + r"\s*\([^;]*?\)\s*\{", text, re.DOTALL)
    if not match:
        fail(f"missing function: {name}")

    start = match.end() - 1
    depth = 0
    for index in range(start, len(text)):
        char = text[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return text[start : index + 1]
    fail(f"could not parse function body: {name}")
    raise AssertionError("unreachable")


def require(text: str, token: str, message: str) -> None:
    if token not in text:
        fail(message)


def main() -> int:
    app_c = read(APP_C)
    app_h = read(APP_H)
    rtc_public = read(RTC_PUBLIC)
    scrubbed = scrub_comments_and_strings(app_c)

    for token in (
        "Entity_Mqtt_Ai_Publish_Result_t",
        "ENTITY_MQTT_AI_PUBLISH_READY",
        "ENTITY_MQTT_AI_PUBLISH_DEFERRED_PROBING",
        "ENTITY_MQTT_AI_PUBLISH_BLOCKED_DISCONNECTED",
        "ENTITY_MQTT_AI_PUBLISH_BLOCKED_DEAD",
        "Entity_Mqtt_App_Prepare_Ai_Publish_Result",
    ):
        require(app_h, token, f"entity_mqtt_app.h missing {token}")

    for token in (
        "s_ai_deferred_cmd_valid",
        "s_ai_deferred_mutex",
        "Entity_Mqtt_App_Store_Deferred_Ai_Publish",
        "Entity_Mqtt_App_Drain_Deferred_Ai_Publish",
        "Entity_Mqtt_App_Drop_Deferred_Ai_Publish",
        "Entity_Mqtt_App_Enqueue_Agent_Cmd",
        "[MQTT_AI_DEFERRED][STORE]",
        "[MQTT_AI_DEFERRED][DRAIN_READY]",
        "[MQTT_AI_DEFERRED][DROP]",
    ):
        require(app_c, token, f"entity_mqtt_app.c missing {token}")

    prepare_result = find_function(scrubbed, "Entity_Mqtt_App_Prepare_Ai_Publish_Result")
    for token in (
        "Entity_Mqtt_App_Get_Effective_State",
        "Entity_Mqtt_Effective_Can_Publish",
        "ENTITY_MQTT_AI_PUBLISH_READY",
        "ENTITY_MQTT_EFFECTIVE_PROBING",
        "ENTITY_MQTT_AI_PUBLISH_DEFERRED_PROBING",
        "Entity_Mqtt_Effective_Should_Reconnect",
    ):
        require(prepare_result, token, f"Prepare_Ai_Publish_Result missing {token}")

    prepare_bool = find_function(scrubbed, "Entity_Mqtt_App_Prepare_Ai_Publish")
    for token in (
        "Entity_Mqtt_App_Prepare_Ai_Publish_Result",
        "ENTITY_MQTT_AI_PUBLISH_READY",
        "ENTITY_MQTT_AI_PUBLISH_DEFERRED_PROBING",
    ):
        require(prepare_bool, token, f"bool Prepare_Ai_Publish wrapper missing {token}")

    topic_publish = find_function(scrubbed, "Entity_Mqtt_App_Topic_Publish")
    for token in (
        "Entity_Mqtt_App_Buffer_Contains",
        "ENTITY_MQTT_EFFECTIVE_PROBING",
        "s_agent_queue == NULL",
        "Entity_Mqtt_App_Store_Deferred_Ai_Publish",
        "Entity_Mqtt_App_Enqueue_Agent_Cmd",
    ):
        require(topic_publish, token, f"Topic_Publish missing AI deferred behavior token {token}")
    require(app_c, "reason=queue_null", "Topic_Publish missing deferred queue_null diagnostic")

    store_func = find_function(scrubbed, "Entity_Mqtt_App_Store_Deferred_Ai_Publish")
    for token in (
        "cmd->ai_access",
        "s_ai_deferred_cmd_valid",
        "s_ai_deferred_cmd = *cmd",
        "stored = s_ai_deferred_cmd",
    ):
        require(store_func, token, f"deferred store missing {token}")

    drain_func = find_function(scrubbed, "Entity_Mqtt_App_Drain_Deferred_Ai_Publish")
    for token in (
        "Entity_Mqtt_App_Get_Effective_State",
        "Entity_Mqtt_Effective_Can_Drain",
        "Entity_Mqtt_App_Enqueue_Agent_Cmd",
        "Entity_Mqtt_Effective_Should_Reconnect",
        "Entity_Mqtt_App_Drop_Deferred_Ai_Publish",
    ):
        require(drain_func, token, f"deferred drain missing {token}")

    drop_func = find_function(scrubbed, "Entity_Mqtt_App_Drop_Deferred_Ai_Publish")
    for token in (
        "Entity_Mem_Free",
        "Entity_Mqtt_Clear_Token_Pending(NULL)",
        "s_ai_deferred_cmd_valid = false",
    ):
        require(drop_func, token, f"deferred drop missing {token}")

    task_func = find_function(scrubbed, "Entity_Mqtt_Client_Task")
    drain_deferred_pos = task_func.find("Entity_Mqtt_App_Drain_Deferred_Ai_Publish")
    drain_queue_pos = task_func.find("Entity_Mqtt_App_Drain_Agent_Queue")
    if drain_deferred_pos < 0 or drain_queue_pos < 0 or drain_deferred_pos > drain_queue_pos:
        fail("MQTT task must drain deferred AI publish before draining the agent queue")

    if re.search(r"mqtt|probing|deferred", rtc_public, re.IGNORECASE):
        fail("public RTC facade must not expose MQTT deferred publish policy")

    print("PASS: entity mqtt AI deferred publish guard")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
