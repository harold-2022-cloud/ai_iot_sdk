# RTC Porting Contract

> Status: SDK contract for product and chip ports
> Baseline: Phase 7 RTC facade, `ai_rtc_facade.h`
> Updated: 2026-07-08

## 1. Purpose

The RTC SDK provides a portable real-time communication boundary for RTC
audio, RTC video, datastream, session lifecycle, and backend vendor
integration. Private backend code may support both Agora datastream and RTM
control transports, but the public facade remains transport-neutral.

The RTC SDK does **not** own board peripherals. Microphone, speaker, codec,
DMA, LCD, AVI playback, buttons, BLE/Wi-Fi provisioning, HTTP/MQTT product
flows, PA GPIO, audio engine startup order, and product resource policy remain
product/platform responsibilities.

For solution providers, this is the most important split:

- SDK-owned: RTC audio/video transport, datastream transport, private RTM
  backend option, Agora vendor headers/libraries, and RTC lifecycle state.
- Product-owned: microphone capture, speaker playback, codec/AEC setup, DMA
  sizing, LCD/AVI/camera workload, buttons, BLE/Wi-Fi provisioning, HTTP/MQTT
  business flow, NFC, 4G/modem/PPP bring-up, GPIO mapping, power policy, and UI
  behavior.

The public API boundary is:

```text
media/rtc_facade/include/ai_rtc_facade.h
```

Products should consume RTC through `Ai_Rtc_Facade_*` only.

## 2. Layer Ownership

| Layer | Owns | Must not own |
|---|---|---|
| Product / board | mic, speaker, codec, AEC, DMA, LCD, AVI/JPEG/display policy, buttons, BLE/Wi-Fi provisioning, HTTP/MQTT product flows, PA GPIO, boot order, local audio playback/capture | Agora vendor lifecycle, RTC backend internals |
| Chip/platform port | OS primitives, time/sleep, memory wrappers if needed, chip-specific vendor component wiring | product UI policy, product audio UX |
| RTC facade public API | token-to-join lifecycle, join/stop state, audio/video/datastream TX/RX API, public callbacks | DMA thresholds, heap policy, LCD/AVI/button/codec/GPIO/BLE/Wi-Fi/HTTP/MQTT APIs, RTM/datastream transport selector |
| RTC backend private layer | Agora service, Agora channel options, backend media mapping, private datastream/RTM control transport, reusable datastream envelope parser support | product peripheral initialization, product event policy |
| Vendor SDK | transport implementation | product-visible API shape |

## 3. Public Facade Contract

The public facade may expose only portable RTC concepts:

- init/deinit
- token result handling
- stop
- state query and joined wait
- audio frame TX
- audio frame RX callback
- video frame TX
- video frame RX callback
- datastream TX
- datastream RX callback
- RTC state events

The public facade must not expose:

- DMA buffer counts or thresholds
- heap/resource policy
- LCD/AVI/button/codec/PA GPIO controls
- BLE/Wi-Fi provisioning controls
- HTTP/MQTT product business controls
- RTM/datastream transport selector controls
- chip-specific GPIO numbers
- chip-specific task/core affinity policy
- product audio engine startup order
- Agora vendor types or headers
- server-specific product business fields beyond RTC token/channel/app-id/uid

If a product needs one of those items, it belongs in the product board layer,
chip platform layer, or SDK private backend, not in `ai_rtc_facade.h`.

## 4. Product Responsibilities

Each product must provide a working local media path before relying on RTC:

1. Initialize microphone capture.
2. Initialize speaker playback.
3. Configure codec/sample rate/frame duration.
4. Configure AEC or decide that AEC is disabled.
5. Configure DMA and memory budgets for that board.
6. Start local audio services in the product-required order.
7. Feed captured audio frames to `Ai_Rtc_Facade_Send_Audio()`.
8. Play `on_audio_rx` frames through the product speaker path.
9. Feed encoded video frames to `Ai_Rtc_Facade_Send_Video()` if video is enabled.
10. Render or forward `on_video_rx` frames if video RX is enabled.
11. Parse or act on product-level datastream events delivered by
    `on_datastream_rx`.
12. Gate visual workload such as LCD refresh, AVI playback, JPEG decode, and
    DMA2D work so it does not starve product audio capture/playback or RTC TX/RX
    hot paths.

