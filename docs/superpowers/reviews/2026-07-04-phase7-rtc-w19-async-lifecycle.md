# Phase 7 RTC W19 Async Lifecycle Closure

Date: 2026-07-04

## Scope

W19 makes the SDK RTC facade safe for Agora asynchronous join lifecycle without
turning the SDK into a high-frequency UI debounce layer.

## Result

- `Ai_Rtc_Facade_Stop()` rejects stop while state is `STARTING`, `JOINING`, or
  `STOPPING`.
- Stop during in-flight join does not call `agora_rtc_leave_channel()` or
  `agora_rtc_destroy_connection()`.
- Private Agora backend tracks session phase so callback and stop paths are
  explicit.
- RTC callbacks are treated as state-machine inputs. The SDK uses
  `connection_id_t` as the connection lifecycle key; callback `uid` values are
  remote-user or sender identity when the Agora callback provides one.
- Product/UI layers remain responsible for gating rapid AI button toggles.

## Verification

- `make -C test/host test`: passed
- `python3 test/host/check_ai_rtc_facade_sdk_agora_migration.py`: passed
- `python3 test/host/check_ai_rtc_facade_public_header.py`: passed
- `make -C test/host clean`: completed
- `git diff --check`: passed
