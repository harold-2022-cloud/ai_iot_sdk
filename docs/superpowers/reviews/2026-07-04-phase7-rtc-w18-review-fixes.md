# Phase 7 RTC W18 Review Fixes

Date: 2026-07-04

## Scope

This note records the first three follow-ups from the Claude RTC facade migration
review.

The scope is SDK-private behavior only. The public RTC facade remains:

- `media/rtc_facade/include/ai_rtc_facade.h`

## Task 1: FAILED Retry Connection Cleanup

Finding:

- If Agora reports failure and the product retries with a new token without
  calling Stop first, the facade can enter `FAILED` while the Agora service
  still owns a live connection.
- A direct retry used to clear `s_agora.conn_id` before creating the next
  connection, which could orphan the old connection.

Correction after lifecycle review:

- Do not force `leave_channel()` / `destroy_connection()` from
  `Ai_Rtc_Agora_Service_Start()` while a resident connection may still be in a
  channel.
- Fast open/close and vendor SDK leave timing can make destructive cleanup from
  Start unsafe.
- If the facade is already `JOINED`, `Ai_Rtc_Facade_On_Token_Result()` renews
  the token and does not recreate the connection.
- FAILED retry cleanup remains a W19 lifecycle design item. It needs an
  explicit session phase model or a vendor/port guarantee before SDK code can
  safely destroy resident connections during retry.

Current host coverage:

- `test_agora_backend_flow` verifies that token result while joined calls
  `renew_token` and does not create a new connection.

## Task 2: Datastream Parser Boundary

Decision:

- The public facade continues to expose raw datastream send/RX only.
- The Agora service callback `on_stream_message` must relay raw
  `Ai_Rtc_Facade_Datastream_Message_t` through the facade.
- Product event semantics such as `message.state` and `message.user` belong in
  product compatibility code.

Clarification:

- `media/rtc_facade/src/agora/ai_rtc_agora_datastream.c` remains a reusable SDK
  private parser.
- ESP32S3 product compatibility code calls that parser from
  `ai_components/network_transfer/rtc_facade_compat.c`.
- Beken currently accepts raw datastream logging. If Beken needs product state
  or ASR activity events later, add a Beken product bridge that calls the SDK
  private parser.

Guard:

- `test/host/check_ai_rtc_facade_sdk_agora_migration.py` checks that SDK service
  raw-relays datastream and that ESP32S3 compat still owns product event
  parsing.

## Task 3: Stale Callback / Connection ID Reuse

Finding:

- Existing stale callback rejection checked only `session_active` and
  `conn_id`.
- `generation` existed but could not be used to validate callbacks because the
  Agora C callback API does not pass a user pointer or generation token.
- If the vendor SDK immediately reused a just-destroyed `connection_id`, a late
  callback from the old session could be indistinguishable from a callback for
  the new session.

Remaining contract:

- No destructive conn-id reuse mitigation is shipped in W18.
- A complete solution requires vendor callback context, a SDK callback
  dispatcher with per-session callback ownership, or a documented vendor/port
  guarantee that no callbacks for a destroyed connection can arrive after
  `agora_rtc_destroy_connection()` returns.
- Until that contract exists, do not destroy a resident in-channel connection
  from Start as a retry shortcut.

## Task 4: Low-Risk Facade/Service Hardening

Implemented scope:

- Send path snapshot:
  - `Ai_Rtc_Agora_Service_Send_Audio()`
  - `Ai_Rtc_Agora_Service_Send_Video()`
  - `Ai_Rtc_Agora_Service_Send_Datastream()`
- Datastream readiness is aligned with active session, joined channel, and
  control stream readiness.
- Audio jitter buffer enablement follows `config->enable_audio`.
- Facade callbacks copy callback function and user pointer into locals before
  invoking product code.
- Duplicate joined-state assignments were removed.
- `stale_callbacks` is reset at session reset/start so diagnostics describe the
  current session instead of accumulating across unrelated turns.

Public API impact:

- No change to `media/rtc_facade/include/ai_rtc_facade.h`.
- No RTM/datastream selector was added to the public facade.
- No product DMA, heap, LCD, key, codec, or audio-policy API was added.

## W18 Runtime Acceptance

Recorded: 2026-07-05

| Platform | Runtime result | Evidence |
| --- | --- | --- |
| ESP32S3 AI alarm | Accepted | Runtime log showed SDK facade markers, successful token-to-join, remote AI user joined, first audio TX/RX, datastream activity, stop to `RTC_STOPPED`, and no panic/assert/backtrace. |
| xiaozhi-esp32 M5Stack CoreS3 | Accepted | Runtime log showed SDK facade path, repeated create/join/remote-user/leave/destroy cycles, expected pre-remote-user audio gate, stale callback guard during fast stop, and no panic/assert/backtrace. |
| Beken BK7258 | Accepted | Runtime log showed `create_conn`/`destroy_conn` paired for connections 1..8, `callback joined`, `callback remote_user_joined`, `RTC_FACADE_TX`, `RTC_FACADE_RX`, `RTC_FACADE_DS`, stop back to IDLE, and no panic/assert/backtrace. |

Non-blocking product-layer observations:

- Beken logs contain repeated player/audio pipeline idempotency warnings such as
  `play_pipeline_stop fail` and `Element already stopped`; RTC TX/RX/datastream
  continued after those lines, so they are not W18 SDK RTC blockers.
- xiaozhi logs contain audio bridge/AFE idempotency warnings; RTC lifecycle and
  audio/datastream path remained valid.
- ESP32S3 logs previously recorded a product/UI recommendation to ignore or
  display busy for AI button events during STARTING/JOINING; this is product
  policy and not a public RTC facade API change.

## W18 Host Verification

Required before SDK baseline commit:

- `make -C test/host test`
- `python3 test/host/check_ai_rtc_facade_sdk_agora_migration.py`
- `python3 test/host/check_ai_rtc_facade_public_header.py`
- `git diff --check`
- `make -C test/host clean`