The product is responsible for frame format compatibility:

- `Ai_Rtc_Facade_Audio_Frame_t.format`
- `sample_rate_hz`
- `channels`
- `duration_ms`
- `Ai_Rtc_Facade_Video_Frame_t.format`
- `width`
- `height`
- `frame_rate_hz`
- keyframe flag

The SDK copies required configuration values at `Ai_Rtc_Facade_Init()` time,
but TX frame buffers remain caller-owned and only need to stay valid for the
call duration. RX callback buffers are valid only during the callback.

## 5. RTC SDK Responsibilities

The SDK is responsible for:

- Keeping `ai_rtc_facade.h` portable.
- Hiding Agora vendor headers and libraries behind SDK private layers.
- Preserving backend lifecycle parity with working legacy integrations.
- Mapping facade audio/video/datastream calls to backend APIs.
- Keeping datastream/RTM backend selection private to SDK backend code.
- Dispatching RTC state events and RX callbacks.
- Keeping Stop session-scoped and Deinit/Shutdown explicit.
- Keeping reconnect/rejoin state behavior deterministic.
- Providing private reusable parsing support for the shared Agora datastream
  envelope.
- Providing host/static tests that catch boundary regressions.

The SDK may provide chip-specific private ports, for example:

```text
chip_esp32s3/vendor/agora_iot_sdk/
media/rtc_facade/src/agora/ai_rtc_agora_port.h
```

Those ports must not leak vendor headers or product peripheral policy through
the public facade.

## 6. Datastream Boundary

The public facade datastream contract is raw by design:

```text
Ai_Rtc_Facade_Send_Datastream()
Ai_Rtc_Facade_Callbacks_t.on_datastream_rx
Ai_Rtc_Facade_Datastream_Message_t
```

`on_datastream_rx` delivers raw backend messages. The SDK Agora service must
not parse product events inside `on_stream_message`; it relays the raw message
through the facade so products can choose their own threading, queueing, and
business semantics.

The SDK may keep a private reusable parser for the common Agora AI datastream
envelope:

```text
media/rtc_facade/src/agora/ai_rtc_agora_datastream.c
```

Products that need `message.state` or `message.user` semantics must call that
parser from their compatibility/product layer, then dispatch product events from
product-owned code. For example, ESP32S3 keeps this bridge in
`ai_components/network_transfer/rtc_facade_compat.c`.

Beken currently accepts raw datastream logging in
`projects/beken_genie_rino/ap/app_rtc_facade_bridge.c`. If Beken needs product
state/user events later, add a Beken product bridge that calls the SDK private
parser; do not move product event policy into `ai_rtc_facade.h` or parse events
inside the Agora service callback.

## 7. Media Gating Contract

Audio TX:

- The product may call `Ai_Rtc_Facade_Send_Audio()` after local capture starts.
- The SDK backend may reject audio until RTC session is active, channel is
  joined, and the remote AI user has joined.
- After reconnect/rejoin, audio TX must not assume remote user presence unless
  the backend state machine says the remote user is joined.

Video TX:

- Video TX is gated by RTC session active and channel joined.
- Video TX does not require the AI remote-user audio gate.
- Product runtime video can remain disabled even when SDK video backend support
  exists.

Datastream TX:

- Datastream requires a joined RTC session and backend stream readiness.
- Product event semantics remain product-owned; SDK private code may parse only
  the shared envelope format.

## 8. Lifecycle And Product Button Gate

Agora RTC channel join is asynchronous. A successful `join_channel()` return
means the join request was accepted; it does not mean the channel is joined.
The SDK reports joined state only after the backend join-success callback.

Products must not issue high-frequency AI open/close requests while RTC is in a
transient lifecycle state. The product/UI layer should gate the AI button:

- allow start only from `AI_RTC_FACADE_STATE_IDLE` or
  `AI_RTC_FACADE_STATE_FAILED`,
- do not call stop while state is `AI_RTC_FACADE_STATE_STARTING` or
  `AI_RTC_FACADE_STATE_JOINING`,
- do not call start while state is `AI_RTC_FACADE_STATE_STOPPING`,
- treat `AI_RTC_FACADE_ERR_BUSY` as "request ignored because RTC lifecycle is
  still in flight", not as a server or AI failure.

