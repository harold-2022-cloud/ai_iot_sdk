# Phase 7 RTC W19 Async Lifecycle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the SDK RTC facade safe for Agora asynchronous join/leave lifecycle without hiding high-frequency AI button toggles inside the RTC backend.

**Architecture:** The public API remains `ai_rtc_facade.h`. The facade rejects lifecycle re-entry while an Agora join is still in flight, and the private Agora service records explicit session phase so callbacks and stop/start paths are deterministic. Product/UI code remains responsible for debounce and for not issuing repeated start/stop requests while RTC is `STARTING`, `JOINING`, or `STOPPING`.

**Tech Stack:** C11, Agora RTC C API, SDK private Agora backend, host tests under `test/host`.

---

## Design Contract

Agora RTC join is asynchronous:

```text
agora_rtc_join_channel() returns OK
        |
        v
on_join_channel_success() arrives afterward
```

Therefore W19 must not treat a successful `join_channel()` return as a fully joined channel. If the product calls stop before `on_join_channel_success()`, the SDK must not force a destructive `leave_channel()` + `destroy_connection()` cleanup on an in-flight join.

W19 is not a product button debounce feature. The product/UI layer should block high-frequency AI button toggles while the facade reports transient lifecycle states.

State policy:

| Facade state | Token/start request | Stop request |
|---|---|---|
| `IDLE` | allowed | no-op |
| `TOKEN_READY` | allowed through normal token path | no-op or stop to `IDLE` |
| `STARTING` | `AI_RTC_FACADE_ERR_BUSY` | `AI_RTC_FACADE_ERR_BUSY` |
| `JOINING` | `AI_RTC_FACADE_ERR_BUSY` | `AI_RTC_FACADE_ERR_BUSY` |
| `JOINED` | renew token if app/channel is same session | leave session |
| `RECONNECTING` | renew token | leave session |
| `STOPPING` | `AI_RTC_FACADE_ERR_BUSY` | idempotent busy/no-op |
| `FAILED` | allowed retry after backend phase is safe | no-op or cleanup |

SDK guarantee:

- No public RTM/resource/DMA/uplink lifecycle API is added.
- No backend `Start()` path destroys a resident in-channel or in-flight connection.
- `Stop()` during `JOINING` is rejected as busy and must not call `agora_rtc_leave_channel()` or `agora_rtc_destroy_connection()`.
- RTC callbacks are state-machine inputs. The SDK uses `connection_id_t` as the
  connection lifecycle key; callback `uid` values identify the remote user or
  media/datastream sender where the Agora callback provides one.
- Product code must gate UI requests so the user cannot fire repeated open/close transitions during `STARTING`, `JOINING`, or `STOPPING`.

## File Map

- Modify `test/host/test_ai_rtc_facade_agora_backend.c`
  - Adds regression coverage for stop while join is still in flight.
  - Adds coverage that repeated token/start during joining stays busy.
- Modify `media/rtc_facade/src/ai_rtc_facade.c`
  - Rejects `Ai_Rtc_Facade_Stop()` while state is `STARTING`, `JOINING`, or `STOPPING`.
  - Keeps current joined token renew path.
- Modify `media/rtc_facade/src/agora/ai_rtc_agora_service.c`
  - Adds private Agora session phase.
  - Records `JOINING`, `JOINED`, `RECONNECTING`, `LEAVING`, `FAILED`, and `IDLE`.
  - Guards backend direct stop/start against unsafe re-entry.
- Modify `docs/rtc-porting-contract.md`
  - Documents product-side debounce/gate requirement.
- Modify `test/host/check_ai_rtc_facade_sdk_agora_migration.py`
  - Adds static guard that `Stop()` during `JOINING` must not be converted back into destructive cleanup.

---

## Task 1: Add Host Regression For In-Flight Join Stop Gate

**Files:**
- Modify: `test/host/test_ai_rtc_facade_agora_backend.c`

- [ ] **Step 1: Write the failing test**

Add this test before `test_agora_backend_flow()`:

```c
static int test_agora_backend_rejects_stop_while_joining(void)
{
    Test_State_t test;
    const Ai_Rtc_Facade_Config_t config = {
        .join_timeout_ms = 3000,
        .enable_audio = true,
        .enable_video = false,
    };
    const Ai_Rtc_Facade_Callbacks_t callbacks = {
        .on_state = on_state,
        .user = &test,
    };
    Ai_Rtc_Facade_Token_Result_t token = {
        .result = 0,
        .rtc_token = "token",
        .channel_name = "channel",
        .app_id = "appid",
        .uid = 9,
    };

    memset(&test, 0, sizeof(test));
    Agora_Rtc_Stub_Reset();
    Ai_Rtc_Agora_Port_Stub_Reset();

    CHECK(Ai_Rtc_Facade_Init(&config, &callbacks) == AI_RTC_FACADE_OK);
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_OK);
    CHECK(Ai_Rtc_Facade_Get_State() == AI_RTC_FACADE_STATE_JOINING);
    CHECK(Agora_Rtc_Stub_State()->join_channel_calls == 1);

    token.rtc_token = "token-2";
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_ERR_BUSY);
    CHECK(Agora_Rtc_Stub_State()->create_connection_calls == 1);
    CHECK(Agora_Rtc_Stub_State()->join_channel_calls == 1);

    CHECK(Ai_Rtc_Facade_Stop() == AI_RTC_FACADE_ERR_BUSY);
    CHECK(Ai_Rtc_Facade_Get_State() == AI_RTC_FACADE_STATE_JOINING);
    CHECK(Agora_Rtc_Stub_State()->leave_channel_calls == 0);
    CHECK(Agora_Rtc_Stub_State()->destroy_connection_calls == 0);

    Agora_Rtc_Stub_Emit_Joined();
    CHECK(Ai_Rtc_Facade_Get_State() == AI_RTC_FACADE_STATE_JOINED);
    CHECK(Ai_Rtc_Facade_Stop() == AI_RTC_FACADE_OK);
    CHECK(Agora_Rtc_Stub_State()->leave_channel_calls == 1);
    CHECK(Agora_Rtc_Stub_State()->destroy_connection_calls == 1);

    Ai_Rtc_Facade_Deinit();
    return 0;
}
```

Register it in `main()` before `test_agora_backend_flow()`:

```c
CHECK(test_agora_backend_rejects_stop_while_joining() == 0);
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test_ai_rtc_facade_agora_backend
/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend
```

Expected failure before implementation:

```text
FAIL ... Ai_Rtc_Facade_Stop() == AI_RTC_FACADE_ERR_BUSY
```

Current code fails because `Ai_Rtc_Facade_Stop()` calls backend stop while the facade is still `JOINING`, which calls `agora_rtc_leave_channel()` and `agora_rtc_destroy_connection()`.

---

## Task 2: Gate Facade Stop During STARTING/JOINING/STOPPING

**Files:**
- Modify: `media/rtc_facade/src/ai_rtc_facade.c`
- Test: `test/host/test_ai_rtc_facade_agora_backend.c`

- [ ] **Step 1: Implement minimal facade gate**

In `Ai_Rtc_Facade_Stop()`, after the `IDLE` no-op check and before emitting `STOPPING`, add:

```c
    if (s_ctx.state == AI_RTC_FACADE_STATE_STARTING ||
        s_ctx.state == AI_RTC_FACADE_STATE_JOINING ||
        s_ctx.state == AI_RTC_FACADE_STATE_STOPPING)
    {
        return AI_RTC_FACADE_ERR_BUSY;
    }
```

This keeps SDK behavior deterministic: the product must wait until join completes or fails before issuing stop.

- [ ] **Step 2: Run focused test**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test_ai_rtc_facade_agora_backend
/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend
```

Expected:

```text
PASS: ai rtc facade agora backend
```

- [ ] **Step 3: Commit Task 1/2**

Run:

```bash
git -C /root/smp/ai_iot_sdk add media/rtc_facade/src/ai_rtc_facade.c test/host/test_ai_rtc_facade_agora_backend.c
git -C /root/smp/ai_iot_sdk commit -m "fix(rtc): reject stop while Agora join is pending"
```

---

## Task 3: Add Private Agora Session Phase Guard

**Files:**
- Modify: `media/rtc_facade/src/agora/ai_rtc_agora_service.c`
- Test: `test/host/test_ai_rtc_facade_agora_backend.c`

- [ ] **Step 1: Add private phase enum and field**

Near the top of `ai_rtc_agora_service.c`, before `Ai_Rtc_Agora_Service_Context_t`, add:

```c
typedef enum
{
    AI_RTC_AGORA_SESSION_IDLE = 0,
    AI_RTC_AGORA_SESSION_JOINING,
    AI_RTC_AGORA_SESSION_JOINED,
    AI_RTC_AGORA_SESSION_RECONNECTING,
    AI_RTC_AGORA_SESSION_LEAVING,
    AI_RTC_AGORA_SESSION_FAILED,
} Ai_Rtc_Agora_Session_Phase_t;
```

Add this field to `Ai_Rtc_Agora_Service_Context_t`:

```c
    Ai_Rtc_Agora_Session_Phase_t phase;
