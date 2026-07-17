# Phase 7 RTC W16 Agora Vendor Baseline

## Accepted Baseline

- ESP32S3 SDK vendor Agora baseline is updated from:
  `/mnt/c/Users/harold.chen/esp32-2/components/agora_rtc`.
- The SDK baseline now carries the RTM-capable Agora RTC header and archive:
  - `chip_esp32s3/vendor/agora_iot_sdk/include/agora_rtc_api.h`
  - `chip_esp32s3/vendor/agora_iot_sdk/include/autoconfig.h`
  - `chip_esp32s3/vendor/agora_iot_sdk/libs/libagora-rtc-sdk.a`
- The vendor archive exposes both existing datastream symbols and RTM symbols:
  - `agora_rtc_create_data_stream`
  - `agora_rtc_send_stream_message`
  - `agora_rtc_login_rtm`
  - `agora_rtc_logout_rtm`
  - `agora_rtc_send_rtm_data`

## Boundary

- This is a vendor baseline update, not a public facade API change.
- `ai_rtc_facade.h` must not expose RTM APIs or control-transport selector
  APIs.
- Phase 7 accepted runtime transport remains RTC datastream.
- RTM remains a future SDK-private backend implementation option.
- AOSL source is not imported in this baseline. The SDK still uses the existing
  `chip_esp32s3/vendor/agora_iot_sdk/libs/libaosl.a` packaging boundary.

## Verification

- `nm` confirms the new `libagora-rtc-sdk.a` exports datastream and RTM symbols.
- `make -C /root/smp/ai_iot_sdk/test/host test` passed after the vendor update.
- `make -C /root/smp/ai_iot_sdk/test/host clean` removed host test binaries.
