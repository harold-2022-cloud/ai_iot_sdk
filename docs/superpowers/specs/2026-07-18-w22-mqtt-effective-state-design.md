# W22 MQTT Effective State Cleanup Design

> Status: SDK host implementation accepted
> Scope: SDK MQTT layer only
> Target SDK: `/mnt/c/Users/harold.chen/ai_iot_sdk`
> Date: 2026-07-18

## 0. Implementation Status

SDK host implementation accepted on 2026-07-18.

Implemented commits:

- `6f2774b6` test(mqtt): add effective state matrix
- `5526aa01` fix(mqtt): add effective state helper
- `a9d5e143` fix(mqtt): gate publish enqueue by effective state
- `371440e9` fix(mqtt): gate agent drain by effective state
- `a6f64c54` / `fe116b44` fix(mqtt): centralize AI reconnect decision and suppress duplicate reconnect force requests
- `d108b0d5` / `9838dd1d` / `3aee5b4b` / `ac81e139` / `2cf24e5a` test(mqtt): guard effective state policy regressions

Host verification:

```bash
make -C /mnt/c/Users/harold.chen/ai_iot_sdk/test/host test
make -C /mnt/c/Users/harold.chen/ai_iot_sdk/test/host clean
```

Result: passed, then host binaries were removed with `make clean`.

Runtime validation status: pending product firmware validation on Beken R1, Beken SpeedTech Wi-Fi, and ESP32S3 ai_alarm after syncing this SDK baseline.

## 1. Purpose

目前 MQTT publish / drain / reconnect 判斷分散在三個 raw 狀態：

- `Entity_Mqtt_Context_t.Is_Connected`
- `s_mqtt_app_connected`
- `Mqtt_Client_Broker_Liveness_Snapshot().state`

這三個值各自有合理用途，但現在它們同時被拿來做控制邏輯，導致狀態短暫不一致時容易產生誤判。例如：

- `PROBING` 被當成 reconnect reason，可能打斷正常等待 PINGRESP 的連線。
- queue drain 只看 `Is_Connected`，可能把已排隊 publish 送進下行已不健康或即將斷開的 socket。
- app 層、client 層、AI gate 各自解讀連線狀態，後續維護者很容易只改其中一處造成 regression。

W22 的目標不是增加保守等待，也不是改變 MQTT 業務流程，而是把 raw 狀態收斂成唯一 effective state，讓 publish、drain、reconnect 使用一致判斷。

## 2. Non-Goals

W22 不做以下事情：

- 不改 RTC public API。
- 不改 MQTT topic、payload、QoS、token request protocol。
- 不改產品按鍵邏輯。
- 不把 MQTT 連線時間刻意拉長。
- 不新增高頻 log。
- 不把 broker liveness raw fields 移除；它們仍保留作診斷。
- 不把 `PROBING` 當成斷線。

## 3. Existing Raw Inputs

### `context->Is_Connected`

位置：

```text
entity_iot_sdk/entity_mqtt/entity_mqtt_client.h
entity_iot_sdk/entity_mqtt/entity_mqtt_client.c
```

語義：

- client state machine 內部連線旗標。
- subscribe complete 後設為 true。
- disconnect / yield failure / reconnect begin 時設為 false。

現有用途：

- `Entity_Mqtt_App_Drain_Agent_Queue()` 用它決定是否排空 publish queue。
- `Entity_Mqtt_Topic_Publish()` 經由 `Entity_Mqtt_Is_Connected()` 用它決定是否 publish。

問題：

- 它不能代表 broker 下行一定健康。
- 它也不能代表 app 層已完成 connected post work。

### `s_mqtt_app_connected`

位置：

```text
entity_iot_sdk/entity_mqtt/entity_mqtt_app.c
```

語義：

- app 層快速 connected flag。
- connected/disconnected callback 更新。
- 外部 task 可無鎖查詢。

現有用途：

- `Entity_Mqtt_App_Topic_Publish()` 用它決定是否接受 enqueue。
- `Entity_Mqtt_App_Is_Connected()` 直接回傳它。

問題：

- 它是 app 層視角，不等於 socket/broker liveness。
- 如果和 `context->Is_Connected` 或 broker state 不一致，現有代碼沒有統一 reconcile 結果。

### `broker.state`

位置：

```text
library/mi_mqtt/mi_mqtt_client.h
chip_bk7258/mqtt/mi_mqtt_client_bk7258.c
chip_esp32s3/mqtt/mi_mqtt_client_esp32.c
```

語義：

