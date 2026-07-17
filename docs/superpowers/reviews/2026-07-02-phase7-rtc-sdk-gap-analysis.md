# Phase 7 RTC SDK Gap Analysis and Objective Corrections

Date: 2026-07-02

Scope: SDK RTC facade migration after moving the ESP32S3 Agora RTC integration from product `network_transfer/agora_rtc` into SDK `media/rtc_facade` and `chip_esp32s3/rtc`.

## Current SDK State

Confirmed:

- Public RTC integration surface remains `media/rtc_facade/include/ai_rtc_facade.h`.
- SDK owns the facade core lifecycle in `media/rtc_facade/src/ai_rtc_facade.c`.
- SDK owns the Agora backend wrapper in `media/rtc_facade/src/ai_rtc_facade_agora_backend.c`.
- SDK owns the Agora service implementation in `media/rtc_facade/src/agora/ai_rtc_agora_service.c`.
- SDK owns the private datastream envelope parser in `media/rtc_facade/src/agora/ai_rtc_agora_datastream.c`.
- SDK owns an ESP32S3 private port under `chip_esp32s3/rtc/`, including the AOSL PSRAM allocation wrap.
- Host coverage exists for facade core, Agora backend lifecycle, datastream parsing, stale callback rejection, async join wait, reconnect/rejoin events, token renew, and repeat stop/fini behavior.

Not yet confirmed:

- Current SDK RTC changes are not yet committed as a clean SDK baseline.
- Product SDK submodule/copy sync has not been re-validated across all consumers.
- ESP32S3 runtime is not stable after JOIN when mic uplink starts.
- Beken has not been re-built and runtime-checked after the latest SDK RTC service changes.
- xiaozhi-esp32 M5Stack CoreS3 has not been re-synced to the current SDK RTC facade and still appears to list its local `ai_components/agora_rtc` component.

## Runtime Evidence

Source log: `/tmp/log.txt`.

Observed sequence:

```text
sid=1:
  token request started
  MQTT ping timeout/reconnect happened before token callback
  token callback arrived with dma_largest=5632
  RTC start heap gate required dma_largest >= 6144
  RTC start skipped

sid=2:
  token callback arrived with dma_largest=6400
  RTC engine start entered SDK facade
  JOIN succeeded
  APP_EVT_AGENT_JOINED arrived
  Controller opened mic uplink
  AUDIO_UPLINK_DIAG START logged
  device panic: Core 0 double exception, corrupted backtrace
```

Heap calculation:

```text
Before_Rtc_Engine_Start_Call:
  internal_largest=6400
  dma_largest=6400

After_Rtc_Join_Wait_Ok:
  internal_largest=416
  dma_largest=416

largest block loss:
  6400 - 416 = 5984 bytes

20 ms mono PCM16 frame:
  16000 Hz * 1 channel * 2 bytes * 20 / 1000 = 640 bytes
```

Objective conclusion:

- The AI service, token path, and Agora JOIN path worked in `sid=2`.
- The failure happened locally after JOIN when product opened mic uplink.
- `JOINED` is not a sufficient condition for `SEND_AUDIO` on constrained targets.
- The SDK migration must preserve the legacy Agora lifecycle and audio-send readiness semantics instead of widening the public RTC API.

## Corrected State Model

Current implicit model:

```text
IDLE
  -> TOKEN_READY
  -> STARTING
  -> JOINING
  -> JOINED
  -> SEND_AUDIO allowed
```

Corrected model:

```text
IDLE
  -> TOKEN_READY
  -> STARTING
  -> JOINING
  -> JOINED
  -> REMOTE_USER_JOINED
  -> SEND_AUDIO allowed
```

`JOINED` means the RTC channel is connected. `REMOTE_USER_JOINED` means the AI peer is present and local audio can be sent without entering the vendor SDK too early. Platform memory diagnostics remain useful evidence, but they should not become public facade API.

## Gaps

### Gap 1: Agora SDK Stop/Fini Lifecycle Diverged From Legacy Practice

Problem:

- Legacy ESP32S3 integration documented that `agora_rtc_init()` and `agora_rtc_fini()` are heavyweight.
- Legacy practice keeps Agora SDK initialized across repeated AI dialogs.
- The SDK migration initially made per-dialog `Stop()` call whole-SDK shutdown.
- That reintroduced the repeated init/fini pattern the legacy integration had already avoided.

Required correction:

- Keep `agora_rtc_init()` resident across repeated same-app-id AI dialog cycles.
- Make per-dialog `Stop()` release only connection-level resources through leave/destroy connection.
- Keep `agora_rtc_fini()` for explicit shutdown/deinit and app id changes.
- Add host/static tests so this lifecycle does not regress.

### Gap 2: `JOINED` Was Treated as Audio Send Readiness

Problem:

- Product audio path uses `Audio_Rtc_Link_Is_Connected()` as its only runtime send gate.
- That query ultimately maps to RTC connected/joined state.
- Legacy Agora integration gated audio TX until the remote AI user joined.
- The SDK migration initially allowed `Send_Audio()` as soon as the channel joined.

