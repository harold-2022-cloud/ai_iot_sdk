# portable-SDK Baseline (P0 冻结)

## 基线 SDK SHA

**f3788c20** (完整: `f3788c2095b3aa1307b67141886b94f1adab344d`)

确认于 2026-06-29 运行 `git -C /mnt/c/Users/harold.chen/esp32s3_ai_alarm/sdk rev-parse HEAD`

---

## 可移植性棘轮当前态

运行 `bash test/rino_portability_guard.sh` 输出：
```
✅ PASS：无新增平台耦合（allowlist 内尚有 8 个已知债务文件待清零）
```

**allowlist 计数：8**

### 8 个已知债务文件清单

| 序号 | 文件 | 清零计划 |
|------|------|---------|
| 1 | `library/mi_mqtt/mi_mqtt_client.c` | P3 清 |
| 2 | `library/mi_mqtt/mi_mqtt_client.h` | P3 清 |
| 3 | `entity_iot_sdk/entity_app/entity_config_net.c` | P4 清 |
| 4 | `entity_iot_sdk/entity_app/entity_os_system.c` | P2 清 |
| 5 | `entity_iot_sdk/entity_http/entity_http_client.c` | P2 清 |
| 6 | `entity_iot_sdk/entity_iot/entity_iot_func.h` | 永久豁免 (`#ifdef ESP_PLATFORM` 条件包 `esp_attr.h`) |
| 7 | `entity_iot_sdk/entity_mqtt/entity_mqtt_app.c` | P2 清 |
| 8 | `entity_iot_sdk/entity_mqtt/entity_mqtt_event_report.c` | P2 清 |

---

## L1 验收口径（控制面可移植）

P0–P1 阶段验收范围（控制面网络功能）：

- 配网（WiFi provisioning）
- WiFi 连接（station 模式）
- MQTT 连接/订阅/发布
- 设备绑定（device registration）
- RTC token 请求-响应（同步获取）

**验收方法**：通过 `test/rino_portability_guard.sh` allowlist 增长曲线（8→4→2→1→0）和集成测试验证。

---

## L2 验收口径（媒体面 RTC/audio/AEC）

**范围外** — 不在 P0–P4 portable-SDK 重构范围内。媒体栈（RTC、音频引擎、AEC）维持原有 esp32s3_ai_alarm 专项实现，与 portable-SDK 隔离。

---

## Phase 7 RTC Vendor/AOSL Baseline

确认于 2026-07-08。此段记录 Phase 7 之后的 RTC media facade baseline；它是 P0 旧基线之后新增的媒体面 SDK 状态，不修改上方 P0–P4 历史记录。

### 分层原则

- SDK owns RTC audio/video/datastream/private RTM backend.
- Product owns microphone, speaker, codec, AEC, DMA, LCD, AVI, button, BLE/Wi-Fi, HTTP/MQTT.
- `media/rtc_facade/include/ai_rtc_facade.h` 不暴露 Agora vendor header、AOSL、DMA/resource/LCD/AVI/BLE/Wi-Fi/HTTP/MQTT、RTM/datastream selector。
- Datastream 是 Phase 7 public control path；RTM 只作为 SDK private Agora backend option，不进入 public facade API。

### ESP32S3 RTC vendor baseline

SDK private vendor component:

```text
chip_esp32s3/vendor/agora_iot_sdk/CMakeLists.txt
chip_esp32s3/vendor/agora_iot_sdk/include/agora_rtc_api.h
chip_esp32s3/vendor/agora_iot_sdk/include/autoconfig.h
chip_esp32s3/vendor/agora_iot_sdk/libs/libagora-rtc-sdk.a
chip_esp32s3/vendor/agora_iot_sdk/libs/libaosl.a
```

ESP32S3 product repos should consume RTC through the SDK facade and SDK vendor component. Product code must not restore `ai_components/agora_iot_sdk` or directly include `agora_rtc_api.h`.

### BK7258 RTC/AOSL baseline

BK7258 AOSL baseline archive tracked by SDK:

```text
chip_bk7258/vendor/aosl/libs/libaosl.a
```

BK7258 Agora RTC vendor component tracked by SDK:

```text
chip_bk7258/vendor/ai_iot_bk7258_agora_iot_sdk/include/bk7258/agora_rtc_api.h
chip_bk7258/vendor/ai_iot_bk7258_agora_iot_sdk/bk7258/libs/librtsa.a
chip_bk7258/vendor/ai_iot_bk7258_agora_iot_sdk/bk7258/libs/libagora-cjson.a
chip_bk7258/vendor/ai_iot_bk7258_agora_iot_sdk/hal/aosl/
```

Beken product integration may still receive the Agora RTC archive/header from the Beken AVDK third-party component when that is required by the Beken build system:

```text
bk_avdk_smp/ap/components/bk_thirdparty/agora-iot-sdk/
```

The SDK-side BK7258 AOSL archive is the accepted baseline for the newer Agora RTC/AOSL integration work. Keep the Agora archive/header, AOSL archive, and Beken SDK mbedtls/trustengine libraries as one matched baseline.

### Link conflict baseline rules

- Do not mix an Agora RTC archive from one baseline with an AOSL archive from another baseline.
- Only one linked library set should provide platform crypto/random symbols such as `rand_bytes`.
- If Beken link fails with `multiple definition of rand_bytes`, classify it as an Agora/AOSL/mbedtls baseline mismatch first, not as an RTC facade API issue.
- Do not fix link conflicts by exposing AOSL, mbedtls, or Agora vendor APIs through `ai_rtc_facade.h`.
- Update this section when replacing `libagora-rtc-sdk.a`, `agora_rtc_api.h`, `autoconfig.h`, or `libaosl.a`.

---

## allowlist 清零进度表

| 阶段 | 清零文件 | 目标 allowlist | 说明 |
|------|---------|----------------|------|
| P0 (冻结基线) | — | 8 | 基线快照，无代码改动 |
| P2 | entity_os_system, entity_http_client, entity_mqtt_app, entity_mqtt_event_report | 4 | 网络/OS 依赖分离 |
| P3 | mi_mqtt_client.c, mi_mqtt_client.h | 2 | MQTT 客户端通用化 |
| P4 | entity_config_net.c | 1 | 配网路由适配 |
| — | entity_iot_func.h | 1 (永久) | 条件编译豁免，保持不变 |

---

## 验证命令

```bash
# 确认基线 SHA
git -C /mnt/c/Users/harold.chen/esp32s3_ai_alarm/sdk rev-parse HEAD
# 应输出：f3788c2095b3aa1307b67141886b94f1adab344d

# 验收 allowlist 当前态
bash test/rino_portability_guard.sh
# 应输出：✅ PASS: allowlist 内尚有 8 个已知债务文件待清零
```

---

冻结于：2026-06-29
