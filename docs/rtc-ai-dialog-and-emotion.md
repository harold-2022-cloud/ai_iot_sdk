# AI 對話和情緒 Datastream

本文件說明產品如何用 RTC 完成 AI 語音對話，並用 RTC datastream 傳遞 AI 狀態、情緒、表情、字幕或控制消息。

無論 SDK private backend 使用 Agora datastream 還是 private RTM，public API 都保持一致：

```text
Ai_Rtc_Facade_Send_Datastream()
Ai_Rtc_Facade_Callbacks_t.on_datastream_rx
```

## 架構

```text
產品按鍵 / wake word
        |
        v
產品服務器 token request
        |
        v
Ai_Rtc_Facade_On_Token_Result()
        |
        v
RTC join lifecycle
        |
        +--> 產品 mic -> Ai_Rtc_Facade_Send_Audio()
        |
        +--> on_audio_rx -> 產品 speaker
        |
        +--> on_datastream_rx -> 產品 emotion/UI/LED/LCD/robot action
```

SDK 負責 RTC lifecycle 和 media/data transport。產品負責本地音頻設備、顯示、表情模型，以及 datastream message 的業務語意。

## 建議消息格式

建議產品和 AI server 使用簡單的 versioned JSON envelope：

```json
{
  "version": 1,
  "type": "emotion",
  "conversation_id": "conv-123",
  "state": "speaking",
  "emotion": "happy",
  "text": "Hello",
  "ts_ms": 123456
}
```

建議欄位：

| 欄位 | 說明 |
| --- | --- |
| `version` | 產品消息 schema 版本。 |
| `type` | `emotion`、`dialog_state`、`caption`、`control` 或產品自定義類型。 |
| `conversation_id` | 產品/server 對話 id，用來過濾 stale message。 |
| `state` | AI 狀態，例如 `thinking`、`speaking`、`listening`、`idle`。 |
| `emotion` | 表情提示，例如 `neutral`、`happy`、`sad`、`surprised`。 |
| `text` | 可選字幕或 assistant 文本片段。 |
| `ts_ms` | 可選 sender timestamp。 |

SDK 不強制這個 schema。這是 AI server 和設備 UI/business layer 之間的產品協議。

## 設備接收示例

```c
static int on_datastream_rx(const Ai_Rtc_Facade_Datastream_Message_t *message,
                            void *user)
{
    (void)user;

    if (message == NULL || message->data == NULL || message->len == 0) {
        return 0;
    }

    /*
     * callback 要保持短小。實際產品中建議 copy payload 到產品 queue，
     * 再由 UI/business task 解析 JSON。
     */
    product_enqueue_ai_event(message->data, message->len);
    return 0;
}
```

產品 task：

```c
void product_ai_event_task(void)
{
    Product_Ai_Event_t event;

    while (product_dequeue_ai_event(&event)) {
        Product_Emotion_t emotion = PRODUCT_EMOTION_NEUTRAL;
        Product_Ai_State_t state = PRODUCT_AI_STATE_IDLE;

        if (product_parse_ai_event(event.data, event.len, &state, &emotion)) {
            product_expression_set(emotion);
            product_ui_set_ai_state(state);
        }
    }
}
```

不要在 `on_datastream_rx` callback 裡做 heavy JSON parsing、LCD rendering、storage write 或 network request。

## 設備發送示例

產品也可以用同一個 public API 發送控制消息給 server：

```c
const char *msg =
    "{\"version\":1,\"type\":\"control\",\"command\":\"barge_in\"}";

int rc = Ai_Rtc_Facade_Send_Datastream((const uint8_t *)msg, strlen(msg));
if (rc == AI_RTC_FACADE_ERR_NOT_READY) {
    /* 產品可在 join/datastream readiness 成立後重試。 */
}
```

## 對話狀態映射

常見映射：

| RTC event / message | 產品行為 |
| --- | --- |
| `AI_RTC_FACADE_EVENT_JOINED` | 顯示 connecting/ready。不要假設遠端 AI user 已 joined。 |
| `AI_RTC_FACADE_EVENT_REMOTE_USER_JOINED` | 開始允許 mic uplink。 |
| `AI_RTC_FACADE_EVENT_RECONNECTING` | 顯示 reconnecting；暫停 audio/datastream ready 假設。 |
| datastream `state=thinking` | 顯示思考表情/動畫。 |
| datastream `state=speaking` | 顯示說話表情並播放 `on_audio_rx`。 |
| datastream `emotion=happy` | 切換 face/LED/LCD/avatar 到 happy。 |
| `AI_RTC_FACADE_EVENT_STOPPED` | 清理 active dialog UI。 |
| `AI_RTC_FACADE_EVENT_FAILED` | 顯示產品定義的 failure state。 |

## RTM 和 Datastream

public API 不暴露 transport selector。

預設路徑：

```text
Product -> Ai_Rtc_Facade_Send_Datastream()
SDK private Agora backend -> RTC datastream
```

private RTM backend option：

```text
Product -> Ai_Rtc_Facade_Send_Datastream()
SDK private Agora backend -> RTM
```

產品不應關心 private transport 具體是 datastream 還是 RTM。這個選擇是 firmware build-time 決定，不是產品 runtime API 決定。

如果產品 firmware 要使用 RTM，在 RTC facade component 編譯時指定：

```c
AI_RTC_AGORA_CONTROL_TRANSPORT=AI_RTC_AGORA_CONTROL_TRANSPORT_RTM
```

RTM build-time 選擇後，token result 需要提供 `user_account` 和 `control_peer_id`，可能還需要 `control_token`。產品 UI 和業務代碼仍然調同一套 public facade function：

```c
Ai_Rtc_Facade_Send_Datastream(data, len);
```

## AI QoS

AI QoS 是當次 RTC session 的 join option，建議由雲端 conversation/start response 或產品配置共同決定。產品拿到雲端結果後填入：

```c
token.enable_audio_ai_qos = cloud_ai_qos_enabled;
```

SDK 不會默認強制開啟 AI QoS；未填時預設為 `false`。這樣可以避免雲端未配合、芯片 vendor SDK 行為不同，或弱網策略不一致時造成難以定位的對話問題。

## 常見錯誤

- 把 datastream payload 當成 SDK 固定 schema。實際上它是 product-owned。
- 直接在 RTC callback 裡更新 LCD/LED/expression。
- 遠端 AI user 還沒 joined 就開始送 audio。
- 在 SDK 裡寫死 AI QoS，而不是根據當次雲端 conversation response 設置。
- reconnect 後沿用舊的 remote-user/datastream ready 假設。
- 把 RTM API 加到 `ai_rtc_facade.h`。
- 嘗試在 runtime 切換 RTM/datastream；目前這是 build-time private backend choice。
- 產品代碼直接調 Agora API。

## Debug marker

診斷 AI 對話時常用 marker：

```text
AI_RTC_FACADE_CORE
AI_RTC_AGORA
sdk init
create_conn
join
callback joined
callback remote_user_joined
send_audio not_ready
send_datastream not_ready
callback reconnecting
callback rejoined
callback stale
```

先分類卡在哪一段：token、backend start、本地 join、遠端 AI user join、audio TX、audio RX、datastream readiness、reconnect 或 stop/restart。