The SDK must reject destructive lifecycle re-entry during in-flight join. It
must not call `leave_channel()` or `destroy_connection()` merely because a
product issued a rapid stop while the join-success callback is still pending.

Lifecycle state meanings:

| Public state | Meaning | Product guidance |
|---|---|---|
| `AI_RTC_FACADE_STATE_IDLE` | No active RTC session. Backend connection resources should be released or reusable in a known idle state. | Start is allowed. Stop is idempotent and should be treated as success. |
| `AI_RTC_FACADE_STATE_TOKEN_READY` | A valid token result was accepted and copied by the facade; backend start is about to run. | Do not issue another start. UI may show busy/starting. |
| `AI_RTC_FACADE_STATE_STARTING` | SDK is starting backend service and connection setup. Agora join has not necessarily been requested yet. | Treat start/stop as busy unless product has an explicit cancel flow above SDK. |
| `AI_RTC_FACADE_STATE_JOINING` | Backend join request is in flight. Agora API return only means request accepted; success requires callback. | Do not destroy connection. Ignore repeated AI button requests or show busy. |
| `AI_RTC_FACADE_STATE_JOINED` | Join-success callback was received. Media/data TX may still have backend-specific gates such as remote AI user joined or datastream readiness. | Audio TX should be sent through facade; product should still tolerate `ERR_NOT_READY`. |
| `AI_RTC_FACADE_STATE_RECONNECTING` | Backend reported network reconnect. Previous remote-user and TX readiness assumptions are invalid until fresh callbacks arrive. | Keep UI in active/reconnecting state and avoid assuming audio uplink is accepted. |
| `AI_RTC_FACADE_STATE_STOPPING` | Leave/stop is in progress. This is the public facade name for the LEAVING phase. | Do not start a new dialog until state returns to IDLE or FAILED. |
| `AI_RTC_FACADE_STATE_FAILED` | Token, backend start, join, renew, or runtime callback reported failure. | Product may return UI to idle/error and allow a new start after cleanup. |

## 9. Callback Contract

Callbacks may run from backend/service context. Product callbacks must be short
and non-blocking.

RTC callbacks are state-machine inputs. The SDK uses Agora `connection_id_t` as
the connection lifecycle key. Callback `uid` values identify the remote user or
media/datastream sender where the Agora callback provides one; `uid` is product
event identity, not the local connection lifecycle key.

Do not mix these identities:

- `connection_id_t` is the lifecycle key for deciding whether a callback belongs
  to the current SDK connection.
- `uid` is the remote user or media/datastream sender identity carried by a
  callback.
- A stale callback with an old `connection_id_t` must not update current
  lifecycle state even if its `uid` looks valid.

Product callbacks should:

- copy RX data if it is needed after the callback returns,
- enqueue work to product tasks when playback/rendering/parsing is expensive,
- avoid blocking network, UI, storage, or long audio operations inside the RTC
  callback,
- avoid calling product stop/deinit paths recursively from deep callback stacks.

Product callbacks must not:

- retain RX frame pointers after return,
- free SDK-owned RX buffers,
- call Agora vendor APIs directly,
- assume a fixed callback thread/core across chips.

## 10. Diagnostic Marker Contract

Ports should keep logs consistent enough that a product integrator can classify
a failure without reading backend code. Existing marker prefixes are acceptable;
do not add high-frequency hot-path logs unless they are throttled.

| Event | Required diagnostic content | Current marker family |
|---|---|---|
| token result accepted or rejected | result code, channel/app-id presence, UID/user-account mode | `AI_RTC_FACADE_CORE` state transition around `TOKEN_READY`; product bridge may add token-result request logs |
| join start | connection id, UID mode, audio/video enable flags, control peer presence | `AI_RTC_AGORA` `join conn=...` |
| joined | connection id and callback result | `AI_RTC_AGORA` joined callback plus `AI_RTC_FACADE_EVENT_JOINED` |
| remote user joined | connection id and remote `uid` | `AI_RTC_AGORA` `callback remote_user_joined` plus `AI_RTC_FACADE_EVENT_REMOTE_USER_JOINED` |
| audio/video TX not ready | active/joined/remote-user/control readiness, connection id, generation | `AI_RTC_AGORA` `send_audio not_ready`, `send_datastream not_ready`; hot-path logs must be throttled |
| audio/video/datastream RX | callback event, sender `uid` where available, frame/message length | facade backend notify callbacks or product RX diagnostics |
| datastream ready | backend control transport readiness: stream id for datastream, RTM login state for RTM | SDK private control transport state; product-visible behavior remains raw datastream facade readiness |
| stop reason | previous state, backend stop return code, final state | `AI_RTC_FACADE_CORE` event/state log and backend stop logs |

