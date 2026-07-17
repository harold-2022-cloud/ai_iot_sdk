# Phase 7 RTC W16b Private Control Transport Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate Agora RTM as an SDK-private control-message transport while preserving the C public facade and the existing datastream runtime path.

**Architecture:** Public `ai_rtc_facade.h` remains C-only and does not expose Agora RTM types, functions, or transport selectors. The Agora backend gains a C private control transport layer with datastream and RTM implementations behind the same send/RX contract. Datastream remains the default accepted Phase 7 runtime path; RTM is selectable by SDK private build/port configuration and requires neutral user/peer control identifiers from the token result.

**Tech Stack:** C11, Agora IoT RTC C API, host C stubs, Python static guards, SDK host `make`.

---

## Reference From `/mnt/c/Users/harold.chen/esp32-2`

The reference project uses C++ product glue, but the SDK implementation must be C.

Relevant behavior:

- `agora_rtc_init()` enables `option.use_string_uid = true`.
- Server returns `rtc.uid` and `agent_uid`.
- RTM local UID is `rtc.uid`.
- RTM peer UID is `agent_uid`.
- RTM login uses the RTC token from server.
- RTM readiness is `RTM_EVENT_TYPE_LOGIN && ERR_RTM_OK`.
- RTC media join uses `agora_rtc_join_channel_with_user_account()`.
- Audio uses RTC media.
- Text/JSON control uses RTM.

SDK must not import:

- C++ `Protocol` class
- `std::string`, `std::vector`, `std::unique_ptr`
- product `DeviceApiClient`
- `Board`, `Application`, `Lang`
- product AEC reference ring buffer
- product audio service callbacks

---

## File Structure

- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/include/ai_rtc_facade.h`
  - Add optional transport-neutral C fields to the token result.
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade.c`
  - Copy and validate the new optional token fields.
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade_backend.h`
  - Pass optional user/control identifiers to the private backend.
- Create: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_control_transport.h`
  - Private C interface for control transports.
- Create: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_control_datastream.c`
  - Existing datastream behavior behind the private transport interface.
- Create: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_control_rtm.c`
  - RTM behavior behind the private transport interface.
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.c`
  - Use the control transport interface instead of hard-coding datastream send/create in service code.
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/CMakeLists.txt`
  - Compile the private control transport files.
- Modify: `/root/smp/ai_iot_sdk/test/host/stubs/agora_rtc_api.h`
  - Add host RTM types and C API declarations.
- Modify: `/root/smp/ai_iot_sdk/test/host/stubs/agora_rtc_stub.c`
  - Add host RTM call recording and callback emit helpers.
- Modify: `/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend.c`
  - Add RTM transport tests and keep datastream tests green.
- Modify: `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_public_header.py`
  - Ensure public header still has no RTM/vendor leakage.
- Modify: `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_sdk_agora_migration.py`
  - Ensure private RTM implementation exists and public facade remains transport-neutral.
- Modify: `/root/smp/ai_iot_sdk/test/host/Makefile`
  - Build new C files into host Agora backend tests.

---

## Public API Rule

The public API must stay C-only and must not expose RTM.

Allowed optional neutral fields:

```c
typedef struct
{
    int result;
    const char *rtc_token;
    const char *channel_name;
    const char *app_id;
    int uid;
    const char *user_account;
    const char *control_peer_id;
    const char *control_token;
} Ai_Rtc_Facade_Token_Result_t;
```

Rules:

- `user_account == NULL` keeps the existing numeric-UID RTC join path.
- `user_account != NULL` lets the Agora backend use string UID join when the selected private transport needs it.
- `control_peer_id == NULL` keeps datastream behavior.
- `control_peer_id != NULL` is required when private RTM transport is selected.
- `control_token == NULL` means reuse `rtc_token`.
- Public names must not include `rtm`, `agora`, or vendor types.

---

## Private Transport Selection

Selection is private SDK configuration, not a public facade API.

Default:

```c
#define AI_RTC_AGORA_CONTROL_TRANSPORT_DATASTREAM 1
#define AI_RTC_AGORA_CONTROL_TRANSPORT_RTM 2

#ifndef AI_RTC_AGORA_CONTROL_TRANSPORT
#define AI_RTC_AGORA_CONTROL_TRANSPORT AI_RTC_AGORA_CONTROL_TRANSPORT_DATASTREAM
#endif
```

Products may select RTM by private build definition:

```cmake
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    AI_RTC_AGORA_CONTROL_TRANSPORT=AI_RTC_AGORA_CONTROL_TRANSPORT_RTM)
```

No `AI_RTC_AGORA_CONTROL_TRANSPORT` symbol may appear in `ai_rtc_facade.h`.

---

## Task 1: Extend Host Agora Stub With RTM C API

**Files:**
- Modify: `/root/smp/ai_iot_sdk/test/host/stubs/agora_rtc_api.h`
- Modify: `/root/smp/ai_iot_sdk/test/host/stubs/agora_rtc_stub.c`

- [x] Add C enum/types matching the vendor header subset:

```c
typedef enum
{
    RTM_EVENT_TYPE_LOGIN = 0,
    RTM_EVENT_TYPE_KICKOFF = 1,
    RTM_EVENT_TYPE_EXIT = 2,
} rtm_event_type_e;

typedef enum
{
    RTM_MSG_STATE_INIT = 0,
    RTM_MSG_STATE_RECEIVED,
    RTM_MSG_STATE_UNREACHABLE,
    RTM_MSG_STATE_TIMEOUT,
} rtm_msg_state_e;

typedef enum
{
    ERR_RTM_OK = 0,
    ERR_RTM_FAILED = 1,
} rtm_err_code_e;

typedef struct
{
    void (*on_rtm_event)(const char *rtm_uid, rtm_event_type_e event_type, rtm_err_code_e err_code);
    void (*on_rtm_data)(const char *rtm_uid, const void *msg, size_t msg_len, const char *custom_type);
    void (*on_rtm_send_data_result)(const char *rtm_uid, uint32_t msg_id, rtm_msg_state_e state);
} agora_rtm_handler_t;
```

- [x] Add stub declarations:

```c
int agora_rtc_login_rtm(const char *rtm_uid, const char *rtm_token, const agora_rtm_handler_t *handler);
int agora_rtc_logout_rtm(void);
int agora_rtc_send_rtm_data(const char *rtm_uid,
                            const void *msg,
                            size_t msg_len,
                            uint32_t msg_id,
                            const char *custom_type);
void Agora_Rtc_Stub_Emit_Rtm_Login_Ok(void);
void Agora_Rtc_Stub_Emit_Rtm_Data(const char *rtm_uid, const void *msg, size_t msg_len);
```

- [x] Extend `Agora_Rtc_Stub_State_t` with:

```c
int login_rtm_calls;
int logout_rtm_calls;
int send_rtm_calls;
const char *last_rtm_uid;
const char *last_rtm_token;
const char *last_rtm_peer_uid;
const void *last_rtm_data;
size_t last_rtm_len;
uint32_t last_rtm_msg_id;
```

Verification: `make -C /root/smp/ai_iot_sdk/test/host test` passed, then `make -C /root/smp/ai_iot_sdk/test/host clean` removed host binaries.

---

## Task 2: Add Optional Neutral Token Fields

**Files:**
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/include/ai_rtc_facade.h`
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade.c`
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade_backend.h`

- [x] Add optional `user_account`, `control_peer_id`, and `control_token` fields to `Ai_Rtc_Facade_Token_Result_t`.
- [x] Copy these optional strings in facade context with bounded buffers.
- [x] Pass them to `Ai_Rtc_Facade_Backend_Start_Config_t`.
- [x] Existing users that only set `uid` must retain current behavior.

Expected public-header guard behavior:

- Pass: `user_account`, `control_peer_id`, `control_token`
- Fail: `rtm`, `agora_rtm`, `agora_rtc_login_rtm`, `AI_RTC_CONTROL_TRANSPORT`

Verification: `make -C /root/smp/ai_iot_sdk/test/host test` passed, then `make -C /root/smp/ai_iot_sdk/test/host clean` removed host binaries.

---

## Task 3: Add Private C Control Transport Interface

**Files:**
- Create: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_control_transport.h`

