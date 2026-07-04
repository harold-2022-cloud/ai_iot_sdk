#!/usr/bin/env python3
from pathlib import Path
import os
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
TEMPLATE_DIR = ROOT / "docs" / "templates" / "entity_interface_reference"
HEADER = TEMPLATE_DIR / "entity_interface_reference.h"
SOURCE = TEMPLATE_DIR / "entity_interface_reference.c"


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


def reject_tokens(label: str, text: str, tokens: list[str]) -> None:
    lowered = text.lower()
    for token in tokens:
        if token in lowered:
            fail(f"{label} contains forbidden token: {token}")


def require_order(label: str, text: str, tokens: list[str]) -> None:
    last = -1
    for token in tokens:
        pos = text.find(token)
        if pos < 0:
            fail(f"{label} missing ordered token: {token}")
        if pos <= last:
            fail(f"{label} token out of order: {token}")
        last = pos


def compile_template() -> None:
    cc = os.environ.get("CC", "cc")
    with tempfile.TemporaryDirectory(prefix="entity_if_ref_") as tmp:
        obj = Path(tmp) / "entity_interface_reference.o"
        cmd = [
            cc,
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I",
            str(TEMPLATE_DIR),
            "-I",
            str(ROOT / "entity_iot_sdk" / "entity_iot"),
            "-c",
            str(SOURCE),
            "-o",
            str(obj),
        ]
        result = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode != 0:
            print(result.stdout, end="")
            fail("template does not compile in isolation")
        print("compile:", " ".join(cmd))


def main() -> int:
    header = read(HEADER)
    source = read(SOURCE)
    combined = header + "\n" + source

    reject_tokens(
        "entity_interface_reference template",
        combined,
        ["bk_", "esp_", "driver/", "components/", "beken", "product_app"],
    )

    require_tokens(
        "entity_interface_reference.h",
        header,
        [
            "ENTITY_INTERFACE_REFERENCE_TEMPLATE_VERSION",
            "Entity_Interface_Reference_Config_t",
            "Entity_Interface_Reference_Ai_Token_Result_t",
            "Entity_Interface_Reference_Hooks",
            "Entity_Product_Hooks_Register",
            "Entity_Product_Hooks_On_Dev_Status",
            "Entity_Product_Hooks_On_Dp_Received",
            "Entity_Product_Hooks_On_Ai_Token_Result",
            "Entity_Iot_Interface_Init",
            "Entity_Device_Access_Export_Interface",
            "Entity_Device_Access_Stop_Export_Interface",
            "Entity_Manual_Reset_Export_Interface",
            "Entity_Manual_Config_Net_Export_Interface",
            "Entity_Interface_Reference_Work_Enqueue",
        ],
    )

    require_tokens(
        "entity_interface_reference.c",
        source,
        [
            "ENTITY_INTERFACE_REF_WORK_DEVICE_ACCESS_REQUEST",
            "ENTITY_INTERFACE_REF_WORK_DEVICE_ACCESS_STOP",
            "ENTITY_INTERFACE_REF_WORK_MANUAL_RESET",
            "ENTITY_INTERFACE_REF_WORK_MANUAL_CONFIG_NET",
            "Entity_Interface_Reference_Enqueue_Scalar_Work",
            "Callback safety rule",
            "Product-owned",
        ],
    )

    require_order(
        "Entity_Iot_Interface_Init",
        source,
        [
            "init_system_imports",
            "init_periph_imports",
            "init_mqtt_imports",
        ],
    )

    compile_template()
    print("PASS: entity interface reference template")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