```

Initialize it in `s_agora`:

```c
    .phase = AI_RTC_AGORA_SESSION_IDLE,
```

- [ ] **Step 2: Update phase transitions**

Apply these transition rules in `ai_rtc_agora_service.c`:

```c
static void service_reset_session_state(void)
{
    s_agora.session_active = false;
    s_agora.joined = false;
    s_agora.remote_user_joined = false;
    s_agora.conn_id = CONNECTION_ID_INVALID;
    s_agora.phase = AI_RTC_AGORA_SESSION_IDLE;
    (void)Ai_Rtc_Agora_Control_Stop(&s_agora.control_state);
    Ai_Rtc_Agora_Datastream_Reset();
}
```

In `on_join_channel_success()`:

```c
    s_agora.phase = AI_RTC_AGORA_SESSION_JOINED;
```

In `on_reconnecting()`:

```c
    s_agora.phase = AI_RTC_AGORA_SESSION_RECONNECTING;
```

In `on_connection_lost()` and `on_error()`:

```c
    s_agora.phase = AI_RTC_AGORA_SESSION_FAILED;
```

In `on_rejoin_channel_success()`:

```c
    s_agora.phase = AI_RTC_AGORA_SESSION_JOINED;
```

After `agora_rtc_create_connection()` succeeds and before issuing `agora_rtc_join_channel()` or `agora_rtc_join_channel_with_user_account()` in `Ai_Rtc_Agora_Service_Start()`:

```c
    s_agora.phase = AI_RTC_AGORA_SESSION_JOINING;
```

In `service_stop_session()`, before calling `agora_rtc_leave_channel()`:

```c
    if (s_agora.phase == AI_RTC_AGORA_SESSION_JOINING)
    {
        return AI_RTC_FACADE_ERR_BUSY;
    }
    s_agora.phase = AI_RTC_AGORA_SESSION_LEAVING;
```

After `leave_channel()` and `destroy_connection()` complete:

```c
    s_agora.phase = AI_RTC_AGORA_SESSION_IDLE;
```

- [ ] **Step 3: Add direct backend guard test**

Add this assertion to `test_agora_backend_rejects_stop_while_joining()` after the facade stop assertion:

```c
    CHECK(Agora_Rtc_Stub_State()->leave_channel_calls == 0);
    CHECK(Agora_Rtc_Stub_State()->destroy_connection_calls == 0);
```

These checks are already part of the Task 1 test; keep them to prove the backend never receives destructive calls during in-flight join.

- [ ] **Step 4: Run focused test**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test_ai_rtc_facade_agora_backend
/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend
```

Expected:

```text
PASS: ai rtc facade agora backend
```

- [ ] **Step 5: Commit**

Run:

```bash
git -C /root/smp/ai_iot_sdk add media/rtc_facade/src/agora/ai_rtc_agora_service.c test/host/test_ai_rtc_facade_agora_backend.c
git -C /root/smp/ai_iot_sdk commit -m "fix(rtc): track Agora async session phase"
```

---

## Task 4: Document Product AI Button Gate

**Files:**
- Modify: `docs/rtc-porting-contract.md`

- [ ] **Step 1: Add lifecycle section**

Add this section after `## 7. Media Gating Contract` and renumber following sections:

```markdown
## 8. Lifecycle And Product Button Gate

Agora RTC channel join is asynchronous. A successful `join_channel()` return
means the join request was accepted; it does not mean the channel is joined.
The SDK reports joined state only after the backend join-success callback.

Products must not issue high-frequency AI open/close requests while RTC is in a
transient lifecycle state. The product/UI layer should gate the AI button:

- allow start only from `AI_RTC_FACADE_STATE_IDLE` or `AI_RTC_FACADE_STATE_FAILED`;
- do not call stop while state is `AI_RTC_FACADE_STATE_STARTING` or
  `AI_RTC_FACADE_STATE_JOINING`;
- do not call start while state is `AI_RTC_FACADE_STATE_STOPPING`;
- treat `AI_RTC_FACADE_ERR_BUSY` as "request ignored because RTC lifecycle is
  still in flight", not as a server or AI failure.

The SDK must reject destructive lifecycle re-entry during in-flight join. It
must not call `leave_channel()` or `destroy_connection()` merely because a
product issued a rapid stop while the join-success callback is still pending.
```