- [ ] Define C-only private config/state:

```c
typedef struct
{
    connection_id_t conn_id;
    const char *rtc_token;
    const char *user_account;
    const char *control_peer_id;
    const char *control_token;
} Ai_Rtc_Agora_Control_Config_t;

typedef struct
{
    int stream_id;
    bool rtm_logged_in;
    uint32_t next_rtm_msg_id;
    const char *control_peer_id;
} Ai_Rtc_Agora_Control_State_t;
```

- [x] Define C-only private config/state.

Note: `control_peer_id` is stored in private state so the later RTM `Send()` path can route to the server-provided peer without adding transport/vendor concepts to the public facade.

- [x] Define private functions:

```c
void Ai_Rtc_Agora_Control_Reset(Ai_Rtc_Agora_Control_State_t *state);
int Ai_Rtc_Agora_Control_Start(Ai_Rtc_Agora_Control_State_t *state,
                               const Ai_Rtc_Agora_Control_Config_t *config);
int Ai_Rtc_Agora_Control_On_Rtc_Joined(Ai_Rtc_Agora_Control_State_t *state,
                                       connection_id_t conn_id);
int Ai_Rtc_Agora_Control_Stop(Ai_Rtc_Agora_Control_State_t *state);
bool Ai_Rtc_Agora_Control_Is_Ready(const Ai_Rtc_Agora_Control_State_t *state);
int Ai_Rtc_Agora_Control_Send(Ai_Rtc_Agora_Control_State_t *state,
                              connection_id_t conn_id,
                              const uint8_t *data,
                              size_t len);
```

Verification: `make -C /root/smp/ai_iot_sdk/test/host test` passed, then `make -C /root/smp/ai_iot_sdk/test/host clean` removed host binaries.

---

## Task 4: Move Datastream Behind Private Transport

**Files:**
- Create: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_control_datastream.c`
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.c`

- [x] Move existing datastream behavior into the private transport:
  - create stream after RTC joined
  - send with `agora_rtc_send_stream_message`
  - ready when `stream_id >= 0`
  - reset on stop
- [x] Keep `on_stream_message()` raw facade relay unchanged.
- [x] Existing datastream host tests must keep passing.

Verification: `make -C /root/smp/ai_iot_sdk/test/host test` passed, then `make -C /root/smp/ai_iot_sdk/test/host clean` removed host binaries.

---

## Task 5: Add RTM Private Transport

**Files:**
- Create: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_control_rtm.c`
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.c`

- [x] RTM start validates:
  - `user_account != NULL`
  - `control_peer_id != NULL`
  - token is `control_token` if present, otherwise `rtc_token`
- [x] RTM start calls:

```c
agora_rtc_login_rtm(user_account, token, &handler);
```

- [x] RTM event callback:
  - `RTM_EVENT_TYPE_LOGIN && ERR_RTM_OK`: set `rtm_logged_in = true`
  - `RTM_EVENT_TYPE_KICKOFF`: clear `rtm_logged_in`
  - `RTM_EVENT_TYPE_EXIT`: clear `rtm_logged_in`
- [x] RTM send gates on `rtm_logged_in` and calls:

```c
agora_rtc_send_rtm_data(control_peer_id, data, len, ++next_rtm_msg_id, NULL);
```

- [x] RTM RX maps to the same facade raw control/datastream callback shape:

```c
Ai_Rtc_Facade_Datastream_Message_t message = {
    .stream_id = -1,
    .sender_uid = 0,
    .data = msg,
    .len = msg_len,
    .sent_ts = 0,
};
Ai_Rtc_Facade_Backend_Notify_Datastream_Rx(&message);
```

- [x] Stop calls `agora_rtc_logout_rtm()` only if RTM logged in or login was started.

Verification: `make -C /root/smp/ai_iot_sdk/test/host clean`, then `make -C /root/smp/ai_iot_sdk/test/host test` passed from a clean host build, then `make -C /root/smp/ai_iot_sdk/test/host clean` removed host binaries. The host test set now includes default datastream and `test_ai_rtc_facade_agora_backend_rtm`.

