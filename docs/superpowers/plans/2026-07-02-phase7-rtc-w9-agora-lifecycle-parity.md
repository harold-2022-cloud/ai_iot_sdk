# Phase 7 RTC W9 Agora Lifecycle Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore the stable Agora RTC lifecycle and audio-send readiness semantics from the legacy ESP32S3 integration inside the SDK backend without expanding the public `ai_rtc_facade.h` API.

**Architecture:** Keep `ai_rtc_facade.h` as the only product-facing RTC API. Move the proven lifecycle behavior into the SDK Agora backend: initialize Agora once per app id, keep the SDK initialized across per-dialog stop/start cycles, release only connection-level resources on `Stop()`, and reserve `agora_rtc_fini()` for explicit shutdown or app id changes. Gate audio TX inside the Agora service until both channel join and remote AI user join have happened, matching the legacy path without exposing ESP32S3 heap or DMA policy to Linux, Beken, or product code.

**Tech Stack:** C11, Agora RTSA C API, SDK RTC facade, SDK private Agora backend, host C tests, Python static checker, GNU Make host test harness.

---

## Current Evidence

- Vendor API model from `ai_components/agora_iot_sdk/include/agora_rtc_api.h`:
  - `agora_rtc_init()` initializes the SDK process context.
  - `agora_rtc_create_connection()` and `agora_rtc_join_channel()` create a per-channel session.
  - `agora_rtc_leave_channel()` and `agora_rtc_destroy_connection()` release per-session resources.
  - `agora_rtc_fini()` releases the whole SDK.
- Legacy ESP32S3 integration in `/mnt/c/Users/harold.chen/esp32s3_ai_alarm/ai_components/network_transfer/agora_rtc/agora_rtc.c` already documents the stable rule:
  - `agora_rtc_init/fini` are heavyweight.
  - SDK init stays resident.
  - Per AI dialog only creates, joins, leaves, and destroys a connection.
  - `fini` is used for app shutdown or app id changes.
- Current SDK backend mismatch:
  - `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.c` calls `service_shutdown_sdk()` from `Ai_Rtc_Agora_Service_Stop()`.
  - That means each dialog stop calls `agora_rtc_fini()`.
- Current SDK audio TX mismatch:
  - `Ai_Rtc_Agora_Service_Send_Audio()` only checks `s_agora.joined`.
  - Legacy ESP32S3 gates audio TX on active session, started state, channel joined, and remote user joined.
- W9 will not add public resource, heap, DMA, or uplink APIs.

## Files

- Modify: `/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend.c`
  - Add host tests that encode the stable lifecycle and audio TX gate.
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.c`
  - Keep SDK initialized across per-dialog `Stop()`.
  - Track remote user joined state.
  - Gate audio TX until remote user joined.
  - Keep `Shutdown()` and app id change as whole-SDK teardown paths.
- Modify: `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_sdk_agora_migration.py`
  - Add static checks that reject public resource policy API drift and require per-dialog stop to avoid `service_shutdown_sdk()`.
- Modify: `/root/smp/ai_iot_sdk/docs/superpowers/reviews/2026-07-02-phase7-rtc-sdk-gap-analysis.md`
  - Correct W9 recommendation from runtime resource policy to Agora lifecycle parity.
- Verify only with host/static commands. Do not run IDF build commands in this plan.

## Task 1: Add Host Test For Stop Keeping Agora SDK Initialized

**Files:**
- Modify: `/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend.c`

- [x] **Step 1: Add the failing lifecycle expectation to `test_agora_backend_flow`**

Change the assertions after the first `Ai_Rtc_Facade_Stop()` from:

```c
CHECK(Agora_Rtc_Stub_State()->fini_calls == 1);
```

to:

```c
CHECK(Agora_Rtc_Stub_State()->fini_calls == 0);
```

Change the repeated same-app start assertions from:

```c
CHECK(Agora_Rtc_Stub_State()->init_calls == 2);
```

to:

```c
CHECK(Agora_Rtc_Stub_State()->init_calls == 1);
```

Change the second stop assertions from:

```c
CHECK(Agora_Rtc_Stub_State()->fini_calls == 2);
```

to:

```c
CHECK(Agora_Rtc_Stub_State()->fini_calls == 0);
```

Change the app id change assertions from:

```c
CHECK(Agora_Rtc_Stub_State()->fini_calls == 2);
CHECK(Agora_Rtc_Stub_State()->init_calls == 3);
```

to:

```c
CHECK(Agora_Rtc_Stub_State()->fini_calls == 1);
CHECK(Agora_Rtc_Stub_State()->init_calls == 2);
```

Change the third stop assertion from:

```c
CHECK(Agora_Rtc_Stub_State()->fini_calls == 3);
```

to:

```c
CHECK(Agora_Rtc_Stub_State()->fini_calls == 1);
```

Change the final deinit assertion from:

```c
CHECK(Agora_Rtc_Stub_State()->fini_calls == 3);
```

to:

```c
CHECK(Agora_Rtc_Stub_State()->fini_calls == 2);
```

- [x] **Step 2: Run the host test and confirm it fails on current implementation**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test_ai_rtc_facade_agora_backend
/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend
```