- [ ] **Step 2: Commit**

Run:

```bash
git -C /root/smp/ai_iot_sdk add docs/rtc-porting-contract.md
git -C /root/smp/ai_iot_sdk commit -m "docs(rtc): document async lifecycle gate"
```

---

## Task 5: Add Static Guard For Join-In-Flight Stop Safety

**Files:**
- Modify: `test/host/check_ai_rtc_facade_sdk_agora_migration.py`

- [ ] **Step 1: Add guard**

Add this check near the existing RTC facade migration guards:

```python
def require_joining_stop_busy_gate(repo: Path) -> None:
    facade = read(repo / "media/rtc_facade/src/ai_rtc_facade.c")
    service = read(repo / "media/rtc_facade/src/agora/ai_rtc_agora_service.c")

    require(
        "AI_RTC_FACADE_STATE_JOINING" in facade
        and "AI_RTC_FACADE_ERR_BUSY" in facade
        and "Ai_Rtc_Facade_Stop" in facade,
        "Ai_Rtc_Facade_Stop must reject stop while join is in flight",
    )
    require(
        "AI_RTC_AGORA_SESSION_JOINING" in service
        and "AI_RTC_AGORA_SESSION_LEAVING" in service,
        "Agora service must track private async session phase",
    )
```

Call it from `main()`:

```python
    require_joining_stop_busy_gate(repo)
```

- [ ] **Step 2: Run guard**

Run:

```bash
python3 /root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_sdk_agora_migration.py
```

Expected:

```text
PASS: SDK Agora RTC migration boundary check
```

- [ ] **Step 3: Commit**

Run:

```bash
git -C /root/smp/ai_iot_sdk add test/host/check_ai_rtc_facade_sdk_agora_migration.py
git -C /root/smp/ai_iot_sdk commit -m "test(rtc): guard async lifecycle stop gate"
```

---

## Task 6: Full SDK Verification And Closure Note

**Files:**
- Create: `docs/superpowers/reviews/2026-07-04-phase7-rtc-w19-async-lifecycle.md`

- [ ] **Step 1: Run full verification**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test
python3 /root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_sdk_agora_migration.py
python3 /root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_public_header.py
make -C /root/smp/ai_iot_sdk/test/host clean
git -C /root/smp/ai_iot_sdk diff --check
```

Expected:

```text
PASS: entity interface reference template
PASS: ai rtc facade public header
PASS: ai rtc facade core boundary
PASS: SDK Agora RTC migration boundary check
PASS valid alloc/free
PASS valid realloc
PASS foreign pointer free
PASS double-free
PASS corrupt magic
PASS Bsp_Psram aliases
PASS: ai rtc facade core
PASS: ai rtc facade agora backend
PASS: ai rtc facade agora backend rtm
PASS: ai rtc agora datastream
```

- [ ] **Step 2: Write closure note**

Create `docs/superpowers/reviews/2026-07-04-phase7-rtc-w19-async-lifecycle.md`:

```markdown
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
- Product/UI layers remain responsible for gating rapid AI button toggles.

## Verification

- `make -C test/host test`: passed
- `python3 test/host/check_ai_rtc_facade_sdk_agora_migration.py`: passed
- `python3 test/host/check_ai_rtc_facade_public_header.py`: passed
- `make -C test/host clean`: completed
- `git diff --check`: passed
```

- [ ] **Step 3: Commit**

Run:

```bash
git -C /root/smp/ai_iot_sdk add docs/superpowers/reviews/2026-07-04-phase7-rtc-w19-async-lifecycle.md
git -C /root/smp/ai_iot_sdk commit -m "docs(rtc): close W19 async lifecycle"
```

---

## Self-Review

- Spec coverage: the plan covers asynchronous join semantics, high-frequency product request ownership, SDK busy behavior, backend phase tracking, docs, guard, and verification.
- Placeholder scan: no unresolved placeholder keywords or deferred implementation steps are present.
- Type consistency: all public constants and functions referenced already exist in `ai_rtc_facade.h`; new private enum names are local to `ai_rtc_agora_service.c`.
