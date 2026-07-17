# Phase 7 RTC Final Closure Note

> Date: 2026-07-03
> Scope: SDK RTC facade migration status across canonical SDK, ESP32S3 AI alarm, Beken BK7258, and xiaozhi-esp32 M5Stack CoreS3.

## 1. Closure Decision

Phase 7 RTC SDK migration is accepted for the current release baseline.

The accepted public SDK boundary is still:

```text
media/rtc_facade/include/ai_rtc_facade.h
```

Products should call `Ai_Rtc_Facade_*` only. Agora vendor headers, archives,
session lifecycle, media mapping, and datastream parsing stay in SDK-private
or product-compatibility layers.

## 2. Current Repo Baselines

| Area | Path | Accepted head / baseline | Status |
|---|---|---:|---|
| Canonical SDK | `/root/smp/ai_iot_sdk` | `77ccff23` | W14 SDK video facade + Beken runtime note accepted |
| ESP32S3 AI alarm | `/mnt/c/Users/harold.chen/esp32s3_ai_alarm` | `1d21dfcb` feature baseline; later docs/cleanup at `ebaf8f30` | W11 runtime accepted |
| xiaozhi-esp32 | `/mnt/c/Users/harold.chen/xiaozhi-esp32` | `d9b7b8aa` | W13 runtime accepted |
| xiaozhi SDK submodule | `/mnt/c/Users/harold.chen/xiaozhi-esp32/sdk` | `99df3fb2` | Consumes W11/vendor SDK baseline, not W14 video baseline |
| Beken product | `/root/smp/bk_solution_ai/projects/beken_genie_rino` | not a git repo in this workspace | W14 build/runtime accepted; product diff recorded, not committed |

## 3. SDK State

The canonical SDK now provides:

- Public RTC facade API for audio, video, and datastream through
  `ai_rtc_facade.h`.
- SDK-private Agora service/backend under `media/rtc_facade/src/agora`.
- SDK-owned ESP32S3 Agora vendor header/libs under
  `chip_esp32s3/vendor/agora_iot_sdk`.
- Host/static guards for public header shape, backend boundary, Agora migration
  boundary, allocator checks, facade core, Agora backend, and datastream.
- Video TX/RX backend mapping in W14:
  - `Ai_Rtc_Facade_Send_Video()` routes through backend.
  - Agora service maps facade video frames to `agora_rtc_send_video_data`.
  - Agora `on_video_data` is bridged to facade video RX callback.

Current media gating model:

- Audio TX is gated by session active, channel joined, and remote AI user joined.
- Rejoin clears the remote-user audio gate; audio must wait for the remote user
  joined event again.
- Video TX is gated by session active and channel joined, matching Agora RTSA
  video examples.
- Datastream remains a facade-owned API with SDK-private envelope parsing.

## 4. ESP32S3 AI Alarm State

ESP32S3 product migration was accepted through W11.

Evidence is recorded in:

```text
/mnt/c/Users/harold.chen/esp32s3_ai_alarm/docs/superpowers/reviews/2026-07-02-phase7-rtc-w9-w10-gap-record.md
```

Accepted facts:

- Product was rewired to SDK facade in
  `1d21dfcb feat(rtc): rewire ESP32S3 product to SDK facade`.
- Product-owned active Agora vendor component was removed from the runtime
  boundary in W11.
- Runtime logs accepted repeated AI conversations after W11 and post-commit
  checks.
- Accepted logs showed no `Guru Meditation`, assert, TLSF heap corruption,
  backtrace, or RTC heap start block.
- Product resource/DMA policy was not added to public `ai_rtc_facade.h`.

## 5. Beken BK7258 State

Beken remains a cross-chip consumer of the SDK facade. It must not consume the
ESP32S3 Agora vendor component.

W14 Beken evidence is recorded in:

```text
/root/smp/ai_iot_sdk/docs/superpowers/plans/2026-07-03-phase7-rtc-w14-video-facade-backend.md
/root/smp/bk_solution_ai/projects/beken_genie_rino/docs/superpowers/progress.md
```

Accepted facts:

- Beken build passed with:

```text
make -C /root/smp/bk_solution_ai/projects/beken_genie_rino bk7258 SDK_DIR=/root/smp/bk_avdk_smp
```

- Firmware package was produced:

```text
/root/smp/bk_solution_ai/projects/beken_genie_rino/build/bk7258/beken_genie_rino/package/all-app.bin
```

- Runtime log `/tmp/log.txt` was accepted by user observation as normal voice
  conversation.
- Runtime showed RTC token, join, remote user joined, TX, RX, datastream, stop,
  and no fatal RTC regression markers.
- Compatibility video bridge now maps `frame_buffer_t` to
  `Ai_Rtc_Facade_Video_Frame_t` and calls `Ai_Rtc_Facade_Send_Video()`.
- Product runtime video remains disabled:

```text
/root/smp/bk_solution_ai/projects/beken_genie_rino/ap/app_rtc_facade_bridge.c
.enable_video = false
.on_video_rx = NULL
```

Caveat:

- `/root/smp/bk_solution_ai` is not a git repository in this workspace, so the
  Beken product-side W14 compatibility diff is intentionally uncommitted.

## 6. xiaozhi-esp32 State

xiaozhi migrated from product-owned direct Agora RTC integration to SDK facade
through W13.

Evidence is recorded in:

```text
/mnt/c/Users/harold.chen/esp32s3_ai_alarm/docs/superpowers/reviews/2026-07-02-phase7-rtc-w9-w10-gap-record.md
/mnt/c/Users/harold.chen/esp32s3_ai_alarm/docs/superpowers/plans/2026-07-03-phase7-rtc-w13-xiaozhi-rtc-facade-migration.md
```

Accepted facts:

- xiaozhi product head is `d9b7b8aa`.
- Product-owned `ai_components/agora_iot_sdk` was removed.
- Product `ai_components/agora_rtc/rtc_facade_compat.c` bridges the existing
  xiaozhi legacy RTC API shape to `Ai_Rtc_Facade_*`.
- M5Stack CoreS3 audio bridge and board codec timing remained product-owned and
  were not rewritten.
- Static guard passed:

```text
python3 /mnt/c/Users/harold.chen/xiaozhi-esp32/tools/check_rtc_facade_migration.py
```

- User rebuilt xiaozhi successfully and reported normal AI conversation.

Caveat:

- xiaozhi `sdk` submodule is currently pinned at `99df3fb2`, which includes the
  W11 RTC facade/vendor archive baseline. It has not been advanced to canonical
  SDK `77ccff23` W14 video backend in this closure.

## 7. Verification Completed

Canonical SDK host verification passed:

```text
make -C /root/smp/ai_iot_sdk/test/host test
make -C /root/smp/ai_iot_sdk/test/host clean
```

Accepted pass categories:

- entity interface reference template
- RTC public header
- RTC facade core boundary
- SDK Agora migration boundary
- allocator tests
- facade core tests
- Agora backend tests
- Agora datastream tests

Beken build/runtime verification passed as described in section 5.

ESP32S3 and xiaozhi firmware builds/runtimes were user-run in the Windows board
environment and accepted in the tracked W11/W13 records.

## 8. Remaining Non-Blocking Items

These are not blockers for Phase 7 closure:

- Long soak/stress runtime for each board remains optional release confidence
  work.
- Token renew remains optional extended validation because the will-expire path
  requires server-side/token-lifetime control.
- Beken product camera/video runtime is not enabled; W14 only completed SDK
  video backend and Beken compatibility mapping.
- xiaozhi has not consumed W14 video backend yet because its SDK submodule is
  pinned at `99df3fb2`.
- Beken product-side changes are not committed because the target tree is not a
  git repository in this workspace.

## 9. Next Recommended Release Prep

1. If a release label is needed, tag canonical SDK at the accepted baseline after
   this closure note commit.
2. Decide separately whether xiaozhi should advance its SDK submodule from
   `99df3fb2` to `77ccff23` to consume W14 video facade support.
3. Keep future product ports constrained to `ai_rtc_facade.h`; product-specific
   audio bridge, LCD, keys, codec, DMA, and boot timing stay product-owned.
