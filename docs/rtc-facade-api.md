# RTC Facade API

RTC facade 是 SDK 對外提供的 AI 實時通信 API。它是 C ABI，目標是保持 chip-neutral 和 vendor-neutral。

public header：

```text
media/rtc_facade/include/ai_rtc_facade.h
```

產品代碼只應 include 這個 header，並調用 `Ai_Rtc_Facade_*` API。產品代碼不要 include Agora vendor header，也不要直接調 Agora vendor API。

## 核心概念

| 概念 | 說明 |
| --- | --- |
| Token result | 服務器下發的 RTC app id、channel、uid/user account、token 和可選 control metadata |
| Facade state | SDK 生命周期狀態，例如 idle、starting、joining、joined、reconnecting、stopping、failed |
| Audio frame | 產品採集後送給 RTC 的本地音頻，或 RTC 收到後交給產品播放的遠端音頻 |
| Video frame | 產品編碼後送給 RTC 的視頻，或 RTC 收到後交給產品渲染的遠端視頻 |
| Datastream message | 透過 RTC 傳輸的原始控制/數據消息，由產品決定語意 |

## 最小流程

```c
#include "ai_rtc_facade.h"

static void on_rtc_state(Ai_Rtc_Facade_Event_t event,
                         Ai_Rtc_Facade_State_t state,
                         int detail,
                         void *user)
{
    (void)user;
    /* callback 裡保持短小，把 UI 或業務工作丟到產品 task。 */
}

static int on_audio_rx(const Ai_Rtc_Facade_Audio_Frame_t *frame, void *user)
{
    (void)user;
    /* frame->data 只在 callback 期間有效，需要保留就先 copy。 */
    return 0;
}

static int on_datastream_rx(const Ai_Rtc_Facade_Datastream_Message_t *message,
                            void *user)
{
    (void)user;
    /* 在這裡解析產品 JSON/binary event，或先 copy 到產品 queue。 */
    return 0;
}

void product_rtc_init(void)
{
    Ai_Rtc_Facade_Config_t cfg = {
        .join_timeout_ms = 10000,
        .enable_audio = true,
        .enable_video = false,
    };
    Ai_Rtc_Facade_Callbacks_t cbs = {
        .on_state = on_rtc_state,
        .on_audio_rx = on_audio_rx,
        .on_video_rx = NULL,
        .on_datastream_rx = on_datastream_rx,
        .user = NULL,
    };

    Ai_Rtc_Facade_Init(&cfg, &cbs);
}
```

產品服務器返回 RTC token 後：

```c
void product_on_token_from_server(void)
{
    Ai_Rtc_Facade_Token_Result_t token = {
        .result = 0,
        .app_id = "your-agora-app-id",
        .channel_name = "ai-dialog-channel",
        .uid = 12345,
        .rtc_token = "rtc-token-from-server",
        .user_account = NULL,
        .control_peer_id = NULL,
        .control_token = NULL,
        .enable_audio_ai_qos = false,
    };

    Ai_Rtc_Facade_On_Token_Result(&token);
}
```

`Ai_Rtc_Facade_On_Token_Result()` 會 copy 必要 token 欄位，呼叫返回後，caller 不需要保持 struct 長期有效。

## Token 欄位

| 欄位 | 必填 | 說明 |
| --- | --- | --- |
| `result` | 是 | `0` 表示 token request 成功。非 0 會被視為失敗。 |
| `app_id` | 是 | Agora RTC app id。 |
| `channel_name` | 是 | 要加入的 RTC channel。 |
| `uid` | numeric join 時必填 | 本地 numeric RTC uid。 |
| `rtc_token` | 是 | channel/user 對應 RTC token。 |
| `user_account` | 可選 | string UID。若提供，backend 會在支持的平台上走 string-account join。 |
| `control_peer_id` | 可選 | private control transport peer。RTM 模式會用到；預設 datastream 不需要。 |
| `control_token` | 可選 | private control transport token。若沒有，backend 可重用 `rtc_token`。 |
| `enable_audio_ai_qos` | 可選 | 當次 session 是否啟用 Agora audio AI QoS join option。預設 `false`，應由產品配置和雲端 conversation response 共同決定。 |

public API 預設使用 RTC datastream。RTM 是 SDK-private backend option，不改變 public API。

## AI QoS

`enable_audio_ai_qos` 是 per-session option，放在 `Ai_Rtc_Facade_Token_Result_t`，不是全局 init config。原因是 AI QoS 通常取決於當次 AI conversation 的雲端策略。

典型流程：

```text
產品請求 /conversations/start
        |
        v
雲端根據設備 capability / agent / region 決定 ai_qos=true/false
        |
        v
產品填入 token.enable_audio_ai_qos
        |
        v
SDK Agora backend 映射到 enable_audio_ai_qos join option
```

示例：

```c
Ai_Rtc_Facade_Token_Result_t token = {
    .result = 0,
    .app_id = app_id,
    .channel_name = channel,
    .uid = uid,
    .rtc_token = rtc_token,
    .enable_audio_ai_qos = cloud_ai_qos_enabled,
};
```