- broker liveness 診斷狀態。
- BK7258 會由 core MQTT keepalive / PINGRESP / last inbound 推導。
- ESP32S3 在有 broker-liveness extension 時由 esp-mqtt 提供；沒有 extension 時降級。

現有用途：

- AI publish 前健康檢查。
- AI token 等待期間健康檢查。
- health snapshot log。

問題：

- `PROBING` 目前可能被 app 層當成 reconnect reason。
- `broker.state` 的 normalize/reconcile 現在寫在 app 層，導致 client 層和 app 層各自理解狀態。

## 4. Effective State

新增一個 SDK MQTT effective state 概念。名稱可以在實作時再定，但語義固定：

```text
DISCONNECTED
CONNECTING
SUBSCRIBING
READY
PROBING
DEAD
```

這個狀態由 client 層的 helper 根據 raw inputs 推導。`s_mqtt_app_connected`
仍由 app 層維護，但 app 層只把它當參數傳給 client helper；reconcile 規則
不散落在 app 層。app 層不再自行組合 raw fields 作 publish / drain /
reconnect 控制。

### State Meaning

| Effective state | Meaning | Publish behavior | Reconnect behavior |
|---|---|---|---|
| `DISCONNECTED` | MQTT/socket 明確未連線，或 context 不可用 | 不 publish | 立即 request reconnect |
| `CONNECTING` | 正在 TCP/MQTT connect 或 reconnect pending | 不 publish | 不重複 request reconnect |
| `SUBSCRIBING` | MQTT connected callback 已到，但 topic subscribe 尚未完成 | 不 publish | 不重複 request reconnect |
| `READY` | app connected + ctx connected + broker healthy | 立即 publish | 不 reconnect |
| `PROBING` | 已連線，正在等待 PINGRESP 或 broker liveness probing | 不誤重連；可短促進 MQTT loop | 不 reconnect，除非轉成 `DEAD` |
| `DEAD` | PINGRESP timeout、明確 socket failure、stale puback 或 broker dead | 不 publish | 立即 request reconnect |

## 5. Derivation Rules

有效狀態推導規則應集中在 client 層。因為 `s_mqtt_app_connected` 是 app
層 static flag，client helper 不應直接引用它；建議接口由 app 層傳入 raw
app flag，例如：

```text
Entity_Mqtt_Get_Effective_State(context, app_connected, stale_puback)
```

或使用一個 input struct：

```text
context
app_connected
stale_puback
```

具體命名可在 implementation plan 決定，但 reconcile 規則只能有一份。

建議規則：

1. `context == NULL` 或 `context->Mqtt_Client == NULL`：
   - `DISCONNECTED`

2. `context->State == ENTITY_MQTT_IDLE_STATE`：
   - `DISCONNECTED`

3. `context->State == ENTITY_MQTT_CONNCET_STATE` 或 `ENTITY_MQTT_RECONNECT_STATE`：
   - `CONNECTING`

4. `context->State == ENTITY_MQTT_SUBSCRIBING_STATE` 或 `ENTITY_MQTT_SUBSCRIBE_COMPLETE_STATE`：
   - `SUBSCRIBING`

5. `context->Is_Connected == false`：
   - `DISCONNECTED` 或 `CONNECTING`，依 `context->State` 決定

6. `s_mqtt_app_connected == false`：
   - 不能對外視為 `READY`
   - 如果 client 正在 connect/subscribing，回傳 `CONNECTING`/`SUBSCRIBING`
   - 否則回傳 `DISCONNECTED`

7. broker snapshot unavailable：
   - 不應直接當 `DEAD`
   - 若 app + ctx connected 且 state is yield，映射為 `READY`，但 log 必須標明 `broker_ok=0`
   - 不新增 `READY_WITHOUT_LIVENESS`，避免 effective enum 膨脹

8. `broker.state == MQTT_BROKER_LIVENESS_READY`：
   - `READY`

9. `broker.state == MQTT_BROKER_LIVENESS_PROBING`：
   - `PROBING`
   - 不 request reconnect

10. `broker.state == MQTT_BROKER_LIVENESS_DEAD` 或 `DISCONNECTED`：
    - `DEAD`

11. stale QoS1 PUBACK：
    - `DEAD`
    - reason 使用 `stale_puback`

## 6. Control Rules

### Publish Enqueue

`Entity_Mqtt_App_Topic_Publish()` 不應只看 `s_mqtt_app_connected`。

它應查 effective state：

