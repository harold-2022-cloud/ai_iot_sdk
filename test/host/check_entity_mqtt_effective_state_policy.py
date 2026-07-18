#!/usr/bin/env python3
"""Guard MQTT app policy decisions against raw broker-state regressions."""

from __future__ import annotations

import os
import re
from pathlib import Path


ROOT = Path(
    os.environ.get("ENTITY_MQTT_POLICY_ROOT", Path(__file__).resolve().parents[2])
)
APP = ROOT / "entity_iot_sdk" / "entity_mqtt" / "entity_mqtt_app.c"
HELPER = ROOT / "entity_iot_sdk" / "entity_mqtt" / "entity_mqtt_effective_state.c"

REMOVED_APP_HELPERS = (
    "Entity_Mqtt_App_Normalize_Broker_State",
    "Entity_Mqtt_App_Broker_Allows_Ai_Publish",
    "Entity_Mqtt_App_Broker_Healthy_For_Ai_Wait",
)

POLICY_FUNCTIONS = (
    "Entity_Mqtt_App_Drain_Agent_Queue",
    "Entity_Mqtt_App_Topic_Publish",
    "Entity_Mqtt_App_Prepare_Ai_Publish",
    "Entity_Mqtt_App_Is_Ai_Link_Healthy",
)

EXPECTED_POLICY_HELPERS = {
    "Entity_Mqtt_App_Drain_Agent_Queue": (
        "Entity_Mqtt_App_Get_Effective_State",
        "Entity_Mqtt_Effective_Can_Drain",
    ),
    "Entity_Mqtt_App_Topic_Publish": (
        "Entity_Mqtt_App_Get_Effective_State",
        "Entity_Mqtt_Effective_Can_Publish",
    ),
    "Entity_Mqtt_App_Prepare_Ai_Publish": (
        "Entity_Mqtt_App_Get_Effective_State",
        "Entity_Mqtt_Effective_Can_Publish",
        "Entity_Mqtt_Effective_Should_Reconnect",
    ),
    "Entity_Mqtt_App_Is_Ai_Link_Healthy": (
        "Entity_Mqtt_App_Get_Effective_State",
        "Entity_Mqtt_Effective_Can_Publish",
    ),
}

RECONNECT_ALLOWED_STATES = {
    "ENTITY_MQTT_EFFECTIVE_DISCONNECTED",
    "ENTITY_MQTT_EFFECTIVE_DEAD",
}

RAW_CONNECTED_TOKENS = (
    r"context\s*->\s*Is_Connected",
    r"Entity_Client_Instance\s*\.\s*Is_Connected",
    r"s_mqtt_app_connected",
    r"Entity_Mqtt_App_Is_Connected\s*\(",
)


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


def require_token(text: str, token: str, message: str) -> None:
    if token not in text:
        fail(message)


def reject_token(text: str, token: str, message: str) -> None:
    if token in text:
        fail(message)


def require_probing_maps_to_effective(helper_text: str) -> None:
    scrubbed = scrub_comments_and_strings(helper_text)
    state_func = find_function(scrubbed, "Entity_Mqtt_Effective_State_From_Input")
    match = re.search(
        r"case\s+MQTT_BROKER_LIVENESS_PROBING\s*:\s*"
        r"return\s+ENTITY_MQTT_EFFECTIVE_PROBING\s*;",
        state_func,
        re.DOTALL,
    )
    if not match:
        fail("effective state helper must map raw PROBING to ENTITY_MQTT_EFFECTIVE_PROBING")


def require_probing_does_not_reconnect(helper_text: str) -> None:
    scrubbed = scrub_comments_and_strings(helper_text)
    reconnect_func = find_function(scrubbed, "Entity_Mqtt_Effective_Should_Reconnect")
    return_exprs = re.findall(r"\breturn\s+([^;]+);", reconnect_func, re.DOTALL)
    if len(return_exprs) != 1:
        fail("Entity_Mqtt_Effective_Should_Reconnect must use one explicit return expression")

    expr = return_exprs[0]
    enum_names = set(re.findall(r"\bENTITY_MQTT_EFFECTIVE_[A-Z_]+\b", expr))
    if enum_names != RECONNECT_ALLOWED_STATES:
        fail(
            "Entity_Mqtt_Effective_Should_Reconnect must only allow "
            "DISCONNECTED or DEAD"
        )

    if "!=" in expr or re.search(r"!(?!=)", expr):
        fail("Entity_Mqtt_Effective_Should_Reconnect must not use negated logic")

    if re.search(r"\b[A-Za-z_][A-Za-z0-9_]*\s*\(", expr):
        fail("Entity_Mqtt_Effective_Should_Reconnect must not delegate reconnect policy")

    compact = re.sub(r"\s+", "", expr).replace("(", "").replace(")", "")
    allowed_terms = {
        f"state=={state}" for state in RECONNECT_ALLOWED_STATES
    } | {
        f"{state}==state" for state in RECONNECT_ALLOWED_STATES
    }
    for term in compact.split("||"):
        if term not in allowed_terms:
            fail(
                "Entity_Mqtt_Effective_Should_Reconnect must be an explicit "
                "DISCONNECTED/DEAD allow-list"
            )


