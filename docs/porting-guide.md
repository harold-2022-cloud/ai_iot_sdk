# 移植指南

本文件說明方案商把 SDK 移植到新芯片或新板子時，哪些部分應該改 SDK，哪些部分應該留在產品/板級代碼。

## 分層規則

請保持以下分層清楚：

| Layer | 負責內容 |
| --- | --- |
| SDK public API | 可移植 IoT/RTC API 和 data structure |
| SDK private backend | vendor RTC 集成、private datastream/RTM transport、chip adapter glue |
| Chip/platform port | OS、queue、semaphore、time、memory、network、storage、OTA、vendor component wiring |
| Product/board | mic、speaker、codec、AEC、DMA、LCD、button、BLE/Wi-Fi UX、HTTP/MQTT 產品流程 |

如果一個改動涉及 GPIO number、LCD timing、DMA descriptor、PA enable、button 行為或板級內存策略，它通常屬於 product/board code 或 chip port，不應加到 `ai_rtc_facade.h`。

## 重要目錄

```text
entity_iot_sdk/                 設備協議、BLE/Wi-Fi、MQTT/HTTP helpers
media/rtc_facade/               public RTC facade 和 private Agora backend
platform_os/                    OS/network/sync/memory 類抽象
platform_storage/               storage abstraction 和 proxy helpers
platform_ota/                   OTA abstraction 和 backend helpers
chip_esp32s3/                   ESP32-S3 chip binding 和 vendor RTC component
chip_bk7258/                    BK7258 chip binding 和 AOSL baseline
test/host/                      host/static tests 和 public boundary guards
docs/                           public 和 maintainer documentation
```

## 在已支持芯片上新增產品

如果新板子使用 SDK 已支持的芯片：

1. 儘量保持 SDK code 不變。
2. 在產品 repo 建立 board config。
3. 配置麥克風和喇叭路徑。
4. 配置 codec、sample rate、AEC、gain 和 PA GPIO。
5. 如果產品有 LCD/AVI，配置 display/animation workload policy。
6. 配置按鍵和 UI 狀態機。
7. 需要配網時，引入 BLE/Wi-Fi provisioning components。
8. 從產品服務器請求 RTC token。
9. 把 token response 傳給 `Ai_Rtc_Facade_On_Token_Result()`。
10. 把 mic frame 傳給 `Ai_Rtc_Facade_Send_Audio()`。
11. 把 `on_audio_rx` 交給產品 speaker code 播放。
12. 在 `on_datastream_rx` 解析產品 AI state、emotion 或 event。

## 新芯片 port

如果是新芯片或新 RTOS，需要實作或適配：

```text
platform_os/
platform_storage/
platform_ota/
chip_<your_chip>/
```

chip port 通常需要提供：

- task/thread creation；
- semaphore/queue primitives；
- sleep/time；
- memory allocation wrappers；
- logging；
- network wrappers；
- storage binding；
- OTA binding；
- RTC vendor component wiring。

RTC backend private port 在：

```text
media/rtc_facade/src/agora/ai_rtc_agora_port.h
```

每個芯片 implementation 都要滿足 port functions，但不能讓 chip header 暴露到 `media/rtc_facade/include/ai_rtc_facade.h`。

## RTC 移植 checklist

測 AI 對話前先確認：

- local mic capture 正常；
- local speaker playback 正常；
- sample rate 和 frame duration 符合 server/backend 預期；
- AEC/gain policy 已確認；
- DMA/internal heap/PSRAM budget 對當前板子可接受；
- 產品 UI 不在 audio hot path 啟動過重的 LCD/AVI/JPEG 工作；
- token server 返回 `app_id`、`channel_name`、`uid` 或 `user_account`、`rtc_token`；
- 如果雲端開啟 AI QoS，產品把結果填到 `Ai_Rtc_Facade_Token_Result_t.enable_audio_ai_qos`；
- 產品在 RTC setup 時調一次 `Ai_Rtc_Facade_Init()`；
- 產品在 token response 後調 `Ai_Rtc_Facade_On_Token_Result()`；
- audio TX 只通過 `Ai_Rtc_Facade_Send_Audio()`；
- 產品能處理 startup/reconnect 期間的 `AI_RTC_FACADE_ERR_NOT_READY`；
- 產品能處理 rapid start/stop 期間的 `AI_RTC_FACADE_ERR_BUSY`。

## RTM build-time 選擇

SDK public API 對外只暴露 transport-neutral 的控制消息接口：

```c
Ai_Rtc_Facade_Send_Datastream(data, len);
```

Agora private backend 可以用 RTC datastream 或 RTM 實作這個接口。目前選擇在 firmware build-time 決定，不提供產品 runtime selector。

預設使用 RTC datastream。若產品 firmware 要使用 RTM，在 RTC facade component 的編譯參數中加入：

```c
AI_RTC_AGORA_CONTROL_TRANSPORT=AI_RTC_AGORA_CONTROL_TRANSPORT_RTM
```

ESP-IDF 類產品可以在對應 component CMake 中加入：

```cmake
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    AI_RTC_AGORA_CONTROL_TRANSPORT=AI_RTC_AGORA_CONTROL_TRANSPORT_RTM
)
```

Beken/Armino 類產品則放在對應 component compile definitions 中。不要把這個 macro 寫進 `ai_rtc_facade.h`，也不要讓產品業務代碼直接 include Agora RTM header。

RTM build-time 選擇後，產品 server 的 token response 要能提供：

- `user_account`：本地 string UID；
- `control_peer_id`：遠端 AI agent RTM uid；
- `control_token`：可選 RTM token，沒有時 backend 可重用 `rtc_token`。

如果某個芯片的 vendor Agora archive/header 不支持 RTM，該產品應保持預設 datastream build，或在芯片 port 中明確返回 unsupported，不能讓產品 link 到不存在的 RTM symbol。

## 產品按鍵 gate

RTC join 是異步的。backend join request 成功送出，不代表 channel 已經 joined。產品 UI/button 應做 gate：

- 只允許從 `IDLE` 或 `FAILED` start；
- `TOKEN_READY`、`STARTING`、`JOINING` 期間忽略重複 start；
- `JOINING` 期間不要 destructive stop；
- `STOPPING` 期間不要 start 新對話；
- 把 `ERR_BUSY` 視為 lifecycle in-flight gate。

## Debug checklist

AI 語音失敗時，先分類第一個失敗階段：

1. token request/response；
2. `Ai_Rtc_Facade_On_Token_Result()`；
3. backend SDK init；
4. connection create；
5. join request；
6. joined callback；
7. remote AI user joined callback；
8. first audio TX；
9. audio RX callback；
10. datastream readiness；
11. reconnect/rejoin；
12. stop/restart cleanup。

不要在 log 還沒證明是 resource starvation 或 board timing 前，就先改 DMA/LCD/audio numeric thresholds。

## Host verification

從 SDK root 執行：

```bash
make -C test/host test
make -C test/host clean
```

static guard 會檢查：

- public RTC header 保持 chip-neutral；
- Agora/vendor API 只在 private layer；
- datastream/RTM selector 保持 private；
- core lifecycle behavior 有 guard；
- host test binaries 可被 `clean` 清除。

## Open-source release checklist

發布產品或 SDK baseline 前：

- 確認 `LICENSE` 和 `NOTICE`；
- 確認 vendor binary redistribution terms；
- 跑 SDK host verification；
- 至少用 clean clone build 一個產品；
- runtime 測 first AI dialog 和 repeated AI dialog；
- 記錄 chip SDK 和 vendor archive baseline；
- internal migration notes 與 public getting-started docs 分開。
