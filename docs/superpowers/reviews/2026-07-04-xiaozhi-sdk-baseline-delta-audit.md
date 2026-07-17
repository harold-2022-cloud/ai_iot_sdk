# xiaozhi SDK Baseline Delta Audit

> Date: 2026-07-04
> Scope: Audit xiaozhi-esp32 impact before moving its `sdk` submodule from
> `99df3fb2` to the Phase 7 SDK accepted baseline.

## 1. Question

Should `/mnt/c/Users/harold.chen/xiaozhi-esp32/sdk` move from:

```text
99df3fb2 fix(rtc): track SDK Agora vendor archives
```

to:

```text
phase7-rtc-sdk-accepted -> 911940b7 docs(rtc): add phase7 final closure
```

or to current canonical SDK HEAD:

```text
15bc5290 docs(rtc): add porting contract
```

This audit only compares and classifies the delta. It does not modify xiaozhi.

## 2. Current xiaozhi State

Repository:

```text
/mnt/c/Users/harold.chen/xiaozhi-esp32
```

Current product head:

```text
d9b7b8a build(rtc): consume SDK Agora vendor archives
```

Current SDK submodule head:

```text
99df3fb2
```

Current xiaozhi CMake consumes these SDK components:

```text
sdk/ai_session_diag
sdk/entity_iot_sdk
sdk/chip_esp32s3
sdk/chip_esp32s3/vendor/agora_iot_sdk
sdk/media/rtc_facade
sdk/library/mi_mqtt
```

The active RTC adapter is:

```text
ai_components/agora_rtc/rtc_facade_compat.c
```

It initializes:

```text
.enable_audio = true
.enable_video = false
.on_audio_rx = rtc_compat_on_audio_rx
.on_datastream_rx = rtc_compat_on_datastream_rx
```

The xiaozhi product does not currently call `Ai_Rtc_Facade_Send_Video()`.

## 3. Commit Delta: `99df3fb2..phase7-rtc-sdk-accepted`

Commits:

```text
bf1e4060 docs: rename SDK README title
394d7505 fix(rtc): reset remote user gate after rejoin
de494baf docs(rtc): plan video facade backend
a8291d08 feat(rtc): route facade video through backend
5af965ff test(rtc): add host Agora video stubs
27950348 feat(rtc): implement Agora video facade backend
01a90d34 test(rtc): guard SDK video facade backend
33f836fb docs(rtc): record W14 video facade acceptance
77ccff23 docs(rtc): record W14 Beken runtime acceptance
911940b7 docs(rtc): add phase7 final closure
```

Changed active SDK runtime files:

```text
media/rtc_facade/include/ai_rtc_facade.h
media/rtc_facade/src/agora/ai_rtc_agora_service.c
media/rtc_facade/src/agora/ai_rtc_agora_service.h
media/rtc_facade/src/ai_rtc_facade.c
media/rtc_facade/src/ai_rtc_facade_agora_backend.c
media/rtc_facade/src/ai_rtc_facade_backend.h
```

Other changed files are docs, README, and host test/guard files.

## 4. Runtime-Relevant Deltas

### 4.1 Video facade backend

W14 changes `Ai_Rtc_Facade_Send_Video()` from a stable unsupported shell into a
real backend-routed API.

Public header delta:

- `Ai_Rtc_Facade_Video_Frame_t` gains `frame_rate_hz`.
- `Ai_Rtc_Facade_Send_Video()` remains the same function name/signature.
- RX data lifetime comment is clarified.

Facade/backend delta:

- `Ai_Rtc_Facade_Send_Video()` validates init, frame, `enable_video`, and joined
  state, then calls backend `send_video`.
- backend vtable gains `send_video`.
- Agora backend maps facade video to `agora_rtc_send_video_data()`.
- Agora `on_video_data` dispatches `on_video_rx`.

xiaozhi impact:

- Build impact: active SDK sources change.
- API impact: low, because xiaozhi does not currently instantiate
  `Ai_Rtc_Facade_Video_Frame_t` or call `Ai_Rtc_Facade_Send_Video()`.
- Runtime impact: low for current voice-only xiaozhi path because
  `enable_video=false`.

Vendor compatibility:

- xiaozhi current vendor header already contains:
  - `video_frame_info_t`
  - `on_video_data`
  - `on_target_bitrate_changed`
  - `on_key_frame_gen_req`
  - `agora_rtc_send_video_data`
  - `VIDEO_DATA_TYPE_GENERIC_JPEG`
  - `VIDEO_STREAM_HIGH`
  - `VIDEO_ORIENTATION_0`
- xiaozhi current vendor header/libs match canonical SDK vendor header/libs.

Expected build risk from W14 video backend: **low**.

### 4.2 Rejoin remote user gate reset

Commit `394d7505` changes the Agora service reconnect/rejoin behavior:

- `on_reconnecting` clears `remote_user_joined`.
- `on_connection_lost` clears `remote_user_joined`.
- `on_rejoin_channel_success` clears `remote_user_joined`.

This makes audio TX stricter after reconnect/rejoin: audio must wait for a new
remote user joined event before `Ai_Rtc_Facade_Send_Audio()` succeeds.

xiaozhi adapter also sets product `agent_online=false` on:

```text
AI_RTC_FACADE_EVENT_RECONNECTING
AI_RTC_FACADE_EVENT_REMOTE_USER_OFFLINE
AI_RTC_FACADE_EVENT_STOPPED
AI_RTC_FACADE_EVENT_FAILED
```

xiaozhi impact:

- Normal one-dialog voice flow: low risk.
- Reconnect/rejoin flow: medium risk, because success depends on Agora/server
  emitting remote-user-joined again after rejoin.
- This is a behavior change compared with `99df3fb2`.

Recommended validation if upgrading:

- standard voice smoke,
- second dialog after stop,
- optional reconnect/rejoin smoke if the test setup can trigger it.

### 4.3 Docs and host guards

Docs added:

- W14 implementation plan
- W14 acceptance notes
- Phase 7 final closure note

Host-only changes:

- host Agora video stubs
- facade core video tests
- Agora backend video tests
- public header guard update
- SDK migration guard update

xiaozhi impact:

- no firmware runtime impact,
- possible local host-test behavior changes only if xiaozhi runs SDK host tests
  from its submodule.

## 5. Delta To Current SDK HEAD `15bc5290`

Compared with `phase7-rtc-sdk-accepted`, current canonical SDK HEAD adds:

```text
15bc5290 docs(rtc): add porting contract
```

Additional file:

```text
docs/rtc-porting-contract.md
```

This is docs-only. It has no xiaozhi firmware build/runtime impact.

## 6. Risk Classification

| Delta | Build risk | Runtime risk | Reason |
|---|---:|---:|---|
| README/docs/final closure | none | none | docs only |
| host tests/guards/stubs | none for firmware | none for firmware | not active firmware path |
| public video frame adds `frame_rate_hz` | low | low | xiaozhi does not instantiate video frame |
| `Ai_Rtc_Facade_Send_Video()` becomes real | low | low | xiaozhi has `enable_video=false` |
| Agora video callbacks registered | low | low | vendor header/libs already support symbols |
| remote user gate reset after reconnect/rejoin | low | medium in reconnect | stricter audio TX after rejoin |
| porting contract docs at `15bc5290` | none | none | docs only |

## 7. Recommendation

Do not treat this upgrade as a Phase 7 closure blocker.

If the goal is stable release handoff, keeping xiaozhi at `99df3fb2` is
acceptable because W13 build/runtime was already accepted and the closure note
records that baseline explicitly.

If the goal is aligning all consumers to the accepted SDK tag, upgrading xiaozhi
to `phase7-rtc-sdk-accepted` is reasonable. Expected build risk is low because
the active runtime delta is contained to `media/rtc_facade`, and xiaozhi's
vendor SDK already supports the W14 video symbols.

Do not upgrade silently. If upgraded, require:

1. `python3 tools/check_rtc_facade_migration.py`
2. Windows ESP-IDF build in `/mnt/c/Users/harold.chen/xiaozhi-esp32`
3. M5Stack CoreS3 runtime smoke:
   - boot
   - Wi-Fi online
   - token received
   - RTC joined
   - remote AI user joined
   - welcome/downlink playback
   - uplink speech
   - clean stop
   - second dialog start/stop
4. Optional reconnect/rejoin validation if a controlled network test is
   available.

Recommended target:

```text
phase7-rtc-sdk-accepted
```

Use current HEAD `15bc5290` only if xiaozhi also wants the docs-only
`rtc-porting-contract.md` in its submodule.