Required correction:

- Gate audio TX inside the SDK Agora service on `session_active && joined && remote_user_joined`.
- Reset remote-user state on stop, reconnecting, connection lost, rejoin, and user offline.
- Keep the product-facing facade simple; product code should not need heap, DMA, or vendor connection details.

### Gap 3: Public API Must Stay Simple

Problem:

- The first W9 proposal would have added private/public-facing resource concepts around audio send safety.
- Linux and higher-resource ports do not need ESP32S3-specific DMA concepts.
- Beken should not inherit ESP32S3 memory policy.

Required correction:

- Keep the public RTC surface at `ai_rtc_facade.h`.
- Prevent public header drift toward heap, DMA, or platform-specific readiness APIs.
- Put lifecycle and remote-user readiness rules in the SDK Agora backend.

### Gap 4: ESP32S3 Start Gate Evidence Still Matters

Problem:

- Current product start gate allows start when `dma_largest >= 6144`.
- Runtime evidence shows `dma_largest=6400` can join successfully but leave only `416` after join.
- That leaves little headroom when local audio starts.

Required correction:

- Keep the numbers as diagnostics for ESP32S3 runtime validation.
- Do not promote ESP32S3 heap thresholds into the shared RTC facade API.
- First restore lifecycle parity and remote-user send gating, then re-test runtime memory behavior.

### Gap 5: Stop/Fini Runtime Recovery Is Still Open

Problem:

- Host tests now require per-dialog stop to keep Agora SDK resident.
- The runtime goal was "after second stop, internal_largest should not remain stuck at 1600".
- Current `/tmp/log.txt` panic happened before a clean stop-after-audio cycle, so this specific runtime recovery target remains unproven.

Required correction:

- Keep the resident-SDK stop strategy.
- Re-test repeat sessions after remote-user audio gate prevents sending before the peer is present.
- Acceptance should include `After_Rtc_Engine_Stop_Call` internal/dma largest recovery.

### Gap 6: Beken Revalidation Is Open

Problem:

- Beken is a special chip/platform and should not inherit ESP32S3 heap policy.
- W8b plan lists BK build/runtime checker as pending.
- Current SDK private port is ESP32S3-oriented for AOSL PSRAM wrap and ESP network refresh; BK coverage is currently host/static plus previous runtime evidence, not latest SDK RTC changes.

Required correction:

- Re-run BK build after SDK RTC changes.
- Re-run accepted BK RTC facade runtime checker.
- If Beken needs allocator diagnostics later, keep them in BK-specific port code rather than public facade API.

### Gap 7: xiaozhi M5Stack CoreS3 Revalidation Is Open

Problem:

- xiaozhi is an ESP32S3 consumer but has a different application, UI/audio stack, and CMake graph.
- It currently appears to include local `ai_components/agora_rtc` and `ai_components/agora_iot_sdk` in the top-level component list.
- Its `sdk/` submodule/copy does not appear to be at the current RTC facade migration state from this workspace inspection.

Required correction:

- Sync xiaozhi SDK to the same RTC facade baseline as alarm.
- Decide explicitly whether xiaozhi continues using local `agora_rtc` temporarily or rewires to SDK facade.
- If rewired, verify M5Stack CoreS3 build graph under `MINIMAL_BUILD` and runtime AI conversation.

### Gap 8: Public API Lacks a Mute/Uplink Control Abstraction

Problem:

- Product still uses compatibility glue such as `Agora_Rtc_Mute_Local_Audio`.
- Public facade has send APIs but no explicit local audio mute/uplink policy API.

Required correction:

- Decide whether local mute/uplink control belongs in public `ai_rtc_facade.h` or remains product/audio policy.
- Until decided, keep compatibility glue clearly marked as transitional.

## Cross-Product Validation Matrix

| Target | Current Status | Required Validation |
| --- | --- | --- |
| SDK host | Passed in previous run after W8b Task 6/7 changes | Re-run after lifecycle and remote-user send gate changes |
| esp32s3_ai_alarm | Builds locally per user; runtime JOIN succeeds then panic after audio send begins | Rebuild by user, verify AI conversation and repeat stop recovery |
| Beken `beken_genie_rino` | Previous RTC facade runtime accepted; latest SDK RTC changes not revalidated | Build and run RTC facade runtime checker with datastream requirement |
| xiaozhi M5Stack CoreS3 | SDK sync/RTC rewire not verified; local `agora_rtc` still appears active | Sync same SDK SHA, verify build graph, then runtime AI/AEC conversation |

## Objective Corrections to W8b Status

- W8b should be treated as "SDK Agora backend migration implemented with host/static coverage" rather than "cross-product runtime complete".
- ESP32S3 runtime has advanced from "cannot start/join" to "joins successfully, then panics after mic uplink starts".
- The next SDK work item should not be described as product-only heap tuning. It should be SDK Agora lifecycle parity and send-readiness hardening based on the legacy integration.
- Beken and xiaozhi validation must remain open acceptance items until they are built and runtime-checked on the current SDK RTC baseline.

## Recommended Next Work Item

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