Expected result before implementation:

```text
FAIL ... CHECK(Agora_Rtc_Stub_State()->fini_calls == 0)
```

- [x] **Step 3: Stop Task 1 after the red test**

Do not modify `ai_rtc_agora_service.c` in Task 1. The red test is the deliverable for this task.

## Task 2: Keep Agora SDK Resident Across Per-Dialog Stop

**Files:**
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.c`

- [x] **Step 1: Update `Ai_Rtc_Agora_Service_Stop()` to release only session resources**

Replace the current function:

```c
int Ai_Rtc_Agora_Service_Stop(void)
{
    int rc = AI_RTC_FACADE_OK;

    if (service_stop_session() != AI_RTC_FACADE_OK)
    {
        rc = AI_RTC_FACADE_ERR_INTERNAL;
    }
    if (service_shutdown_sdk() != AI_RTC_FACADE_OK)
    {
        rc = AI_RTC_FACADE_ERR_INTERNAL;
    }
    service_reset_session_state();
    return rc;
}
```

with:

```c
int Ai_Rtc_Agora_Service_Stop(void)
{
    int rc = AI_RTC_FACADE_OK;

    if (service_stop_session() != AI_RTC_FACADE_OK)
    {
        rc = AI_RTC_FACADE_ERR_INTERNAL;
    }
    service_reset_session_state();
    return rc;
}
```

- [x] **Step 2: Keep app id change as explicit SDK teardown**

Verify this existing branch remains in `Ai_Rtc_Agora_Service_Start()`:

```c
if (s_agora.sdk_initialized && strcmp(s_agora.app_id, config->app_id) != 0)
{
    (void)Ai_Rtc_Agora_Service_Shutdown();
}
```

No code change is needed for this step if the branch still calls `Ai_Rtc_Agora_Service_Shutdown()`.

- [x] **Step 3: Run the lifecycle host test**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test_ai_rtc_facade_agora_backend
/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend
```

Expected result after implementation:

```text
PASS
```

## Task 3: Gate Audio TX Until Remote User Joined

**Files:**
- Modify: `/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend.c`
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.c`

- [x] **Step 1: Add a failing audio TX gate test inside `test_agora_backend_flow`**

Immediately after:

```c
Agora_Rtc_Stub_Emit_Joined();
CHECK(Ai_Rtc_Facade_Is_Joined());
CHECK(test.joined_events == 1);
CHECK(Agora_Rtc_Stub_State()->create_data_stream_calls == 1);
CHECK(Agora_Rtc_Stub_State()->last_reliable);
CHECK(Agora_Rtc_Stub_State()->last_ordered);
```

add:

```c
CHECK(Ai_Rtc_Facade_Send_Audio(&audio) == AI_RTC_FACADE_ERR_NOT_READY);
CHECK(Agora_Rtc_Stub_State()->send_audio_calls == 0);
```

After:

```c
Agora_Rtc_Stub_Emit_User_Joined(66u);
CHECK(test.remote_joined_events == 1);
CHECK(test.last_detail == 66);
```

add:

```c
CHECK(Ai_Rtc_Facade_Send_Audio(&audio) == AI_RTC_FACADE_OK);
CHECK(Agora_Rtc_Stub_State()->send_audio_calls == 1);
```

Remove or update the later first-send block so the send count expects the second successful send:

```c
CHECK(Ai_Rtc_Facade_Send_Audio(&audio) == AI_RTC_FACADE_OK);
CHECK(Agora_Rtc_Stub_State()->send_audio_calls == 2);
CHECK(Agora_Rtc_Stub_State()->last_audio_data == audio_data);
CHECK(Agora_Rtc_Stub_State()->last_audio_len == sizeof(audio_data));
CHECK(Agora_Rtc_Stub_State()->last_audio_type == AUDIO_DATA_TYPE_G722);
```

- [x] **Step 2: Run the test and confirm current implementation fails**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test_ai_rtc_facade_agora_backend
/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend
```

Expected result before implementation:

```text
FAIL ... CHECK(Ai_Rtc_Facade_Send_Audio(&audio) == AI_RTC_FACADE_ERR_NOT_READY)
```

- [x] **Step 3: Track remote user state in the Agora service context**

In `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.c`, add a field to `Ai_Rtc_Agora_Service_Context_t`:

```c
volatile bool remote_user_joined;
```

Update `service_reset_session_state()`:

```c
static void service_reset_session_state(void)
{
    s_agora.session_active = false;
    s_agora.joined = false;
    s_agora.remote_user_joined = false;
    s_agora.conn_id = CONNECTION_ID_INVALID;
    s_agora.stream_id = -1;
    Ai_Rtc_Agora_Datastream_Reset();
}
```

Update the per-start reset block before `agora_rtc_create_connection()`:

```c
s_agora.session_active = false;
s_agora.joined = false;
s_agora.remote_user_joined = false;
s_agora.conn_id = CONNECTION_ID_INVALID;
s_agora.stream_id = -1;
```

- [x] **Step 4: Maintain remote user state from callbacks**

In `on_user_joined()` before notifying the facade, set:

```c
s_agora.remote_user_joined = true;
```

In `on_user_offline()` before notifying the facade, set:

```c
s_agora.remote_user_joined = false;
```

In `on_reconnecting()` and `on_connection_lost()`, set:

```c
s_agora.remote_user_joined = false;
```

In `on_rejoin_channel_success()`, set:

```c
s_agora.remote_user_joined = false;
```

- [x] **Step 5: Gate `Ai_Rtc_Agora_Service_Send_Audio()`**

Replace:

```c
if (!s_agora.joined || frame == NULL || frame->data == NULL || frame->len == 0u)
{
    return AI_RTC_FACADE_ERR_NOT_READY;
}
```

with:

```c
if (frame == NULL || frame->data == NULL || frame->len == 0u)
{
    return AI_RTC_FACADE_ERR_NOT_READY;
}
if (!s_agora.session_active || !s_agora.joined || !s_agora.remote_user_joined)
{
    return AI_RTC_FACADE_ERR_NOT_READY;
}
```

- [x] **Step 6: Run the host test**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test_ai_rtc_facade_agora_backend
/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend
```

Expected result:

```text
PASS
```

## Task 4: Add Static Guard Against Public Resource Policy API Drift

**Files:**
- Modify: `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_sdk_agora_migration.py`

- [x] **Step 1: Add public header negative checks**

Add this function:

```python
def require_public_header_has_no_resource_policy() -> None:
    public_header = read(SDK_ROOT / "media/rtc_facade/include/ai_rtc_facade.h")
    forbidden_tokens = [
        "heap_caps",
        "MALLOC_CAP",
        "dma_largest",
        "internal_largest",
        "resource_snapshot",
        "resource_can_use",
        "UPLINK_READY",
        "uplink_ready",
    ]
    for token in forbidden_tokens:
        require(token not in public_header, f"public facade header leaked resource policy token: {token}")
```

Call it from `main()`:

```python
require_public_header_has_no_resource_policy()
```

- [x] **Step 2: Add service lifecycle static checks**

Add this function:

```python
def require_per_dialog_stop_keeps_sdk_resident() -> None:
    service = read(SDK_ROOT / "media/rtc_facade/src/agora/ai_rtc_agora_service.c")
    stop_match = re.search(
        r"int\\s+Ai_Rtc_Agora_Service_Stop\\s*\\([^)]*\\)\\s*\\{(?P<body>.*?)\\n\\}",
        service,
        re.DOTALL,
    )
    require(stop_match is not None, "Ai_Rtc_Agora_Service_Stop function not found")
    stop_body = stop_match.group("body")
    require("service_stop_session()" in stop_body, "Stop must release connection-level session resources")
    require("service_shutdown_sdk()" not in stop_body, "Stop must not call service_shutdown_sdk per dialog")
    require("agora_rtc_fini" not in stop_body, "Stop must not call agora_rtc_fini per dialog")

    shutdown_match = re.search(
        r"int\\s+Ai_Rtc_Agora_Service_Shutdown\\s*\\([^)]*\\)\\s*\\{(?P<body>.*?)\\n\\}",
        service,
        re.DOTALL,
    )
    require(shutdown_match is not None, "Ai_Rtc_Agora_Service_Shutdown function not found")
    require("service_shutdown_sdk()" in shutdown_match.group("body"), "Shutdown must release whole SDK")