Diagnostic markers must not become public API. They are operational markers for
porting and runtime analysis only.

## 11. Chip Porting Checklist

For a new chip or board, confirm these before runtime validation:

1. `ai_rtc_facade.h` builds without product peripheral includes.
2. Product code does not include `agora_rtc_api.h` directly.
3. Product CMake links SDK RTC facade and chip vendor component through SDK
   paths.
4. Microphone capture produces the expected audio format and frame duration.
5. Speaker playback accepts `on_audio_rx` format or product converts it.
6. AEC mode matches hardware:
   - hardware AEC has a real speaker reference path,
   - software AEC has a reference ring buffer or equivalent software feed.
7. DMA and memory budgets are handled in product/platform startup code.
8. Product UI/LCD/key startup order is validated independently of RTC facade.
9. One dialog can start, join, send audio, receive audio, and stop.
10. A second dialog can start after stop without product resource leakage.
11. Optional reconnect/rejoin validation is run if the test environment can
    trigger network transitions.
12. Optional token renew validation is run only when token lifetime can be
    controlled server-side.

## 12. Platform Examples

ESP32S3 AI alarm:

- Product owns I2S, DMA, UI/LCD, Bluetooth memory policy, and audio engine
  startup order.
- SDK owns Agora facade backend and ESP32S3 vendor archives.
- Product consumes `Ai_Rtc_Facade_*` through its compatibility adapter.

Beken BK7258:

- Product owns `audio_engine`, Voice Service, AEC V3 mode, ADC channels,
  speaker PA/gain, LCD, keys, and board-specific external peripherals.
- SDK facade remains the RTC boundary.
- Beken product runtime video may remain disabled while compatibility video
  mapping calls `Ai_Rtc_Facade_Send_Video()`.
- Beken BK7258 R1 visual workload note: AVI/JPEG decode, DMA2D blit/wait, LCD
  SPI flush, and display refresh run in product-owned code and can compete with
  the audio/RTC hot path. Product should define an AI visual workload policy:
  full AVI when idle if needed, reduced frame rate/static animation/frame skip
  while AI dialog is joined, and no SDK public API knobs for LCD/AVI/DMA2D
  tuning.

xiaozhi-esp32 M5Stack CoreS3:

- Product owns M5Stack CoreS3 audio bridge and board codec timing.
- Product legacy RTC API shape can remain as a compatibility adapter.
- Adapter must call `Ai_Rtc_Facade_*` and must not include Agora vendor headers.

## 13. Boundary Guard Expectations

Host/static guards should reject regressions where:

- `ai_rtc_facade.h` gains DMA, heap, LCD, AVI, button, codec, GPIO, BLE/Wi-Fi,
  HTTP/MQTT, RTM/datastream transport selector, or product resource policy APIs,
- product active source includes Agora vendor headers directly,
- product active CMake links product-owned Agora vendor archives instead of SDK
  vendor paths,
- `Ai_Rtc_Agora_Service_Stop()` performs per-dialog full SDK shutdown,
- resident/repeat session behavior bypasses required backend state gates,
- video facade API returns hard-coded unsupported while SDK video backend is
  expected for that baseline.

## 14. Acceptance Criteria For A Port

A port is accepted when:

- SDK host/static verification passes.
- Product build passes with only SDK-owned RTC vendor dependencies.
- Runtime reaches token ready, starting, joining, joined, remote user joined,
  media TX/RX, datastream if enabled, and stop.
- At least two consecutive dialogs work on the same boot.
- Logs show no assert, panic, allocator corruption, watchdog, hard fault, or RTC
  failed state.
- Product-specific peripherals continue to work after RTC start/stop.

Failures should be classified by layer before changing code:

- RTC facade/API mismatch
- backend vendor lifecycle/state issue
- product audio capture/playback issue
- AEC/reference mismatch
- product DMA/memory/resource issue
- UI/LCD/key startup issue
- server/token/service issue
