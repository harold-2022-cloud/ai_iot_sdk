# RTC Best Practices Context Prompt

Use this context when continuing RTC SDK migration, debugging, or porting work.

```text
You are working on the AI IoT SDK RTC facade.

Canonical SDK repo:
- /root/smp/ai_iot_sdk

Primary RTC SDK files:
- media/rtc_facade/include/ai_rtc_facade.h
- media/rtc_facade/src/ai_rtc_facade.c
- media/rtc_facade/src/ai_rtc_facade_backend.h
- media/rtc_facade/src/ai_rtc_facade_agora_backend.c
- media/rtc_facade/src/agora/ai_rtc_agora_service.c
- media/rtc_facade/src/agora/ai_rtc_agora_datastream.c
- media/rtc_facade/src/agora/ai_rtc_agora_datastream.h
- media/rtc_facade/src/agora/ai_rtc_agora_port.h

The SDK public RTC facade owns:
- token-to-join lifecycle
- start/join/stop/deinit state transitions
- joined/reconnecting/rejoined/remote-user/token/failed events
- audio TX/RX API shape
- video TX/RX API shape
- raw datastream TX/RX API shape
- backend-independent error return values

The SDK private Agora backend owns:
- Agora RTC init/fini lifecycle
- Agora connection create/join/leave/destroy
- Agora event handler mapping into facade events
- Agora audio/video frame type mapping
- Agora datastream create/send/on_stream_message mapping
- SDK-private datastream envelope parser
- stale callback rejection after stop/restart
- vendor SDK header/binary boundary

Product/platform code owns:
- codec, speaker, microphone, PA, I2S, DMA, ADC, buttons, LCD, LVGL
- wake word, prompt audio, UI overlay, business state, BLE/Wi-Fi UX
- heap/DMA thresholds and startup ordering for its board
- product interpretation of raw datastream messages
- queueing work out of RTC callbacks into product tasks

Current Phase 7 transport decision:
- Datastream is the accepted AI control-message transport.
- Public ai_rtc_facade.h must expose only raw datastream send/RX:
  Ai_Rtc_Facade_Send_Datastream() and on_datastream_rx.
- Do not add public RTM APIs.
- Do not add public CONTROL_TRANSPORT selector APIs.
- RTM is future private backend work only.
- RTM and datastream are not expected to coexist in the same AI scenario.
- ESP32S3 vendor Agora SDK may contain RTM-capable headers/libs
  (`agora_rtc_login_rtm`, `agora_rtc_logout_rtm`,
  `agora_rtc_send_rtm_data`). These symbols belong to the SDK private backend
  layer only and must not leak into `ai_rtc_facade.h`.
- The ESP32S3 vendor baseline may use the existing SDK `libaosl.a` packaging
  boundary. Do not import AOSL source unless that is explicitly planned as a
  separate vendor dependency task.

Required state-machine expectations:
- Token result starts RTC join only when facade is initialized and idle/failed/stopped.
- JOINED means local RTC channel join success.
- Audio uplink must not send until remote AI user joined.
- Reconnect clears joined/remote-user readiness until rejoin/user-joined callbacks arrive.
- Rejoin must not immediately send audio; wait for remote user joined again.
- Datastream send requires facade joined and backend stream id readiness.
- Stop is idempotent and must release connection-level session resources.
- Stop must not call Agora SDK fini per dialog.
- Deinit/shutdown may release the whole Agora SDK.
- Stale callbacks from old connection IDs must not reach product callbacks.

Callback safety rules:
- RTC callbacks may run from backend/vendor context.
- Product callbacks must be short and non-blocking.
- Product code must copy RX frame/datastream pointers if needed after callback return.
- Product code must not free SDK-owned RX buffers.
- Product code must not call Agora vendor APIs directly.
- Product code should enqueue UI/audio/business work to product tasks.
- Avoid heavy JSON parsing, LCD work, audio playback, stop/deinit, storage, or network operations inside deep RTC callback stacks.

Datastream rules:
- SDK relays raw Ai_Rtc_Facade_Datastream_Message_t through on_datastream_rx.
- SDK private parser may parse the shared Agora datastream envelope for validation/reusable support.
- Product event semantics remain product-owned.
- Do not move product UI/dialog policy into ai_rtc_facade.h.
- Do not parse datastream directly inside Agora on_stream_message unless stack/reentrancy has been explicitly designed and tested.

Cross-chip portability rules:
- ai_rtc_facade.h must not include Agora, ESP-IDF, Beken, FreeRTOS, driver, product, LCD, codec, DMA, or network_transfer headers.
- Product repos consume the SDK facade, not vendor Agora APIs.
- Backend/platform ports may adapt vendor SDK details behind private SDK files.
- Board resources are configured by the product/platform layer, not by public RTC facade API.

Debugging discipline:
- Do not guess. Start from logs, state transitions, and function call graph.
- For AI voice failures, first identify the exact stuck point:
  token result, start request, Agora init, create connection, join channel,
  local joined, datastream created, remote user joined, first audio TX,
  audio RX, stop/restart, or crash.
- For crashes, classify with addr2line/backtrace before changing behavior.
- For memory issues, separate:
  internal heap free, DMA-capable largest block, PSRAM free, task stack high-water,
  double free, stale callback, and callback reentrancy.
- Do not change LCD/audio/DMA numeric thresholds without proving why the existing
  product timing no longer matches the legacy contract.
- If a product worked before SDK migration, compare legacy RTC call order and SDK
  call order before changing unrelated subsystems.

Recommended verification:
- SDK host/static:
  make -C /root/smp/ai_iot_sdk/test/host test
  make -C /root/smp/ai_iot_sdk/test/host clean
- Public header boundary:
  python3 /root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_public_header.py
- SDK Agora migration boundary:
  python3 /root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_sdk_agora_migration.py
- Product runtime validation must confirm:
  token acquired, RTC start requested, local joined, remote AI user joined,
  audio TX starts after remote user joined, audio RX arrives, datastream RX/TX
  works if required, repeated stop/start works, no stale callback reaches product.

When making changes:
- Prefer host/static tests before runtime changes.
- Keep public API small and chip-neutral.
- Keep Agora-specific details private.
- Keep product peripheral timing product-owned.
- Record accepted/deferred validation explicitly in docs.
```

## Purpose

This prompt exists to prevent future RTC SDK work from mixing three different
layers:

- SDK public RTC facade
- SDK private Agora backend
- Product/platform peripheral policy

The expected result is a portable RTC SDK that can be consumed by ESP32S3,
BK7258, xiaozhi-esp32, and future chips without moving board-specific LCD,
audio, DMA, or button policy into the public RTC API.
