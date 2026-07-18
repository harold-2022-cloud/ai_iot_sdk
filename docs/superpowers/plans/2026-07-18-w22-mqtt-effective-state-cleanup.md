# W22 MQTT Effective State Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> Status: SDK host implementation completed on 2026-07-18. Product runtime validation is the remaining handoff step.

**Goal:** Make SDK MQTT publish, drain, and reconnect decisions use one effective state so `PROBING` does not cause false reconnects and queued publishes are not drained into unhealthy sockets.

**Architecture:** Keep `context->Is_Connected`, `s_mqtt_app_connected`, and broker liveness as raw diagnostics. Add a small SDK MQTT client helper that derives `DISCONNECTED / CONNECTING / SUBSCRIBING / READY / PROBING / DEAD` from those raw inputs. Update app publish gate, agent queue drain, and AI reconnect decisions to call the helper instead of each reinterpreting raw flags.

**Tech Stack:** C11, existing `entity_iot_sdk/entity_mqtt` module, existing `library/mi_mqtt` broker-liveness API, host `make -C test/host test`.

---

## File Structure

- Create `entity_iot_sdk/entity_mqtt/entity_mqtt_effective_state.c`
  - Owns pure effective-state derivation and predicates.
  - No product code, no RTC code, no socket calls.
  - Included automatically by `entity_iot_sdk/CMakeLists.txt` via `FILE(GLOB_RECURSE files ${DIR}/*.c)`.

- Modify `entity_iot_sdk/entity_mqtt/entity_mqtt_client.h`
  - Adds `Entity_Mqtt_Effective_State_t`.
  - Adds `Entity_Mqtt_Effective_Input_t`.
  - Declares helper functions.

- Modify `entity_iot_sdk/entity_mqtt/entity_mqtt_app.c`
  - Removes app-local broker normalize/control decisions.
  - Builds an effective-state input from app flag, pending QoS1, and broker snapshot.
  - Uses effective state for publish enqueue, queue drain, AI publish gate, AI link health, and reconnect decision.

- Create `test/host/test_entity_mqtt_effective_state.c`
  - Host unit test for state matrix.
  - Compiles only the new helper and lightweight structs.

- Modify `test/host/Makefile`
  - Adds `ENTITY_MQTT_EFFECTIVE_TARGET`.
  - Adds include paths needed by `entity_mqtt_client.h`.
  - Runs the test as part of `make -C test/host test`.
  - Cleans the new binary.

- Update `docs/superpowers/specs/2026-07-18-w22-mqtt-effective-state-design.md`
  - Record final implemented function names and validation result after code lands.

---

### Task 1: Add Failing Host Test For Effective State Matrix

**Files:**
- Create: `test/host/test_entity_mqtt_effective_state.c`
- Modify: `test/host/Makefile`

- [ ] **Step 1: Create the failing host test**

Create `test/host/test_entity_mqtt_effective_state.c`:

```c
#include "entity_mqtt_client.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int s_failures;

static void expect_state(const char *name,
                         Entity_Mqtt_Process_State_e process_state,
                         bool ctx_connected,
                         bool app_connected,
                         bool broker_ok,
                         Mqtt_Client_Broker_Liveness_t broker_state,
                         bool stale_puback,
                         Entity_Mqtt_Effective_State_t expected_state,
                         bool expected_publish,
                         bool expected_drain,
                         bool expected_reconnect)
{
    Entity_Mqtt_Context_t ctx;
    Entity_Mqtt_Effective_Input_t input;

    memset(&ctx, 0, sizeof(ctx));
    memset(&input, 0, sizeof(input));

    ctx.Mqtt_Client = (void *)0x1;
    ctx.State = (uint8_t)process_state;
    ctx.Is_Connected = ctx_connected;

    input.app_connected = app_connected;
    input.broker_ok = broker_ok;
    input.broker_state = broker_state;
    input.stale_puback = stale_puback;

    Entity_Mqtt_Effective_State_t actual =
        Entity_Mqtt_Effective_State_From_Input(&ctx, &input);

    bool can_publish = Entity_Mqtt_Effective_Can_Publish(actual);
    bool can_drain = Entity_Mqtt_Effective_Can_Drain(actual);
    bool should_reconnect = Entity_Mqtt_Effective_Should_Reconnect(actual);

    if (actual != expected_state ||
        can_publish != expected_publish ||
        can_drain != expected_drain ||
        should_reconnect != expected_reconnect)
    {
        printf("FAIL %s: state=%s expected=%s publish=%d/%d drain=%d/%d reconnect=%d/%d\n",
               name,
               Entity_Mqtt_Effective_State_Str(actual),
               Entity_Mqtt_Effective_State_Str(expected_state),
               can_publish ? 1 : 0,
               expected_publish ? 1 : 0,
               can_drain ? 1 : 0,
               expected_drain ? 1 : 0,
               should_reconnect ? 1 : 0,
               expected_reconnect ? 1 : 0);
        s_failures++;
    }
}

int main(void)
{
    expect_state("idle disconnected",
                 ENTITY_MQTT_IDLE_STATE,
                 false,
                 false,
                 true,
                 MQTT_BROKER_LIVENESS_DISCONNECTED,
                 false,
                 ENTITY_MQTT_EFFECTIVE_DISCONNECTED,
                 false,
                 false,
                 true);

    expect_state("connecting does not reconnect again",
                 ENTITY_MQTT_CONNCET_STATE,
                 false,
                 false,
                 true,
                 MQTT_BROKER_LIVENESS_DISCONNECTED,
                 false,
                 ENTITY_MQTT_EFFECTIVE_CONNECTING,
                 false,
                 false,
                 false);

    expect_state("subscribing waits",
                 ENTITY_MQTT_SUBSCRIBING_STATE,
                 false,
                 false,
                 true,
                 MQTT_BROKER_LIVENESS_READY,
                 false,
                 ENTITY_MQTT_EFFECTIVE_SUBSCRIBING,
                 false,
                 false,
                 false);

    expect_state("ready publishes and drains",
                 ENTITY_MQTT_YIELD_STATE,
                 true,
                 true,
                 true,
                 MQTT_BROKER_LIVENESS_READY,
                 false,
                 ENTITY_MQTT_EFFECTIVE_READY,
                 true,
                 true,
                 false);

    expect_state("probing does not reconnect or drain",
                 ENTITY_MQTT_YIELD_STATE,
                 true,
                 true,
                 true,
                 MQTT_BROKER_LIVENESS_PROBING,
                 false,
                 ENTITY_MQTT_EFFECTIVE_PROBING,
                 false,
                 false,
                 false);

    expect_state("dead reconnects",
                 ENTITY_MQTT_YIELD_STATE,
                 true,
                 true,
                 true,
                 MQTT_BROKER_LIVENESS_DEAD,
                 false,
                 ENTITY_MQTT_EFFECTIVE_DEAD,
                 false,
                 false,
                 true);

    expect_state("broker disconnected reconnects",
                 ENTITY_MQTT_YIELD_STATE,
                 true,
                 true,
                 true,
                 MQTT_BROKER_LIVENESS_DISCONNECTED,
                 false,
                 ENTITY_MQTT_EFFECTIVE_DEAD,
                 false,
                 false,
                 true);

    expect_state("stale puback is dead",
                 ENTITY_MQTT_YIELD_STATE,
                 true,
                 true,
                 true,
                 MQTT_BROKER_LIVENESS_READY,
                 true,
                 ENTITY_MQTT_EFFECTIVE_DEAD,
                 false,
                 false,
                 true);

    expect_state("broker snapshot unavailable falls back to ready when app and ctx are ready",
                 ENTITY_MQTT_YIELD_STATE,
                 true,
                 true,
                 false,
                 MQTT_BROKER_LIVENESS_DISCONNECTED,
                 false,
                 ENTITY_MQTT_EFFECTIVE_READY,
                 true,
                 true,
                 false);

    if (s_failures != 0)
    {
        printf("test_entity_mqtt_effective_state failed: %d failures\n", s_failures);
        return 1;
    }

    printf("test_entity_mqtt_effective_state passed\n");
    return 0;
}
```

- [ ] **Step 2: Add the test target to `test/host/Makefile`**

Patch the Makefile:

```make
CFLAGS += -I../../entity_iot_sdk/entity_mqtt -I../../entity_iot_sdk/entity_iot -I../../library/mi_mqtt

ENTITY_MQTT_EFFECTIVE_TARGET := test_entity_mqtt_effective_state
ENTITY_MQTT_EFFECTIVE_SRCS := test_entity_mqtt_effective_state.c ../../entity_iot_sdk/entity_mqtt/entity_mqtt_effective_state.c

all: $(TARGET) $(RTC_CORE_TARGET) $(RTC_AGORA_TARGET) $(RTC_AGORA_RTM_TARGET) $(RTC_AGORA_DATASTREAM_TARGET) $(ENTITY_MQTT_EFFECTIVE_TARGET)

test: $(TARGET) $(RTC_CORE_TARGET) $(RTC_AGORA_TARGET) $(RTC_AGORA_RTM_TARGET) $(RTC_AGORA_DATASTREAM_TARGET) $(ENTITY_MQTT_EFFECTIVE_TARGET) check_entity_interface_reference_template check_ai_rtc_facade_public_header check_ai_rtc_facade_core_boundary check_ai_rtc_facade_sdk_agora_migration check_ai_rtc_control_transport_buildtime check_ai_rtc_diagnostic_markers
	./$(TARGET)
	./$(RTC_CORE_TARGET)
	./$(RTC_AGORA_TARGET)
	./$(RTC_AGORA_RTM_TARGET)
	./$(RTC_AGORA_DATASTREAM_TARGET)
	./$(ENTITY_MQTT_EFFECTIVE_TARGET)

$(ENTITY_MQTT_EFFECTIVE_TARGET): $(ENTITY_MQTT_EFFECTIVE_SRCS)
	$(CC) $(CFLAGS) $(ENTITY_MQTT_EFFECTIVE_SRCS) -o $@

clean:
	rm -f $(TARGET) $(RTC_CORE_TARGET) $(RTC_AGORA_TARGET) $(RTC_AGORA_RTM_TARGET) $(RTC_AGORA_DATASTREAM_TARGET) $(ENTITY_MQTT_EFFECTIVE_TARGET)
```

When editing, merge these lines into the existing `all`, `test`, and `clean` rules rather than duplicating those rules.

- [ ] **Step 3: Run test and verify it fails**

Run:

```bash
make -C test/host test_entity_mqtt_effective_state
```

Expected result:

```text
entity_mqtt_effective_state.c: No such file or directory
```

or compile errors for undefined `Entity_Mqtt_Effective_*` types/functions.

- [ ] **Step 4: Commit the failing test**

```bash
git add test/host/test_entity_mqtt_effective_state.c test/host/Makefile
git commit -m "test(mqtt): add effective state matrix"
```

---

### Task 2: Add Client-Layer Effective State Helper

**Files:**
- Modify: `entity_iot_sdk/entity_mqtt/entity_mqtt_client.h`
- Create: `entity_iot_sdk/entity_mqtt/entity_mqtt_effective_state.c`
- Test: `test/host/test_entity_mqtt_effective_state.c`

- [ ] **Step 1: Add public-in-module types and function declarations**

Append these declarations in `entity_iot_sdk/entity_mqtt/entity_mqtt_client.h` after `struct Entity_Mqtt_Context`:

```c
typedef enum
{
    ENTITY_MQTT_EFFECTIVE_DISCONNECTED = 0,
    ENTITY_MQTT_EFFECTIVE_CONNECTING,
    ENTITY_MQTT_EFFECTIVE_SUBSCRIBING,
    ENTITY_MQTT_EFFECTIVE_READY,
    ENTITY_MQTT_EFFECTIVE_PROBING,
    ENTITY_MQTT_EFFECTIVE_DEAD,
} Entity_Mqtt_Effective_State_t;

typedef struct
{
    bool app_connected;
    bool broker_ok;
    Mqtt_Client_Broker_Liveness_t broker_state;
    bool stale_puback;
} Entity_Mqtt_Effective_Input_t;

const char *Entity_Mqtt_Effective_State_Str(Entity_Mqtt_Effective_State_t state);
Entity_Mqtt_Effective_State_t Entity_Mqtt_Effective_State_From_Input(const Entity_Mqtt_Context_t *context,
                                                                      const Entity_Mqtt_Effective_Input_t *input);
bool Entity_Mqtt_Effective_Can_Publish(Entity_Mqtt_Effective_State_t state);
bool Entity_Mqtt_Effective_Can_Drain(Entity_Mqtt_Effective_State_t state);
bool Entity_Mqtt_Effective_Should_Reconnect(Entity_Mqtt_Effective_State_t state);
```

