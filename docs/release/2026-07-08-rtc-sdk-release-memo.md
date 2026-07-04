# RTC SDK Release Memo

> Date: 2026-07-08
> Scope: Phase 7 RTC facade / Agora backend / BK7258 AOSL baseline
> Tag target: `phase7-rtc-sdk-contract-baseline`

## Release Purpose

This memo records the current RTC SDK baseline before cleaning build outputs,
removing stale SDK copies, and publishing a fresh GitHub repository.

The baseline goal is not to change product behavior. It freezes the SDK boundary
that was validated across ESP32S3 AI alarm, xiaozhi-esp32, and Beken BK7258
product integrations:

- RTC audio/video/datastream facade is SDK-owned.
- Agora vendor integration is SDK-private.
- RTM exists only as a private Agora control transport option.
- Product-specific peripherals and business flows stay product-owned.

## Current SDK Boundary

SDK-owned:

- `media/rtc_facade/include/ai_rtc_facade.h`
- RTC token-to-join lifecycle
- audio TX/RX facade
- video TX/RX facade
- raw datastream TX/RX facade
- private Agora datastream control transport
- private Agora RTM control transport option
- Agora service lifecycle and channel option mapping
- SDK-private datastream envelope parser
- ESP32S3 Agora vendor archive/header component
- BK7258 AOSL baseline archive

Product/platform-owned:

- microphone capture path
- speaker playback path
- codec, AEC, gain, PA GPIO
- DMA and memory policy
- LCD, AVI, JPEG decode, DMA2D, display flush
- buttons and UI state
- BLE/Wi-Fi provisioning
- HTTP/MQTT product business flows
- server token request shape and product binding logic

Public RTC facade must not expose product/platform policy such as DMA,
resource thresholds, LCD/AVI controls, BLE/Wi-Fi, HTTP/MQTT, or a public
datastream/RTM selector.

## Baseline Contents

Key RTC SDK changes included in this release line:

- Agora RTC integration moved behind SDK facade.
- Product code consumes `Ai_Rtc_Facade_*` instead of direct Agora vendor APIs.
- Audio TX is gated by backend join and remote AI user joined state.
- Reconnect/rejoin clears remote-user readiness until a fresh callback arrives.
- Stop is session-scoped; full Agora SDK shutdown is reserved for deinit/shutdown.
- String UID join is supported when `user_account` is provided.
- Datastream remains the public control transport surface.
- RTM is available only as an SDK-private Agora control transport option.
- Host/static guards prevent public facade boundary regressions.
- BK7258 `libaosl.a` baseline is tracked under SDK vendor paths.

## Platform Runtime Status

ESP32S3 AI alarm:

- Consumes SDK RTC facade.
- Runtime voice dialog was accepted after W18 diagnostics/hardening.
- Product-side UI busy gate remains a product improvement item for rapid AI
  button toggles.

xiaozhi-esp32:

- Consumes SDK RTC facade.
- Runtime voice dialog was accepted after SDK baseline sync.
- Video and RTM are not required for normal voice dialog.

Beken BK7258:

- Consumes SDK RTC facade through product bridge.
- Runtime voice dialog was accepted with the current RTC SDK baseline.
- BK7258 AOSL baseline is tracked for newer Agora/AOSL integration.
- Beken SDK 3.1.1.8 migration is a product/platform integration topic, not a
  public RTC facade change.

## Beken R1 Integration Notes

Beken R1 uses product-owned LCD/AVI/JPEG/DMA2D/display flush paths. These paths
can compete with the audio/RTC hot path. The SDK should not expose tuning knobs
for those peripherals.

Recommended product policy:

- keep full AVI/display workload for idle or non-dialog states,
- reduce animation rate, use static frames, or skip frames during active AI
  dialog,
- throttle product diagnostics on audio/RTC hot paths,
- compare old/new Beken SDK behavior with the same product code and the same RTC
  SDK baseline,
- classify issues first as mic capture, speaker playback, RTC uplink/downlink,
  or visual workload starvation before changing SDK code.

Observed Beken integration risks to keep documented:

- `AEL_IO_ABORT` behavior changed between Beken SDK revisions.
- AVI/JPEG/DMA2D/display flush can introduce RTC TX timing gaps.
- `enable_audio_ai_qos` can affect perceived dialog smoothness and should be
  validated per product/platform.
- Agora/AOSL/mbedtls archive mismatches can produce symbols such as
  `rand_bytes` multiple definition errors.

## Vendor Baseline Rules

- Keep Agora RTC archive/header, AOSL archive, and chip SDK crypto/mbedtls
  libraries as one matched baseline.
- Do not mix `libagora-rtc-sdk.a` from one baseline with `libaosl.a` from
  another baseline.
- Do not solve vendor link conflicts by exposing vendor APIs in
  `ai_rtc_facade.h`.
- Update `docs/portable-sdk-baseline.md` whenever replacing Agora archive,
  Agora header, `autoconfig.h`, or AOSL archive.

## Dirty Worktree Notes

The BLE/Wi-Fi provisioning state change is intentionally not part of this RTC
release baseline unless committed separately:

- `entity_iot_sdk/entity_iot/entity_iot_cloud.h`
- `entity_iot_sdk/entity_wifi/entity_wifi.c`

Those files add a product-useful provisioning state:

```text
DEV_BLE_PROVISION_WIFI_CONNECTED_STATE
```

This is an entity provisioning change, not an RTC facade change.

## Next Release Steps

1. Run the clean build checklist in
   `docs/release/2026-07-08-rtc-sdk-clean-build-checklist.md`.
2. Tag the SDK repo after this memo and checklist are committed.
3. Sync the tagged SDK baseline to product repos only after host verification is
   clean.
4. Archive or remove stale SDK copies only after a clean clone can build.