如果產品沒有填這個欄位，C zero-init 會保持 `false`，舊產品行為不變。

## Control transport build-time 選擇

`Ai_Rtc_Facade_Send_Datastream()` 是 SDK public API 的控制消息抽象。它不代表產品一定直接使用 Agora RTC datastream。SDK private Agora backend 可以在 firmware build-time 選擇用 RTC datastream 或 RTM 實作這個控制消息通道。

預設不指定時使用 RTC datastream：

```c
AI_RTC_AGORA_CONTROL_TRANSPORT_DATASTREAM
```

如果產品 firmware 要使用 RTM，在 SDK RTC facade component 的編譯參數中指定：

```c
AI_RTC_AGORA_CONTROL_TRANSPORT=AI_RTC_AGORA_CONTROL_TRANSPORT_RTM
```

上層產品 API 不變，仍然使用：

```c
Ai_Rtc_Facade_Send_Datastream(data, len);
```

RTM build-time 選擇後，token result 必須提供：

- `user_account`：本地用戶 string UID，同時用於 RTC string UID join 和 RTM login；
- `control_peer_id`：遠端 AI agent 的 RTM uid；
- `control_token`：RTM token，可選；沒有時 backend 可重用 `rtc_token`。

不要在 `ai_rtc_facade.h` 新增 RTM public API。RTM/datastream 是 SDK private backend 選擇，不是產品 runtime selector。

## State 和 Event

重要 state：

| State | 說明 |
| --- | --- |
| `AI_RTC_FACADE_STATE_IDLE` | 沒有 active RTC session，可以 start。 |
| `AI_RTC_FACADE_STATE_TOKEN_READY` | token 已被接受和 copy，backend start 即將執行。 |
| `AI_RTC_FACADE_STATE_STARTING` | backend service/connection setup 正在執行。 |
| `AI_RTC_FACADE_STATE_JOINING` | RTC join request 已送出，但 join success callback 還沒到。 |
| `AI_RTC_FACADE_STATE_JOINED` | 本地 join channel 成功。media/data 仍可能還有 backend readiness gate。 |
| `AI_RTC_FACADE_STATE_RECONNECTING` | backend 回報網絡 reconnect。ready 條件要等 callback 重建。 |
| `AI_RTC_FACADE_STATE_STOPPING` | leave/stop 正在進行。 |
| `AI_RTC_FACADE_STATE_FAILED` | token、backend start、join、renew 或 runtime 失敗。 |

產品應把 `AI_RTC_FACADE_ERR_BUSY` 視為 lifecycle gate，不是 server failure。當 state 是 starting、joining 或 stopping 時，不要快速連續 start/stop。

## Audio TX

產品負責麥克風採集，然後把音頻 frame 送給 facade：

```c
Ai_Rtc_Facade_Audio_Frame_t frame = {
    .data = pcm,
    .len = pcm_len,
    .format = AI_RTC_FACADE_AUDIO_FORMAT_PCM16,
    .sample_rate_hz = 16000,
    .channels = 1,
    .duration_ms = 20,
    .timestamp_ms = now_ms,
};
int rc = Ai_Rtc_Facade_Send_Audio(&frame);
```

在 channel joined 且遠端 AI user joined 前，SDK 可能返回 `AI_RTC_FACADE_ERR_NOT_READY`。產品在啟動、reconnect 和快速 restart 時應能容忍這個返回值。

## Audio RX

遠端音頻通過 `on_audio_rx` 送到產品。產品負責喇叭播放。RX pointer 只在 callback 期間有效，需要延後使用就先 copy 或 enqueue。

## Video

啟用 video：

```c
Ai_Rtc_Facade_Config_t cfg = {
    .enable_audio = true,
    .enable_video = true,
};
```

產品通過 `Ai_Rtc_Facade_Send_Video()` 發 encoded frame。產品仍負責 camera capture、encoding、frame rate、keyframe policy 和 rendering。

## Datastream

datastream 適合傳 AI 狀態、情緒、表情、字幕或產品命令：

```c
const char *json = "{\"type\":\"emotion\",\"emotion\":\"happy\"}";
Ai_Rtc_Facade_Send_Datastream((const uint8_t *)json, strlen(json));
```

datastream send 需要 session joined 且 backend stream ready。如果 SDK 返回 `AI_RTC_FACADE_ERR_NOT_READY`，產品可以在 joined/datastream-ready 條件成立後重試。

## Callback 規則

callback 可能從 backend/vendor context 觸發：

- callback 裡保持短小；
- RX buffer 需要保留就先 copy；
- UI、audio heavy work、storage、network work 丟給產品 task；
- 不要在產品代碼直接調 Agora vendor API；
- 不要 free SDK-owned RX buffer；
- 不要假設 callback thread/core 在所有芯片上固定。

## 驗證

```bash
make -C test/host test
make -C test/host clean
```