- [ ] **Step 2: Implement the helper**

Create `entity_iot_sdk/entity_mqtt/entity_mqtt_effective_state.c`:

```c
#include "entity_mqtt_client.h"

const char *Entity_Mqtt_Effective_State_Str(Entity_Mqtt_Effective_State_t state)
{
    switch (state)
    {
        case ENTITY_MQTT_EFFECTIVE_DISCONNECTED: return "DISCONNECTED";
        case ENTITY_MQTT_EFFECTIVE_CONNECTING:   return "CONNECTING";
        case ENTITY_MQTT_EFFECTIVE_SUBSCRIBING:  return "SUBSCRIBING";
        case ENTITY_MQTT_EFFECTIVE_READY:        return "READY";
        case ENTITY_MQTT_EFFECTIVE_PROBING:      return "PROBING";
        case ENTITY_MQTT_EFFECTIVE_DEAD:         return "DEAD";
        default:                                 return "UNKNOWN";
    }
}

Entity_Mqtt_Effective_State_t Entity_Mqtt_Effective_State_From_Input(const Entity_Mqtt_Context_t *context,
                                                                      const Entity_Mqtt_Effective_Input_t *input)
{
    if (context == NULL || context->Mqtt_Client == NULL)
    {
        return ENTITY_MQTT_EFFECTIVE_DISCONNECTED;
    }

    switch ((Entity_Mqtt_Process_State_e)context->State)
    {
        case ENTITY_MQTT_IDLE_STATE:
            return ENTITY_MQTT_EFFECTIVE_DISCONNECTED;

        case ENTITY_MQTT_CONNCET_STATE:
        case ENTITY_MQTT_RECONNECT_STATE:
            return ENTITY_MQTT_EFFECTIVE_CONNECTING;

        case ENTITY_MQTT_SUBSCRIBING_STATE:
        case ENTITY_MQTT_SUBSCRIBE_COMPLETE_STATE:
            return ENTITY_MQTT_EFFECTIVE_SUBSCRIBING;

        case ENTITY_MQTT_YIELD_STATE:
        default:
            break;
    }

    if (!context->Is_Connected)
    {
        return ENTITY_MQTT_EFFECTIVE_DISCONNECTED;
    }

    if (input == NULL || !input->app_connected)
    {
        return ENTITY_MQTT_EFFECTIVE_DISCONNECTED;
    }

    if (input->stale_puback)
    {
        return ENTITY_MQTT_EFFECTIVE_DEAD;
    }

    if (!input->broker_ok)
    {
        return ENTITY_MQTT_EFFECTIVE_READY;
    }

    switch (input->broker_state)
    {
        case MQTT_BROKER_LIVENESS_READY:
            return ENTITY_MQTT_EFFECTIVE_READY;

        case MQTT_BROKER_LIVENESS_PROBING:
            return ENTITY_MQTT_EFFECTIVE_PROBING;

        case MQTT_BROKER_LIVENESS_DEAD:
        case MQTT_BROKER_LIVENESS_DISCONNECTED:
            return ENTITY_MQTT_EFFECTIVE_DEAD;

        case MQTT_BROKER_LIVENESS_CONNECTING:
            return ENTITY_MQTT_EFFECTIVE_CONNECTING;

        case MQTT_BROKER_LIVENESS_SUBSCRIBING:
            return ENTITY_MQTT_EFFECTIVE_SUBSCRIBING;

        default:
            return ENTITY_MQTT_EFFECTIVE_DEAD;
    }
}

bool Entity_Mqtt_Effective_Can_Publish(Entity_Mqtt_Effective_State_t state)
{
    return state == ENTITY_MQTT_EFFECTIVE_READY;
}

bool Entity_Mqtt_Effective_Can_Drain(Entity_Mqtt_Effective_State_t state)
{
    return state == ENTITY_MQTT_EFFECTIVE_READY;
}

bool Entity_Mqtt_Effective_Should_Reconnect(Entity_Mqtt_Effective_State_t state)
{
    return state == ENTITY_MQTT_EFFECTIVE_DISCONNECTED ||
           state == ENTITY_MQTT_EFFECTIVE_DEAD;
}
```

- [ ] **Step 3: Run the host matrix test**