```

Call it from `main()`:

```python
require_per_dialog_stop_keeps_sdk_resident()
```

- [x] **Step 3: Run static checker**

Run:

```bash
python3 /root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_sdk_agora_migration.py
```

Expected result:

```text
PASS: ai_rtc_facade SDK Agora migration checks passed
```

## Task 5: Correct W9 Recommendation In Gap Analysis

**Files:**
- Modify: `/root/smp/ai_iot_sdk/docs/superpowers/reviews/2026-07-02-phase7-rtc-sdk-gap-analysis.md`

- [x] **Step 1: Replace resource-policy recommendation with lifecycle parity recommendation**

Replace the W9 recommendation section with:

```markdown
Create SDK W9: Agora Lifecycle Parity / Repeat-Session Hardening.

Scope:
- Keep `ai_rtc_facade.h` unchanged.
- Keep `agora_rtc_init()` resident across repeated AI dialog stop/start cycles.
- Make per-dialog `Stop()` call only leave/destroy connection-level Agora resources.
- Keep `agora_rtc_fini()` for explicit facade deinit/shutdown and app id changes.
- Gate audio TX inside the SDK Agora service until the remote AI user has joined.
- Preserve host/static tests that reject public heap, DMA, or resource policy API drift.

Out of scope for W9:
- Adding public resource, heap, DMA, or uplink-ready APIs.
- Running IDF builds from the agent environment.
- Rewiring Beken or xiaozhi product integrations.
```

- [x] **Step 2: Run a placeholder scan**

Run:

```bash
rg -n "runtime resource policy|resource policy layer|UPLINK_READY|uplink_ready|dma_largest.*W9" /root/smp/ai_iot_sdk/docs/superpowers/reviews/2026-07-02-phase7-rtc-sdk-gap-analysis.md
```

Expected result:

```text
```

The command should print no matching lines.

## Task 6: Run Host Verification

**Files:**
- Verify: `/root/smp/ai_iot_sdk/test/host/Makefile`
- Verify: `/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend.c`
- Verify: `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_sdk_agora_migration.py`

- [x] **Step 1: Run the focused Agora backend test**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test_ai_rtc_facade_agora_backend
/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend
```

Expected result:

```text
PASS
```

- [x] **Step 2: Run the SDK Agora migration checker**

Run:

```bash
python3 /root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_sdk_agora_migration.py
```

Expected result:

```text
PASS: ai_rtc_facade SDK Agora migration checks passed
```

- [x] **Step 3: Run all SDK host tests**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test
```

Expected result:

```text
./test_bk7258_allocator_guard
PASS
./test_ai_rtc_facade_core
PASS
./test_ai_rtc_facade_agora_backend
PASS
./test_ai_rtc_agora_datastream
PASS
```

- [x] **Step 4: Run diff whitespace check**

Run:

```bash
git -C /root/smp/ai_iot_sdk diff --check -- media/rtc_facade/src/agora/ai_rtc_agora_service.c test/host/test_ai_rtc_facade_agora_backend.c test/host/check_ai_rtc_facade_sdk_agora_migration.py docs/superpowers/reviews/2026-07-02-phase7-rtc-sdk-gap-analysis.md docs/superpowers/plans/2026-07-02-phase7-rtc-w9-agora-lifecycle-parity.md
```

Expected result:

```text
```

The command should print no whitespace errors.

## Runtime Validation Handoff

The agent must not run IDF firmware builds for this project. After host verification passes, hand off these firmware checks to the user:

- ESP32S3 alarm:
  - Build from Windows cmd after setting `IDF_TOOLS_PATH=C:\Espressif\.espressif_v5.5` and running `C:\Users\harold.chen\esp-idf-v5.5\export.bat`.
  - Capture a repeat wakeup log with at least three AI sessions.
  - Expected runtime evidence:
    - First session initializes Agora SDK.
    - Per-dialog stop logs leave/destroy connection but does not fini.
    - Same app id second session skips SDK init.
    - Audio TX starts only after remote user joined.
- Beken:
  - Confirm facade public API remains unchanged and no ESP32S3 resource policy leaked into common SDK headers.
- xiaozhi M5Stack CoreS3:
  - Confirm the product can consume the same public facade contract without extra heap or DMA API calls.

## Self-Review

- Spec coverage: The plan keeps Agora API usage simple, moves proven lifecycle behavior into SDK backend, avoids public resource policy APIs, adds host tests, adds static checks, and defines user-side firmware validation.
- Placeholder scan: The plan avoids deferred implementation markers and includes exact paths, code blocks, commands, and expected results.
- Type consistency: The plan uses existing names from `ai_rtc_facade.h`, `ai_rtc_agora_service.c`, `agora_rtc_api.h`, and current host stubs.