- `READY`：接受 enqueue。
- `PROBING`：不直接 reconnect；對 AI token request 可回 busy/not-ready，讓 MQTT task 快速推進。
- `CONNECTING` / `SUBSCRIBING`：不 enqueue，回 not-ready。
- `DISCONNECTED` / `DEAD`：不 enqueue，request reconnect 或等待 MQTT task 既有 reconnect。

### Queue Drain

`Entity_Mqtt_App_Drain_Agent_Queue()` 不應只看 `context->Is_Connected`。

它應查 effective state：

- `READY`：drain queue。
- 其他狀態：不 drain。

這可以避免 queued publish 被送進不健康 socket。

### Reconnect Trigger

只有以下狀態可觸發 reconnect：

- `DISCONNECTED`
- `DEAD`

以下狀態不可觸發 reconnect：

- `CONNECTING`
- `SUBSCRIBING`
- `PROBING`
- `READY`

尤其 `PROBING` 不可直接 reconnect。它代表 MQTT 正在等待 PINGRESP，不是已經斷線。

### AI Publish Gate

AI token request 的目標是低延遲：

- `READY`：立即 publish。
- `PROBING`：不重連、不長等；返回 not-ready/busy，或讓 MQTT task 先跑短切片後再判斷。
- `DEAD`：立即 request reconnect。
- `CONNECTING` / `SUBSCRIBING`：等待現有流程，不額外打斷。

W22 不應引入「不是 READY 就等很久」的策略。

## 7. Logging Contract

低頻 log 應同時打印 raw fields 和 effective state：

```text
effective=READY raw_ctx_connected=1 raw_app_connected=1 broker=READY
effective=PROBING raw_ctx_connected=1 raw_app_connected=1 broker=PROBING wait_ping=1 pingreq_age=...
effective=DEAD raw_ctx_connected=1 raw_app_connected=1 broker=DEAD reason=pingresp_timeout
```

必要 marker：

- `[MQTT_EFFECTIVE_STATE]`
- `[MQTT_PUBLISH_GATE]`
- `[MQTT_DRAIN_GATE]`
- `[MQTT_RECONNECT_DECISION]`

這些 marker 只能低頻打印，不能在 hot path 高頻刷屏。

## 8. Validation

Host/static tests 應覆蓋：

1. `READY` 允許 publish enqueue。
2. `READY` 允許 queue drain。
3. `PROBING` 不觸發 reconnect。
4. `PROBING` 不 drain queue 到 socket。
5. `DEAD` 觸發 reconnect。
6. `DISCONNECTED` 觸發 reconnect。
7. `CONNECTING` 不重複 reconnect。
8. `SUBSCRIBING` 不 publish、不重複 reconnect。
9. stale QoS1 PUBACK 轉成 `DEAD`。
10. log snapshot 同時包含 raw fields 和 effective state。

Firmware runtime validation：

1. Beken R1：首次 AI 對話成功。
2. Beken R1：連續兩次 AI start/stop 成功。
3. Beken SpeedTech Wi-Fi：AI request latency 不比修正前更差。
4. ESP32S3 ai_alarm：AI 對話正常。
5. sleep/wakeup 後 MQTT reconnect 只在 `DEAD` 或 `DISCONNECTED` 發生。
6. 不再看到 `broker=PROBING` 直接造成 reconnect。

## 9. Expected Impact

正面影響：

- 減少正常連線被 `PROBING` 誤重連。
- 減少死 socket publish 後等待 AI token timeout。
- 讓 publish/drain/reconnect 判斷一致。
- 降低二次開發者修改 MQTT gate 時的維護風險。

風險：

- 若 effective state 規則寫錯，會影響所有 SDK MQTT publish 使用者。
- 若 `PROBING` 處理太保守，AI request 可能短暫返回 busy。
- 需要三平台回歸：Beken R1、Beken SpeedTech、ESP32S3 ai_alarm。

風險控制：

- 先加 host tests，再改邏輯。
- `READY` 不加任何額外等待。
- `PROBING` 不重連、不長等。
- `DEAD` 才重連。
- 保留 raw diagnostic fields，便於定位真實網路問題。

## 10. Recommended Implementation Order

1. 在 host test 裡建立 effective state matrix。
2. 在 client 層新增 effective state enum 和 snapshot function。
3. 讓 app 層 publish gate 改用 effective state。
4. 讓 drain queue 改用 effective state。
5. 讓 reconnect decision 改用 effective state。
6. 更新低頻 log marker。
7. 跑 SDK host tests。
8. 同步產品 SDK baseline。
9. 跑 Beken R1 / SpeedTech / ESP32S3 runtime。
