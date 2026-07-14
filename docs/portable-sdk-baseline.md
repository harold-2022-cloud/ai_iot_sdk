# Phase 7 RTC Vendor/AOSL Baseline

This document records the public portable SDK baseline for integration and
release checks.

## Ownership

SDK owns RTC audio/video/datastream/private RTM backend. The SDK provides the
chip-neutral facade and the private Agora backend integration, including the
private RTM backend option when enabled at build time.

Product owns microphone, speaker, codec, AEC, DMA, LCD, AVI, button,
BLE/Wi-Fi, HTTP/MQTT. Product firmware owns board resources and business flows:

- microphone, speaker, codec, AEC, DMA
- LCD, AVI, camera, buttons, sensors
- BLE/Wi-Fi provisioning
- HTTP/MQTT device business logic

Public RTC facade APIs must not expose board resource policy such as DMA, LCD,
AVI, BLE, Wi-Fi, MQTT, HTTP, or RTM transport selection. Those remain product
or SDK-private concerns.

## Vendor Baseline

ESP32-S3 vendor archives are owned by the SDK private vendor tree:

- `chip_esp32s3/vendor/agora_iot_sdk/libs/libagora-rtc-sdk.a`
- `chip_esp32s3/vendor/agora_iot_sdk/libs/libaosl.a`

BK7258 AOSL baseline archive:

- `chip_bk7258/vendor/aosl/libs/libaosl.a`

BK7258 Agora RTC vendor component:

- `chip_bk7258/vendor/ai_iot_bk7258_agora_iot_sdk/include/bk7258/agora_rtc_api.h`
- `chip_bk7258/vendor/ai_iot_bk7258_agora_iot_sdk/bk7258/libs/librtsa.a`
- `chip_bk7258/vendor/ai_iot_bk7258_agora_iot_sdk/bk7258/libs/libagora-cjson.a`
- `chip_bk7258/vendor/ai_iot_bk7258_agora_iot_sdk/hal/aosl`

When updating Agora/AOSL vendor archives, verify that platform symbols are
provided by exactly one layer. In particular, avoid duplicate `rand_bytes`
definitions between Agora vendor objects and platform mbedtls/PSA objects.

## Build Integration Rules

- Products should consume RTC through `ai_rtc_facade.h`.
- Products should not directly include Agora vendor headers.
- Products should not directly link Agora vendor archives.
- BK7258 products that use the SDK RTC facade should add
  `chip_bk7258/vendor/ai_iot_bk7258_agora_iot_sdk` to their component search
  path so the SDK private Agora component is selected instead of a product-local
  or Beken SDK tree vendor copy.
- BK7258 products should exclude the Beken SDK built-in `agora-iot-sdk`
  component when the SDK-owned `ai_iot_bk7258_agora_iot_sdk` component is used.
- RTM/datastream backend selection is private build-time SDK integration, not a
  public facade selector.

## Verification

Run the host checks before publishing an SDK baseline:

```sh
make -C test/host test
make -C test/host clean
```

The checks enforce public facade boundaries, Agora private integration, and
host behavior for RTC facade lifecycle, datastream, RTM-private transport
stubs, and diagnostics.
