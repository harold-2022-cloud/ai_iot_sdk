# Phase 7 RTC W2b Agora Backend Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the real private Agora RTC backend behind the W2a facade core so second products use the SDK-owned facade for token-to-join, audio frame transport, raw datastream transport, stop/status, and callback relay without including Agora, BK, ESP-IDF, FreeRTOS, or product headers.

**Architecture:** W2b keeps the public facade header chip-neutral and adds a private `ai_rtc_facade_agora_backend.c` that includes `agora_rtc_api.h` only in the implementation. The W2a core remains the owner of public state, callback gates, and lifecycle semantics; the Agora backend only translates backend vtable calls into Agora SDK calls and translates Agora callbacks back into private facade events. Host tests compile the backend against a small `agora_rtc_api.h` stub so call order and mapping are locked without requiring firmware.

**Tech Stack:** C11, Agora RTC SDK C API, host stubs, GNU Make, Python static boundary checks.

---

## Scope

W2b does:

- keep W2a core/fake backend host tests passing;
- add `Ai_Rtc_Facade_Send_Datastream()` to the public facade surface;
- add a private Agora backend source file that maps token/session, audio tx/rx, raw datastream tx/rx, renew-token, and stop to Agora RTC APIs;
- compile the Agora backend in host tests against stubs;
- add static guards proving public headers and W2a core stay free of runtime SDK/product headers.

W2b does not:

- rewire BK product code to use the facade;
- flash firmware;
- move board audio, UX, DP, LED, key, prompt, timeout, or datastream JSON policy into the facade;
- expose `agora_rtc_api.h`, `connection_id_t`, `audio_frame_info_t`, or `video_frame_info_t` in public headers;
- implement video transport beyond the existing unsupported shell.

## Files

- Modify: `media/rtc_facade/include/ai_rtc_facade.h`
- Create: `media/rtc_facade/src/ai_rtc_facade_backend.h`
- Create: `media/rtc_facade/src/ai_rtc_facade.c`
- Create: `media/rtc_facade/src/ai_rtc_facade_agora_backend.c`
- Create: `test/host/fakes/ai_rtc_facade_fake_backend.h`
- Create: `test/host/fakes/ai_rtc_facade_fake_backend.c`
- Create: `test/host/stubs/agora_rtc_api.h`
- Create: `test/host/stubs/agora_rtc_stub.c`
- Create: `test/host/test_ai_rtc_facade_core.c`
- Create: `test/host/test_ai_rtc_facade_agora_backend.c`
- Create: `test/host/check_ai_rtc_facade_core_boundary.py`
- Modify: `test/host/check_ai_rtc_facade_public_header.py`
- Modify: `test/host/Makefile`

## Tasks

### Task 1: Add W2a core and fake backend

- [ ] Add the private backend vtable in `media/rtc_facade/src/ai_rtc_facade_backend.h`.
- [ ] Implement `media/rtc_facade/src/ai_rtc_facade.c` as the only public API implementation.
- [ ] Implement the fake backend under `test/host/fakes/`.
- [ ] Add `test/host/test_ai_rtc_facade_core.c` covering init validation, token-to-join success/failure, stop idempotency, wait behavior, audio send gates, datastream send gates, rx relay, raw datastream relay, and unsupported video.

### Task 2: Add datastream send public API

- [ ] Add `int Ai_Rtc_Facade_Send_Datastream(const uint8_t *data, size_t len);` to `ai_rtc_facade.h`.
- [ ] Update `check_ai_rtc_facade_public_header.py` required tokens and second-product compile sample.
- [ ] Ensure the public-header guard still rejects Agora/BK/ESP/FreeRTOS/product tokens.

### Task 3: Add real Agora backend

- [ ] Add `media/rtc_facade/src/ai_rtc_facade_agora_backend.c`.
- [ ] Include only `ai_rtc_facade_backend.h` and private `agora_rtc_api.h` plus standard C headers.
- [ ] On start: initialize Agora, create a connection, join the token channel, and store the connection id.
- [ ] On Agora joined callback: create one reliable ordered data stream, then notify the facade core joined.
- [ ] On Agora reconnect/rejoin/user/token/error callbacks: notify the facade core through private backend events.
- [ ] On audio callback: map Agora audio frame type to `Ai_Rtc_Facade_Audio_Frame_t` and relay through the facade core.
- [ ] On stream message callback: relay raw stream id, sender uid, payload pointer/length, and sent timestamp without JSON parsing.
- [ ] On send audio: map facade audio format to Agora `audio_frame_info_t` and call `agora_rtc_send_audio_data()`.
- [ ] On send datastream: call `agora_rtc_send_stream_message()` using the default stream created after join.
- [ ] On stop: leave channel, destroy connection, finish Agora, clear connection and stream state.

### Task 4: Add host Agora stubs and backend tests

- [ ] Add a minimal `test/host/stubs/agora_rtc_api.h` with the types and function prototypes W2b uses.
- [ ] Add `test/host/stubs/agora_rtc_stub.c` to record init/create/join/create-stream/send/leave/destroy/fini/renew calls and trigger callbacks.
- [ ] Add `test/host/test_ai_rtc_facade_agora_backend.c` to verify token-to-join call order, joined event after Agora callback, datastream creation after join, audio mapping, datastream send, raw rx relay, renew-token event, and stop cleanup.

### Task 5: Add boundary and build integration

- [ ] Add `test/host/check_ai_rtc_facade_core_boundary.py` to reject forbidden runtime tokens from W2a core/private backend header and ensure only the real Agora backend includes `agora_rtc_api.h`.
- [ ] Update `test/host/Makefile` with `test_ai_rtc_facade_core`, `test_ai_rtc_facade_agora_backend`, `check_ai_rtc_facade_core_boundary`, and existing allocator/public-header guards.
- [ ] Run `make -C /root/smp/ai_iot_sdk/test/host test`.
- [ ] Run `git -C /root/smp/ai_iot_sdk diff --check`.

## Verification

Expected host output includes:

```text
PASS: entity interface reference template
PASS: ai rtc facade public header
PASS: ai rtc facade core boundary
PASS valid alloc/free
PASS valid realloc
PASS foreign pointer free
PASS double-free
PASS corrupt magic
PASS Bsp_Psram aliases
PASS: ai rtc facade core
PASS: ai rtc facade agora backend
```

After verification, confirm no public or W2a core file contains forbidden tokens:

```bash
rg -n "agora_rtc_api|connection_id_t|audio_frame_info_t|video_frame_info_t|bk_|beken|esp_|freertos|driver/|components/|network_transfer|ntwk_trans|product_app" /root/smp/ai_iot_sdk/media/rtc_facade/include /root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade.c /root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade_backend.h
```

Expected: no matches.
