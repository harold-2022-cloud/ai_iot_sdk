# W22b MQTT AI Deferred Publish Runtime Note

Date: 2026-07-18

## Scope

W22b moves the ESP32S3 temporary AI token retry idea back into the SDK MQTT layer for the narrow pre-publish case:

- Product calls AI token request once.
- If MQTT effective state is `PROBING` before publish, SDK stores one AI token publish in a private deferred slot.
- When MQTT effective state returns `READY`, SDK drains the deferred publish from the MQTT task.
- `DEAD` or disconnected state does not publish; the deferred request is dropped and token pending is cleared.

This does not change RTC facade public API and does not change RTC audio/video/datastream behavior.

## Code Boundary

SDK-owned behavior:

- `entity_iot_sdk/entity_mqtt/entity_mqtt_app.c`
  - `Entity_Mqtt_App_Prepare_Ai_Publish_Result()`
  - `Entity_Mqtt_App_Topic_Publish()`
  - SDK private AI deferred publish slot

Product-owned behavior:

- Button/UI/session policy
- Post-publish token timeout handling
- Post-publish MQTT reconnect recovery
- ESP32S3 `mqtt_reconnect_resend` remains a product recovery path and is not W22b.

## ESP32S3 Runtime Observation

Runtime log:

- `/tmp/esp32s3_ai_alarm_.txt`

Observed result:

- AI dialog started successfully.
- MQTT was `READY` when the AI token request was made.
- Token request was published once.
- RTC joined successfully.
- Remote AI user joined.
- Audio TX gate opened after remote user joined.
- Mic uplink frames were sent with no TX failures.

Key log facts:

- `stage=ai_publish_ready effective=READY`
- `key_to_first_publish_ms=308`
- `last_publish_to_token_rx_ms=1967`
- `key_to_join_success_ms=6483`
- `publish_count=1`
- `[RTC_FACADE_TX_GATE] allowed=1 reason=remote_user_joined`
- `fail_frames=0`
- `no_tx_func=0`
- `ring_drop_frames=0`

No firmware panic was found in the log. The final `Error reading from serial device` is a monitor/serial-side disconnect unless accompanied by firmware panic markers such as `Guru Meditation`, `assert failed`, or `Backtrace`.

## What Was Not Runtime-Validated

This log did not validate the W22b deferred path because the AI key was not pressed while MQTT was in `PROBING`.

Expected W22b deferred runtime markers, not seen in this log:

- `[MQTT_AI_DEFERRED][ACCEPT_PROBING]`
- `[MQTT_AI_DEFERRED][STORE]`
- `[MQTT_AI_DEFERRED][DRAIN_READY]`

Therefore the correct runtime status is:

- ESP32S3 normal `READY` publish path: accepted.
- W22b `PROBING -> READY` deferred publish path: host/static accepted, runtime not exercised.

## Post-Publish Reconnect Is Separate

`mqtt_reconnect_resend` is a product-side post-publish recovery path. It handles a different case:

- AI token request was already published.
- MQTT disconnects before token response is received.
- Product may resend after reconnect to avoid waiting for the full token timeout window.

This is not the W22b problem. W22b only handles the pre-publish `PROBING` window.

Current decision:

- Do not remove `mqtt_reconnect_resend` as part of W22b.
- Do not add more SDK state for post-publish reconnect in this closure.
- Record it as a possible future W22c topic only if runtime logs prove duplicate publish causes worse behavior than waiting for timeout.

## Verification

SDK host/static verification already passed before sync:

```sh
make -C /mnt/c/Users/harold.chen/ai_iot_sdk/test/host test
make -C /mnt/c/Users/harold.chen/ai_iot_sdk/test/host clean
```

Product SDK copies also passed W22b guards:

```sh
python3 sdk/test/host/check_entity_mqtt_ai_deferred_publish.py
python3 sdk/test/host/check_entity_mqtt_effective_state_policy.py
```