Run:

```bash
make -C test/host test_entity_mqtt_effective_state
./test/host/test_entity_mqtt_effective_state
```

Expected:

```text
test_entity_mqtt_effective_state passed
```

- [ ] **Step 4: Commit the helper**

```bash
git add entity_iot_sdk/entity_mqtt/entity_mqtt_client.h entity_iot_sdk/entity_mqtt/entity_mqtt_effective_state.c
git commit -m "fix(mqtt): add effective state helper"
```

---

### Task 3: Change App Publish Gate To Effective State

**Files:**
- Modify: `entity_iot_sdk/entity_mqtt/entity_mqtt_app.c`
- Test: `test/host/test_entity_mqtt_effective_state.c`

- [ ] **Step 1: Add an app-local helper to collect input**

Add this helper near `Entity_Mqtt_App_Normalize_Broker_State()` before removing that function:

```c
static Entity_Mqtt_Effective_State_t Entity_Mqtt_App_Get_Effective_State(Entity_Mqtt_Context_t *context,
                                                                         bool stale_puback,
                                                                         Mqtt_Client_Broker_Liveness_Snapshot_t *broker_out,
                                                                         bool *broker_ok_out)
{
    Mqtt_Client_Broker_Liveness_Snapshot_t broker = {0};
    bool broker_ok = false;
    Entity_Mqtt_Effective_Input_t input = {0};

    if (context != NULL && context->Mqtt_Client != NULL)
    {
        broker_ok = Mqtt_Client_Broker_Liveness_Snapshot(context->Mqtt_Client, &broker);
    }

    input.app_connected = s_mqtt_app_connected;
    input.broker_ok = broker_ok;
    input.broker_state = broker.state;
    input.stale_puback = stale_puback;

    if (broker_out != NULL)
    {
        *broker_out = broker;
    }
    if (broker_ok_out != NULL)
    {
        *broker_ok_out = broker_ok;
    }

    return Entity_Mqtt_Effective_State_From_Input(context, &input);
}
```

- [ ] **Step 2: Replace publish enqueue gate**

In `Entity_Mqtt_App_Topic_Publish()`, replace the `if (!s_mqtt_app_connected)` block with:

```c
Entity_Mqtt_Effective_State_t effective =
    Entity_Mqtt_App_Get_Effective_State(&Entity_Client_Instance, false, NULL, NULL);

if (!Entity_Mqtt_Effective_Can_Publish(effective))
{
    ENTITY_LOGW("[MQTT_PUBLISH_GATE] accept=0 effective=%s raw_app_connected=%d "
                "raw_ctx_connected=%d state=%s queue_depth=%u len=%d qos=%d\r\n",
                Entity_Mqtt_Effective_State_Str(effective),
                s_mqtt_app_connected ? 1 : 0,
                Entity_Client_Instance.Is_Connected ? 1 : 0,
                Entity_Mqtt_App_State_Str(Entity_Client_Instance.State),
                s_agent_queue ? (unsigned int)Entity_Msg_Queue_Get_Msg_Num(&s_agent_queue) : 0,
                len,
                qos);
    return OPRT_COM_ERROR;
}
```

- [ ] **Step 3: Run host tests**

Run:

```bash
make -C test/host test
```

Expected:

```text
test_entity_mqtt_effective_state passed
```

and existing RTC/guard tests pass.

- [ ] **Step 4: Commit publish gate**

```bash
git add entity_iot_sdk/entity_mqtt/entity_mqtt_app.c
git commit -m "fix(mqtt): gate publish enqueue by effective state"
```

---

### Task 4: Change Agent Queue Drain To Effective State

**Files:**
- Modify: `entity_iot_sdk/entity_mqtt/entity_mqtt_app.c`

- [ ] **Step 1: Replace drain precondition**

In `Entity_Mqtt_App_Drain_Agent_Queue()`, replace:

```c
if (entity_context == NULL || !entity_context->Is_Connected || s_agent_queue == NULL)
{
    return;
}
```

with:

