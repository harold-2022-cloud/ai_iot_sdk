#!/usr/bin/env python3
"""Guard: entity_mqtt_app.c must route every heap alloc/free through Entity_Mem_*.

Finding #1 (2026-07-21): a bare libc `malloc()` fallback whose result is later
released via `Entity_Mem_Free` (= `Bsp_Mem_Free`) is a cross-allocator free. The
Bsp allocator prepends an 8-byte header that its `free()` validates, so a raw
`malloc` pointer is either leaked (guard on) or corrupts the heap (guard off).
The business layer must therefore allocate/free ONLY through the `Entity_Mem_*`
HAL seam. This guard forbids bare libc allocator calls in entity_mqtt_app.c so
that bug (and its twin) cannot silently return.

Portable by construction: the rule is "use the HAL seam, do not hardcode an
allocator" -- exactly what keeps the file portable to any target, including a
Linux port that binds Entity_Mem_* to glibc malloc/free. This guard is scoped to
the ONE business file on purpose: neutral/platform layers (platform_os,
_neutral_big_malloc, chip allocator impls) legitimately wrap malloc/free and are
NOT covered here.
"""
from __future__ import annotations

import os
import re
from pathlib import Path


ROOT = Path(os.environ.get("ENTITY_MQTT_POLICY_ROOT", Path(__file__).resolve().parents[2]))
APP_C = ROOT / "entity_iot_sdk" / "entity_mqtt" / "entity_mqtt_app.c"

# Bare libc allocator calls only. The lookbehind excludes a leading identifier
# char (so Entity_Mem_Malloc / Bsp_Mem_Free / os_malloc / pvPortMalloc /
# heap_caps_malloc never match) and a leading '.' or '>' (member/arrow calls).
FORBIDDEN = re.compile(r"(?<![A-Za-z0-9_.>])(malloc|calloc|realloc|free)\s*\(")


def fail(message: str) -> None:
    raise SystemExit(f"FAIL: {message}")


def read(path: Path) -> str:
    if not path.exists():
        fail(f"missing file: {path}")
    return path.read_text(encoding="utf-8", errors="replace")


def scrub_comments_and_strings(text: str) -> str:
    """Replace comment/string bodies with blank lines, preserving line numbers."""
    pattern = re.compile(
        r"""
        //[^\n]*               |
        /\*.*?\*/              |
        "(?:\\.|[^"\\])*"     |
        '(?:\\.|[^'\\])*'
        """,
        re.DOTALL | re.VERBOSE,
    )
    return pattern.sub(lambda m: "\n" * m.group(0).count("\n"), text)


def main() -> int:
    text = scrub_comments_and_strings(read(APP_C))
    violations = [
        (text[: m.start()].count("\n") + 1, m.group(1))
        for m in FORBIDDEN.finditer(text)
    ]
    if violations:
        lines = "\n".join(
            f"  entity_mqtt_app.c:{ln}: bare `{tok}(` -- route through Entity_Mem_* instead"
            for ln, tok in violations
        )
        fail(
            "entity_mqtt_app.c must not call libc allocators directly "
            "(cross-allocator free vs Entity_Mem_Free / Bsp_Mem_Free):\n" + lines
        )
    print("PASS: entity mqtt allocator pairing guard")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