---

## Task 6: Support String UID Join When User Account Is Provided

**Files:**
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.c`
- Modify: `/root/smp/ai_iot_sdk/test/host/stubs/agora_rtc_api.h`
- Modify: `/root/smp/ai_iot_sdk/test/host/stubs/agora_rtc_stub.c`

- [x] Add host stub for:

```c
int agora_rtc_join_channel_with_user_account(connection_id_t conn_id,
                                             const char *channel_name,
                                             const char *user_account,
                                             const char *token,
                                             rtc_channel_options_t *options);
```

- [x] Backend join rule:
  - if `user_account` is non-empty: call `agora_rtc_join_channel_with_user_account`
  - else: call existing `agora_rtc_join_channel`
- [x] Datastream path with numeric UID remains unchanged when `user_account == NULL`.

Verification: `make -C /root/smp/ai_iot_sdk/test/host clean`, then `make -C /root/smp/ai_iot_sdk/test/host test` passed, then `make -C /root/smp/ai_iot_sdk/test/host clean` removed host binaries.

---

## Task 7: Host Tests

**Files:**
- Modify: `/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend.c`

- [x] Datastream default tests:
  - default transport still creates data stream after join
  - default transport still sends via `agora_rtc_send_stream_message`
  - stop resets stream state

- [x] RTM selected tests with `CFLAGS_EXTRA=-DAI_RTC_AGORA_CONTROL_TRANSPORT=AI_RTC_AGORA_CONTROL_TRANSPORT_RTM`:
  - start calls `agora_rtc_login_rtm(user_account, token, handler)`
  - send before RTM login returns `AI_RTC_FACADE_ERR_NOT_READY`
  - `Agora_Rtc_Stub_Emit_Rtm_Login_Ok()` makes control transport ready
  - send after RTM login calls `agora_rtc_send_rtm_data(control_peer_id, data, len, msg_id, NULL)`
  - RTM RX calls the same public `on_datastream_rx`
  - stop calls `agora_rtc_logout_rtm`

Verification: `make -C /root/smp/ai_iot_sdk/test/host clean && make -C /root/smp/ai_iot_sdk/test/host test && make -C /root/smp/ai_iot_sdk/test/host clean` passed.

---

## Task 8: Static Guards And Verification

**Files:**
- Modify: `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_public_header.py`
- Modify: `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_sdk_agora_migration.py`
- Modify: `/root/smp/ai_iot_sdk/test/host/Makefile`

- [x] Public header guard:
  - reject `rtm`
  - reject `agora_rtm`
  - reject `agora_rtc_login_rtm`
  - reject `AI_RTC_CONTROL_TRANSPORT`
  - allow neutral `control_peer_id`

- [x] SDK migration guard:
  - require private RTM source file
  - require private control transport header
  - require RTM calls only under `media/rtc_facade/src/agora`
  - require `ai_rtc_facade.h` has no RTM/vendor tokens

- [x] Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test
make -C /root/smp/ai_iot_sdk/test/host clean
```

Expected:

```text
PASS: ai rtc facade public header
PASS: SDK Agora RTC migration boundary check
PASS: ai rtc facade agora backend
PASS: ai rtc agora datastream
```

Verification: full host output included `PASS: ai rtc facade public header`, `PASS: SDK Agora RTC migration boundary check`, `PASS: ai rtc facade agora backend`, `PASS: ai rtc facade agora backend rtm`, and `PASS: ai rtc agora datastream`.

---

## Open Runtime Validation

Host tests can prove C API mapping and state gates. Product runtime still needs user-run ESP32S3 validation because RTM depends on server token/account behavior and vendor binary behavior.

Required runtime log evidence:

- RTM login requested with local user account.
- RTM login success event received.
- RTC joined with user account.
- Audio TX/RX still works.
- Control message TX goes through RTM when selected.
- Control message RX reaches product callback.
- Stop logs RTC leave/destroy and RTM logout.
- Repeated start/stop has no stale callbacks or crash.