```c
if (entity_context == NULL || s_agent_queue == NULL)
{
    return;
}

Entity_Mqtt_Effective_State_t effective =
    Entity_Mqtt_App_Get_Effective_State(entity_context, false, NULL, NULL);

if (!Entity_Mqtt_Effective_Can_Drain(effective))
{
    uint32_t queue_depth = (uint32_t)Entity_Msg_Queue_Get_Msg_Num(&s_agent_queue);
    if (queue_depth > 0)
    {
        ENTITY_MQTT_VERBOSE_LOGI("[MQTT_DRAIN_GATE] drain=0 effective=%s raw_ctx_connected=%d "
                                 "raw_app_connected=%d state=%s queue_depth=%u\r\n",
                                 Entity_Mqtt_Effective_State_Str(effective),
                                 entity_context->Is_Connected ? 1 : 0,
                                 s_mqtt_app_connected ? 1 : 0,
                                 Entity_Mqtt_App_State_Str(entity_context->State),
                                 (unsigned int)queue_depth);
    }
    return;
}
```

- [ ] **Step 2: Run host tests**

Run:

```bash
make -C test/host test
```

Expected: all host tests pass.

- [ ] **Step 3: Commit drain gate**

```bash
git add entity_iot_sdk/entity_mqtt/entity_mqtt_app.c
git commit -m "fix(mqtt): gate agent drain by effective state"
```

---

### Task 5: Change AI Publish And Reconnect Decision To Effective State

**Files:**
- Modify: `entity_iot_sdk/entity_mqtt/entity_mqtt_app.c`

- [ ] **Step 1: Remove app-local broker normalize usage**

Stop calling:

```c
broker.state = Entity_Mqtt_App_Normalize_Broker_State(context, broker.state);
```

from:

```text
Entity_Mqtt_App_Prepare_Ai_Publish()
Entity_Mqtt_App_Is_Ai_Link_Healthy()
Entity_Mqtt_App_Get_Health_Snapshot()
```

Use `Entity_Mqtt_App_Get_Effective_State()` instead.

- [ ] **Step 2: Update `Entity_Mqtt_App_Prepare_Ai_Publish()`**

Replace the broker-ready branch and reconnect calculation with:

```c
broker_ok = false;
Entity_Mqtt_Effective_State_t effective =
    Entity_Mqtt_App_Get_Effective_State(context, stale_puback, &broker, &broker_ok);

if (Entity_Mqtt_Effective_Can_Publish(effective))
{
    ENTITY_LOGI("[MQTT_EFFECTIVE_STATE] stage=ai_publish_ready effective=%s broker_ok=%d "
                "broker=%s raw_ctx_connected=%d raw_app_connected=%d stale_puback=%d "
                "keepalive=%u pingresp_timeout=%u last_inbound_age=%u wait_ping=%d "
                "pingreq_age=%u pingresp_rtt=%u last_rx_age=%u pending_qos1=%u "
                "oldest_msgid=%u oldest_age=%u\r\n",
                Entity_Mqtt_Effective_State_Str(effective),
                broker_ok ? 1 : 0,
                Mqtt_Client_Broker_Liveness_Str(broker.state),
                context->Is_Connected ? 1 : 0,
                s_mqtt_app_connected ? 1 : 0,
                stale_puback ? 1 : 0,
                (unsigned int)broker.keepalive_ms,
                (unsigned int)broker.pingresp_timeout_ms,
                (unsigned int)broker.last_inbound_alive_age_ms,
                broker.wait_ping ? 1 : 0,
                (unsigned int)broker.pingreq_age_ms,
                (unsigned int)broker.last_pingresp_rtt_ms,
                (unsigned int)last_rx_age_ms,
                (unsigned int)pending_count,
                (unsigned int)oldest_msg_id,
                (unsigned int)oldest_age_ms);
    return true;
}

bool request_reconnect = Entity_Mqtt_Effective_Should_Reconnect(effective);
ENTITY_LOGW("[MQTT_RECONNECT_DECISION] stage=ai_publish effective=%s request_reconnect=%d "
            "broker_ok=%d broker=%s raw_ctx_connected=%d raw_app_connected=%d "
            "stale_puback=%d wait_ping=%d pingreq_age=%u last_rx_age=%u "
            "pending_qos1=%u oldest_msgid=%u oldest_age=%u\r\n",
            Entity_Mqtt_Effective_State_Str(effective),
            request_reconnect ? 1 : 0,
            broker_ok ? 1 : 0,
            Mqtt_Client_Broker_Liveness_Str(broker.state),
            context->Is_Connected ? 1 : 0,
            s_mqtt_app_connected ? 1 : 0,
            stale_puback ? 1 : 0,
            broker.wait_ping ? 1 : 0,
            (unsigned int)broker.pingreq_age_ms,
            (unsigned int)last_rx_age_ms,
            (unsigned int)pending_count,
            (unsigned int)oldest_msg_id,
            (unsigned int)oldest_age_ms);
```

