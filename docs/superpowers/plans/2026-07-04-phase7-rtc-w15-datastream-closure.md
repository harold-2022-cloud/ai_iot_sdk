# Phase 7 RTC W15 Datastream Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close Phase 7 RTC SDK datastream work by freezing datastream as the accepted control-message transport, keeping RTM private/future-only, and verifying the facade/Agora backend boundary.

**Architecture:** The SDK public surface remains chip-neutral: audio, video, raw datastream TX/RX, and session lifecycle through `ai_rtc_facade.h`. Agora datastream transport stays behind the private backend using `agora_rtc_create_data_stream`, `agora_rtc_send_stream_message`, and `on_stream_message`; SDK-private envelope parsing remains available for shared validation, while product event semantics stay product-owned. RTM is documented as a future private backend transport choice and must not appear in the public facade API during W15.

**Tech Stack:** C11 RTC facade, Agora IoT RTC C API, Python static guards, host `make` verification.

---

## File Structure

- Modify: `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_public_header.py`
  - Responsibility: reject vendor/product/chip-specific tokens in `ai_rtc_facade.h`; W15 adds explicit RTM/control-transport public API guards.
- Modify: `/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend.c`
  - Responsibility: host-test Agora backend lifecycle and transport mapping; W15 strengthens datastream stream-ready and stop-reset assertions.
- Modify: `/root/smp/ai_iot_sdk/docs/rtc-porting-contract.md`
  - Responsibility: document SDK/product ownership boundaries; W15 records datastream as the accepted Phase 7 AI control transport and RTM as future private backend work.
- Create: `/root/smp/ai_iot_sdk/docs/superpowers/reviews/2026-07-04-phase7-rtc-w15-datastream-closure.md`
  - Responsibility: closure note with verification results and accepted boundary.

No runtime product files change in W15. No RTM API is added in W15.

---

### Task 1: Guard Public Facade Against RTM / Transport Selector API

**Files:**
- Modify: `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_public_header.py`
- Test: `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_public_header.py`

- [ ] **Step 1: Write the failing guard**

Add these tokens to `FORBIDDEN_PUBLIC_TOKENS`:

```python
    "rtm",
    "ai_rtc_control_transport",
    "ai_rtc_control_transport_t",
    "control_transport",
```

These tokens intentionally reject:

```c
typedef enum {
    AI_RTC_CONTROL_TRANSPORT_DATASTREAM = 0,
    AI_RTC_CONTROL_TRANSPORT_RTM,
} Ai_Rtc_Control_Transport_t;
```

- [ ] **Step 2: Verify the guard catches a temporary bad header**

Temporarily append this block to `/tmp/ai_rtc_facade_bad.h` after copying the real header:

```c
typedef enum {
    AI_RTC_CONTROL_TRANSPORT_DATASTREAM = 0,
    AI_RTC_CONTROL_TRANSPORT_RTM,
} Ai_Rtc_Control_Transport_t;
```

Then point the guard at the temporary file by substituting `HEADER = Path("/tmp/ai_rtc_facade_bad.h")` in a temporary copy of the guard and run:

```bash
python3 /tmp/check_ai_rtc_facade_public_header_bad.py
```

Expected output includes:

```text
FAIL: public RTC facade header contains forbidden token: rtm
```

- [ ] **Step 3: Keep the real public header unchanged**

Do not modify `/root/smp/ai_iot_sdk/media/rtc_facade/include/ai_rtc_facade.h`.

The accepted public datastream API remains:

```c
typedef struct
{
    int stream_id;
    uint32_t sender_uid;
    const uint8_t *data;
    size_t len;
    uint64_t sent_ts;
} Ai_Rtc_Facade_Datastream_Message_t;

typedef int (*Ai_Rtc_Facade_Datastream_Rx_Cb)(const Ai_Rtc_Facade_Datastream_Message_t *message,
                                              void *user);

int Ai_Rtc_Facade_Send_Datastream(const uint8_t *data, size_t len);
```

- [ ] **Step 4: Verify the real public header passes**

Run:

```bash
python3 /root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_public_header.py
```

Expected output:

```text
PASS: RTC facade public header boundary check
```

---

### Task 2: Strengthen Datastream Backend Lifecycle Host Coverage

**Files:**
- Modify: `/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend.c`
- Test: `/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend.c`

- [ ] **Step 1: Add failing assertions for datastream stream readiness and stop reset**

Inside `test_agora_backend_flow()`, after:

```c
Agora_Rtc_Stub_Emit_Rejoined();
CHECK(test.rejoined_events == 1);
CHECK(Ai_Rtc_Facade_Is_Joined());
```

add:

```c
    CHECK(Ai_Rtc_Facade_Send_Datastream(msg, sizeof(msg)) == AI_RTC_FACADE_OK);
    CHECK(Agora_Rtc_Stub_State()->send_stream_calls == 1);
```

Then change the later datastream success assertion from:

```c
    CHECK(Agora_Rtc_Stub_State()->send_stream_calls == 1);
```

to:

```c
    CHECK(Agora_Rtc_Stub_State()->send_stream_calls == 2);
```

After the first successful stop:

```c
CHECK(Ai_Rtc_Facade_Get_State() == AI_RTC_FACADE_STATE_IDLE);
```

add:

```c
    CHECK(Ai_Rtc_Facade_Send_Datastream(msg, sizeof(msg)) == AI_RTC_FACADE_ERR_NOT_READY);
    CHECK(Agora_Rtc_Stub_State()->send_stream_calls == 2);
```

This verifies that rejoin keeps datastream usable when the Agora stream id is still valid, and stop resets facade/backend state so datastream cannot send while idle.

- [ ] **Step 2: Run the focused host test**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test_ai_rtc_facade_agora_backend
/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend
```

Expected output:

```text
PASS: ai rtc facade agora backend
```

- [ ] **Step 3: Keep runtime behavior unchanged**

Do not modify `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.c` unless Step 2 fails. The expected current implementation is:

```c
int Ai_Rtc_Agora_Service_Send_Datastream(const uint8_t *data, size_t len)
{
    if (!s_agora.joined || s_agora.stream_id < 0 || data == NULL || len == 0u)
    {
        return AI_RTC_FACADE_ERR_NOT_READY;
    }

    return (agora_rtc_send_stream_message(s_agora.conn_id, s_agora.stream_id, (const char *)data, len) < 0)
        ? AI_RTC_FACADE_ERR_INTERNAL
        : AI_RTC_FACADE_OK;
}
```

---

### Task 3: Document Datastream-Only Closure Boundary

**Files:**
- Modify: `/root/smp/ai_iot_sdk/docs/rtc-porting-contract.md`
- Create: `/root/smp/ai_iot_sdk/docs/superpowers/reviews/2026-07-04-phase7-rtc-w15-datastream-closure.md`

- [ ] **Step 1: Update the porting contract datastream section**

Replace the current `Datastream TX` subsection with:

```markdown
Datastream TX/RX:

- Phase 7 accepts Agora RTC datastream as the AI control-message transport.
- Datastream requires a joined RTC session and backend stream readiness.
- The public facade exposes raw datastream send/RX only:
  `Ai_Rtc_Facade_Send_Datastream()` and `on_datastream_rx`.
- Product event semantics remain product-owned; products decide how raw
  messages affect UI, dialog, playback, and business state.
- SDK private code may parse only the shared Agora datastream envelope format
  for validation and reusable backend support.
- RTM is a future private backend transport option. It is not part of the
  Phase 7 public facade API and must not be mixed with datastream in the same
  AI session.
```

- [ ] **Step 2: Create W15 closure note**

Create `/root/smp/ai_iot_sdk/docs/superpowers/reviews/2026-07-04-phase7-rtc-w15-datastream-closure.md` with:

```markdown
# Phase 7 RTC W15 Datastream Closure

## Accepted Boundary

- Phase 7 uses RTC datastream as the SDK control-message transport.
- `ai_rtc_facade.h` exposes raw datastream TX/RX and does not expose RTM or
  control-transport selector APIs.
- Agora RTM support in newer vendor headers is recorded as future private
  backend work. RTM and datastream are not mixed in the AI scenario.

## Current SDK Behavior

- Agora backend creates an ordered/reliable datastream after RTC join success.
- Datastream send is rejected before join, after stop, or before backend stream
  readiness.
- Datastream RX is relayed as raw `Ai_Rtc_Facade_Datastream_Message_t`.
- SDK-private datastream envelope parser remains available and host-tested.
- Product code owns product event semantics, UI state, playback behavior, and
  business actions triggered by datastream messages.

## Verification

- `python3 test/host/check_ai_rtc_facade_public_header.py`
- `make -C test/host test`
- `make -C test/host clean`
```

---

### Task 4: Full Host Verification And Clean

**Files:**
- Test: `/root/smp/ai_iot_sdk/test/host/Makefile`

- [ ] **Step 1: Run full host verification**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test
```

Expected output includes all of:

```text
PASS: BK7258 allocator guard
PASS: RTC facade public header boundary check
PASS: RTC facade core boundary check
PASS: SDK Agora RTC migration boundary check
PASS: ai rtc facade core
PASS: ai rtc facade agora backend
PASS: ai rtc agora datastream
```

- [ ] **Step 2: Clean host binaries**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host clean
```

Expected output removes:

```text
test_bk7258_allocator_guard
test_ai_rtc_facade_core
test_ai_rtc_facade_agora_backend
test_ai_rtc_agora_datastream
```

- [ ] **Step 3: Check worktree**

Run:

```bash
git -C /root/smp/ai_iot_sdk status --short
```

Expected tracked changes are limited to:

```text
 M docs/rtc-porting-contract.md
 A docs/superpowers/plans/2026-07-04-phase7-rtc-w15-datastream-closure.md
 A docs/superpowers/reviews/2026-07-04-phase7-rtc-w15-datastream-closure.md
 M test/host/check_ai_rtc_facade_public_header.py
 M test/host/test_ai_rtc_facade_agora_backend.c
```

No host test binaries should remain.

---

## Self-Review

- Spec coverage: W15 locks datastream as Phase 7 control transport, keeps public API chip-neutral, records RTM as future private backend work, verifies datastream lifecycle behavior, and cleans host binaries.
- Placeholder scan: this plan intentionally contains no `TBD`, `TODO`, or deferred implementation placeholders.
- Type consistency: all symbols referenced already exist in `ai_rtc_facade.h`, `ai_rtc_agora_service.c`, and host tests.
