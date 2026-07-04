# AI IoT SDK

AI IoT SDK 是一套面向 AI 聯網設備的可移植 C SDK。它提供設備連接、BLE/Wi-Fi 配網、存儲/OTA 端口，以及面向 AI 語音、視頻和控制消息的跨芯片 RTC facade。

這個 SDK 的目標使用者是方案商和產品開發者：同一套 AI 設備能力可以移植到不同芯片、不同麥克風、喇叭、LCD、按鍵和網絡產品上。

## SDK 提供什麼

| 範圍 | 組件 | 用途 |
| --- | --- | --- |
| 設備連接 | `entity_iot_sdk`, `library/mi_mqtt` | 設備身份、MQTT/HTTP 協議輔助、命令和事件處理 |
| 配網 | `entity_iot_sdk/entity_ble`, `entity_iot_sdk/entity_wifi` | BLE/Wi-Fi 配網流程和 Wi-Fi 狀態整合 |
| RTC 媒體 | `media/rtc_facade` | 跨芯片 RTC audio、video、datastream、生命周期和 callback |
| 平台端口 | `platform_os`, `platform_storage`, `platform_ota` | OS、網絡、同步、存儲和 OTA 抽象 |
| 芯片端口 | `chip_esp32s3`, `chip_bk7258` | 芯片綁定和 vendor 集成 |
| 診斷/測試 | `ai_session_diag`, `test/host` | runtime 診斷和 host/static 邊界測試 |

## RTC 是重點

RTC 對外主 API 是：

```text
media/rtc_facade/include/ai_rtc_facade.h
```

產品代碼只應使用 public `Ai_Rtc_Facade_*` API：

- `Ai_Rtc_Facade_Init`
- `Ai_Rtc_Facade_On_Token_Result`
- `Ai_Rtc_Facade_Stop`
- `Ai_Rtc_Facade_Send_Audio`
- `Ai_Rtc_Facade_Send_Video`
- `Ai_Rtc_Facade_Send_Datastream`
- RTC state/audio/video/datastream callbacks

Agora RTC、AOSL、datastream 和 private RTM backend 都封裝在 SDK private layer。產品代碼不要直接 include Agora vendor header，也不要直接調 Agora vendor API。

## Product 需要自己負責什麼

SDK 負責可移植的協議和 RTC 邊界。產品或板級代碼負責外設和業務策略：

- 麥克風採集
- 喇叭播放
- codec、AEC、gain、PA GPIO
- DMA 和內存策略
- LCD、AVI、JPEG、display flush、動畫策略
- 按鍵和 UI 狀態
- BLE/Wi-Fi 產品交互
- HTTP/MQTT 產品業務流程
- server token request 的參數和設備綁定流程

產品採集本地音頻後，通過 `Ai_Rtc_Facade_Send_Audio()` 發給 RTC。SDK 收到遠端音頻後，通過 `on_audio_rx` callback 交給產品喇叭播放。AI 狀態、情緒、表情、字幕或 UI 事件可以通過 RTC datastream 傳輸，再由產品的 `on_datastream_rx` callback 處理。

## 目前 baseline

目前 SDK baseline 包含：

- ESP32-S3 private Agora RTC vendor component
- BK7258 AOSL vendor baseline
- RTC audio facade
- RTC video facade
- RTC raw datastream facade
- SDK-private Agora datastream control transport
- SDK-private Agora RTM control transport option

public API 預設仍是 datastream-based、transport-neutral。也就是說，上層產品不需要知道底層控制消息是 datastream 還是 RTM。

## 文檔入口

產品開發者建議先讀：

- [RTC facade API](docs/rtc-facade-api.md)
- [AI 對話和情緒 datastream](docs/rtc-ai-dialog-and-emotion.md)
- [移植指南](docs/porting-guide.md)
- [RTC porting contract](docs/rtc-porting-contract.md)
- [vendor binary 說明](docs/vendor-binaries.md)

內部遷移和 release 歷史放在 `docs/superpowers/` 和 `docs/release/`。這些資料對維護者有用，但方案商二次開發應先看上面的公開文檔。

## Build 集成方式

這個 repo 以 components 方式組織。產品項目通常把它作為 Git submodule 或源碼目錄引入，再根據芯片 build system 引入需要的 components。

ESP-IDF 類項目通常通過 `EXTRA_COMPONENT_DIRS` 引入 SDK。

Beken/Armino 類項目通常通過產品 component graph 和 `SDK_DIR` 引入 SDK。

具體產品 build command 屬於產品項目，因為不同芯片 SDK、板級配置和燒錄打包方式都不一樣。

## 驗證

發布或同步到產品前，先跑 SDK host/static verification：

```bash
make -C test/host test
make -C test/host clean
```

重點檢查：

- public RTC header 保持 chip-neutral；
- Agora/vendor 依賴只存在於 SDK private layer；
- datastream/RTM selector 不外露到 public API；
- RTC lifecycle 和 repeat-session 行為有 host guard。

## License

本 SDK repo 使用 Apache License 2.0。詳見 [LICENSE](LICENSE)。

repo 內包含可再分發的 Agora RTC/AOSL vendor binary/header baseline。詳見 [NOTICE](NOTICE) 和 [docs/vendor-binaries.md](docs/vendor-binaries.md)。
