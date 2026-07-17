# Phase 7 RTC W16b Final Closure

Date: 2026-07-04

## Scope

W16b closes the SDK-private Agora control transport work.

The goal was to add RTM-capable backend structure without changing the public RTC facade contract or the accepted datastream runtime behavior.

## SDK Baseline

Code baseline:

- Repository: `/root/smp/ai_iot_sdk`
- Commit: `43ba0fd5 feat(rtc): add private Agora control transport`

Closure documentation baseline:

- Repository: `/root/smp/ai_iot_sdk`
- Commit: `7837b870 docs(rtc): record W16b runtime acceptance`

## Public API Decision

Public API remains:

- `media/rtc_facade/include/ai_rtc_facade.h`

Public API does not expose:

- Agora vendor types
- RTM API names
- public transport selector
- chip-specific RTC backend details

Neutral optional token fields are allowed because they are transport-agnostic:

- `user_account`
- `control_peer_id`
- `control_token`

## Runtime Acceptance

Accepted default runtime transport:

- Datastream

RTM status:

- Integrated as SDK-private Agora backend capability.
- Not enabled for ESP32S3 or xiaozhi W16b runtime acceptance.
- Future RTM enablement must be selected below the public facade boundary.

Product acceptance:

- ESP32S3 `esp32s3_ai_alarm`
  - Product commit: `2bc8d334 chore(rtc): sync SDK W16b baseline`
  - SDK gitlink: `43ba0fd581e44f99c1bd137c23648d39cdc86106`
  - User build passed.
  - User reported normal AI conversation runtime.

- xiaozhi-esp32
  - Product commit: `c9d8abf chore(rtc): sync SDK W16b baseline`
  - SDK gitlink: `43ba0fd581e44f99c1bd137c23648d39cdc86106`
  - User build passed.
  - User reported normal AI conversation runtime.

## Beken Status

Beken consumes `/root/smp/ai_iot_sdk` directly and does not need a product SDK gitlink update.

Observed Beken state:

- W16b C facade/control transport sources compile in the Beken build path.
- BK7258 AOSL vendor archive has been captured in the SDK at
  `chip_bk7258/vendor/aosl/libs/libaosl.a`.
- Beken build is accepted after the BK7258 Agora/AOSL vendor baseline update.
- Verification command:
  `make -C /root/smp/bk_solution_ai/projects/beken_genie_rino bk7258 SDK_DIR=/root/smp/bk_avdk_smp`
- Output firmware:
  `/root/smp/bk_solution_ai/projects/beken_genie_rino/build/bk7258/beken_genie_rino/package/all-app.bin`

Beken vendor ownership remains a separate maintenance boundary from the public
facade API:

- Beken Agora vendor baseline ownership
- AOSL symbol ownership
- mbedtls archive/link ownership

## Release Position

Recommended release interpretation:

- RTC SDK code baseline: `43ba0fd5`
- W16b runtime acceptance documentation: `7837b870`
- Product accepted gitlinks: `43ba0fd5`

If a release tag must represent runnable code only, tag `43ba0fd5`.

If a release tag must include closure documentation, tag the final docs closure commit after this note is committed.

## Closure Decision

W16b is closed for SDK default datastream runtime on ESP32S3, xiaozhi, and the
Beken build path.

No further RTC behavior changes are required for W16b.

Remaining work is release management and any future Beken vendor archive
maintenance.