Keep the existing `Entity_Mqtt_Force_Reconnect()` call only inside:

```c
if (request_reconnect)
{
    const char *reason = stale_puback ? "ai_publish_pending_qos1" :
                         (effective == ENTITY_MQTT_EFFECTIVE_DEAD) ? "ai_publish_broker_dead" :
                         "ai_publish_disconnected";
    Entity_Mqtt_App_Log_Reconnect_Pending_Snapshot("ai_publish_not_ready", reason);
    s_mqtt_app_connected = false;
    (void)Entity_Mqtt_Force_Reconnect(context, reason);
}
```

This removes `PROBING` as a reconnect reason.

- [ ] **Step 3: Update `Entity_Mqtt_App_Is_Ai_Link_Healthy()`**

Use:

```c
Entity_Mqtt_Effective_State_t effective =
    Entity_Mqtt_App_Get_Effective_State(context, stale_puback, &broker, &broker_ok);
healthy = Entity_Mqtt_Effective_Can_Publish(effective);
```

Log `effective=%s` in the existing `[MQTT_DIAG][AI_LINK_HEALTH]` line.

- [ ] **Step 4: Update health snapshot**

In `Entity_Mqtt_App_Get_Health_Snapshot()`, keep raw fields and store normalized broker state by using the helper result for app decision logs. If the public snapshot struct has no effective field, do not change the public struct in this task; log effective state from app call sites instead.

- [ ] **Step 5: Run host tests**

Run:

```bash
make -C test/host test
```

Expected: all host tests pass.

- [ ] **Step 6: Commit AI/reconnect decision**

```bash
git add entity_iot_sdk/entity_mqtt/entity_mqtt_app.c
git commit -m "fix(mqtt): centralize ai reconnect decision"
```

---

### Task 6: Add Static Guard For PROBING And Raw-State Drift

**Files:**
- Create: `test/host/check_entity_mqtt_effective_state_policy.py`
- Modify: `test/host/Makefile`

- [ ] **Step 1: Add static policy check**

Create `test/host/check_entity_mqtt_effective_state_policy.py`:

```python
#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
APP = ROOT / "entity_iot_sdk" / "entity_mqtt" / "entity_mqtt_app.c"
HELPER = ROOT / "entity_iot_sdk" / "entity_mqtt" / "entity_mqtt_effective_state.c"


def fail(message: str) -> None:
    print(f"FAIL: {message}")
    raise SystemExit(1)


app = APP.read_text(encoding="utf-8")
helper = HELPER.read_text(encoding="utf-8")

if "Entity_Mqtt_App_Normalize_Broker_State" in app:
    fail("app layer still owns broker normalize/reconcile")

if "broker.state == MQTT_BROKER_LIVENESS_PROBING" in app:
    fail("app layer still branches directly on PROBING")

if "Entity_Mqtt_Effective_Should_Reconnect" not in app:
    fail("app layer reconnect decision must use effective state helper")

if "case MQTT_BROKER_LIVENESS_PROBING" not in helper:
    fail("effective state helper must explicitly map PROBING")

probing_block = helper.split("case MQTT_BROKER_LIVENESS_PROBING", 1)[1].split("case ", 1)[0]
if "ENTITY_MQTT_EFFECTIVE_PROBING" not in probing_block:
    fail("PROBING must map to ENTITY_MQTT_EFFECTIVE_PROBING")

reconnect_func = helper.split("Entity_Mqtt_Effective_Should_Reconnect", 1)[1]
if "ENTITY_MQTT_EFFECTIVE_PROBING" in reconnect_func:
    fail("PROBING must not trigger reconnect")

print("entity mqtt effective-state policy check passed")
```

- [ ] **Step 2: Wire the policy check into Makefile**

Add phony target:

```make
.PHONY: check_entity_mqtt_effective_state_policy

test: ... check_entity_mqtt_effective_state_policy

check_entity_mqtt_effective_state_policy:
	python3 check_entity_mqtt_effective_state_policy.py
```

