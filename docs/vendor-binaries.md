# Vendor Binary Baseline

本文件記錄 SDK repo 內包含的 vendor binary/header baseline，以及替換時需要遵守的原則。

## 目前包含的 vendor 文件

ESP32-S3 Agora RTC baseline：

```text
chip_esp32s3/vendor/agora_iot_sdk/include/agora_rtc_api.h
chip_esp32s3/vendor/agora_iot_sdk/include/autoconfig.h
chip_esp32s3/vendor/agora_iot_sdk/libs/libagora-rtc-sdk.a
chip_esp32s3/vendor/agora_iot_sdk/libs/libaosl.a
```

BK7258 AOSL baseline：

```text
chip_bk7258/vendor/aosl/libs/libaosl.a
```

## 使用原則

- vendor API 只能在 SDK private backend 或 chip port 中使用；
- public header 不能 include vendor header；
- 產品代碼不能直接 link Agora/AOSL archive；
- 產品代碼應通過 `ai_rtc_facade.h` 使用 RTC；
- 替換 vendor archive/header 後，必須重跑 host/static verification 和產品 build/runtime。

## Beken baseline 注意事項

BK7258 的 Agora/AOSL/mbedtls link 順序和 symbol 可能受 Beken SDK 版本影響。過去曾遇到 `rand_bytes` multiple definition，原因是 Agora archive 和 Beken `psa_mbedtls` 同時提供相同 symbol。

處理原則：

- 保持 Agora archive、AOSL archive、Beken SDK 版本成組管理；
- 不要只替換單一 `.a`；
- link conflict 要在 SDK baseline 文檔中記錄；
- Beken build accepted 後，再同步到產品 repo。

## 發布前檢查

```bash
make -C test/host test
make -C test/host clean
```

同時至少完成一個產品 clean build 和一次 runtime AI 對話驗證。