def require_policy_uses_effective_helpers(app_text: str) -> None:
    scrubbed = scrub_comments_and_strings(app_text)
    for name, helpers in EXPECTED_POLICY_HELPERS.items():
        body = find_function(scrubbed, name)
        for helper in helpers:
            require_token(
                body,
                helper,
                f"{name} must use {helper} for effective-state policy",
            )


def reject_raw_connected_policy_gates(app_text: str) -> None:
    scrubbed = scrub_comments_and_strings(app_text)
    raw_connected = "|".join(RAW_CONNECTED_TOKENS)
    condition_patterns = (
        re.compile(r"\bif\s*\((?P<condition>[^{};]*)\)", re.DOTALL),
        re.compile(r"\bwhile\s*\((?P<condition>[^{};]*)\)", re.DOTALL),
        re.compile(r"\breturn\s+(?P<condition>[^;]*);", re.DOTALL),
    )
    for_pattern = re.compile(
        r"\bfor\s*\([^;]*;\s*(?P<condition>[^;]*)\s*;[^)]*\)",
        re.DOTALL,
    )
    alias_pattern = re.compile(
        r"""
        (?:
            \b(?:bool|int|uint\d+_t|size_t|unsigned\s+int|const\s+bool)\s+
        )?
        (?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*
        (?<![=!<>])=(?!=)\s*
        (?P<expr>[^;]*)
        ;
        """,
        re.DOTALL | re.VERBOSE,
    )

    for name in POLICY_FUNCTIONS:
        body = find_function(scrubbed, name)
        for match in alias_pattern.finditer(body):
            if re.search(raw_connected, match.group("expr")):
                line = scrubbed.count("\n", 0, scrubbed.find(match.group(0))) + 1
                fail(
                    f"{name} must not derive local policy aliases from raw "
                    f"MQTT connected state near line {line}"
                )

        for pattern in condition_patterns:
            for match in pattern.finditer(body):
                condition = match.group("condition")
                if re.search(raw_connected, condition):
                    line = scrubbed.count("\n", 0, scrubbed.find(match.group(0))) + 1
                    fail(
                        f"{name} must not gate policy directly on raw MQTT "
                        f"connected state near line {line}"
                    )

        for match in for_pattern.finditer(body):
            condition = match.group("condition")
            if re.search(raw_connected, condition):
                line = scrubbed.count("\n", 0, scrubbed.find(match.group(0))) + 1
                fail(
                    f"{name} must not gate for-loop policy directly on raw "
                    f"MQTT connected state near line {line}"
                )


def reject_raw_probing_policy(app_text: str) -> None:
    scrubbed = scrub_comments_and_strings(app_text)
    for helper in REMOVED_APP_HELPERS:
        reject_token(scrubbed, helper, f"app layer must not define/use removed helper: {helper}")

    raw_probing = "MQTT_BROKER_LIVENESS_PROBING"
    for name in POLICY_FUNCTIONS:
        body = find_function(scrubbed, name)
        if raw_probing in body:
            fail(f"{name} must not branch directly on raw broker PROBING state")

    for pattern in (
        r"\bbroker\s*\.\s*state\s*==\s*MQTT_BROKER_LIVENESS_PROBING\b",
        r"\bMQTT_BROKER_LIVENESS_PROBING\s*==\s*broker\s*\.\s*state\b",
        r"\bswitch\s*\([^)]*broker\s*\.\s*state[^)]*\)",
    ):
        match = re.search(pattern, scrubbed)
        if match:
            line = scrubbed.count("\n", 0, match.start()) + 1
            fail(f"app layer must not make raw broker-state policy decisions near line {line}")


def main() -> int:
    app_text = read(APP)
    helper_text = read(HELPER)

    reject_raw_probing_policy(app_text)
    require_policy_uses_effective_helpers(app_text)
    reject_raw_connected_policy_gates(app_text)
    require_probing_maps_to_effective(helper_text)
    require_probing_does_not_reconnect(helper_text)

    print("PASS: entity mqtt effective-state policy guard")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