Merge into the existing `.PHONY` and `test` lines.

- [ ] **Step 3: Run policy check**

Run:

```bash
make -C test/host check_entity_mqtt_effective_state_policy
```

Expected:

```text
entity mqtt effective-state policy check passed
```

- [ ] **Step 4: Commit static guard**

```bash
git add test/host/check_entity_mqtt_effective_state_policy.py test/host/Makefile
git commit -m "test(mqtt): guard effective state policy"
```

---

### Task 7: Host Verification And Clean

**Files:**
- No new files

- [ ] **Step 1: Run full host verification**

Run:

```bash
make -C test/host test
```

Expected:

```text
test_entity_mqtt_effective_state passed
entity mqtt effective-state policy check passed
```

Existing RTC host binaries and guard scripts must also pass.

- [ ] **Step 2: Clean host binaries**

Run:

```bash
make -C test/host clean
```

Expected:

```text
rm -f ...
```

- [ ] **Step 3: Confirm no host binaries remain**

Run:

```bash
git status --short
```

Expected:

```text
```

No generated host binaries should appear. Existing unrelated dirty files may remain if they were already present before W22.

- [ ] **Step 4: Commit verification note**

If the implementation commits already include all changes, do not create an empty commit. If docs/spec was updated with implementation names or verification result, commit only that doc:

```bash
git add docs/superpowers/specs/2026-07-18-w22-mqtt-effective-state-design.md
git commit -m "docs(mqtt): record effective state verification"
```

---

### Task 8: Product Runtime Validation

**Files:**
- Product SDK baseline updates only after SDK host tests pass

- [ ] **Step 1: Sync SDK to Beken R1 product**

Copy the verified SDK baseline into the product SDK/submodule workflow used by `/root/smp/bk7258-r1-ai-device`.

Record the SDK commit hash:

```bash
git -C /mnt/c/Users/harold.chen/ai_iot_sdk rev-parse --short HEAD
```

- [ ] **Step 2: Build Beken R1**

Run from product repo:

```bash
cd /root/smp/bk7258-r1-ai-device/projects/beken_genie_r1
make clean
./build.sh
```

Expected: build succeeds and produces `build/bk7258/beken_genie_r1/package/all-app.bin`.

- [ ] **Step 3: Validate Beken R1 runtime**

Runtime log must show:

```text
[MQTT_EFFECTIVE_STATE] ... effective=READY
[MQTT_PUBLISH_GATE] accept path only when effective=READY
[MQTT_RECONNECT_DECISION] ... effective=PROBING request_reconnect=0
```

AI validation:

- first AI dialog succeeds,
- repeated AI start/stop succeeds,
- no `broker=PROBING` immediate reconnect,
- no AI token timeout caused by queued publish draining into a dead socket.

- [ ] **Step 4: Validate SpeedTech Wi-Fi**

Build and run `/root/smp/bk7258-r1-ai-device/projects/beken_genie_speedtech`.

Expected:

- Wi-Fi MQTT connects,
- AI token request still reaches server,
- no 4G auto bring-up is required for this validation,
- `PROBING` does not force reconnect.

- [ ] **Step 5: Validate ESP32S3 ai_alarm**

After syncing SDK into `/mnt/c/Users/harold.chen/esp32s3_ai_alarm/sdk`, user compiles in Windows CMD.

Expected:

- AI dialog succeeds,
- MQTT token request path still works,
- no RTC public API change is required.

---

## Self-Review

Spec coverage:

- Raw inputs are preserved and treated as diagnostics: Tasks 2, 3, 5.
- Effective state is introduced: Task 2.
- Publish gate uses effective state: Task 3.
- Queue drain uses effective state: Task 4.
- `PROBING` does not reconnect: Tasks 2, 5, 6.
- `DEAD` reconnects: Tasks 2, 5.
- `READY` does not wait: Tasks 2, 3, 5.
- Low-frequency marker contract is covered: Task 5.
- Host/static verification is covered: Tasks 1, 6, 7.
- Product runtime validation is covered: Task 8.

Risk notes:

- This plan intentionally keeps raw diagnostics visible in logs.
- The effective helper is pure and testable.
- The first implementation must not change QoS, topic, keepalive, or RTC behavior.
