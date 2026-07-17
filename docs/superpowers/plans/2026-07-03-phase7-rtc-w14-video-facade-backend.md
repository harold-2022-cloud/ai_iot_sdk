# Phase 7 RTC W14 Video Facade Backend Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement SDK-owned RTC video TX/RX backend support behind `ai_rtc_facade.h` while preserving the existing audio/datastream behavior and keeping product UI/audio startup timing unchanged.

**Architecture:** Keep `ai_rtc_facade.h` as the product-facing RTC API and keep Agora-specific types private to `media/rtc_facade/src/agora`. The facade core adds a backend video vtable path, the Agora service maps `Ai_Rtc_Facade_Video_Frame_t` to Agora `video_frame_info_t`, and received Agora video frames are relayed through the existing `on_video_rx` callback. Bandwidth and keyframe callbacks are recorded as SDK-private backend events first; they are not exposed in the public API until a product encoder contract is defined.

**Tech Stack:** C11, Agora RTSA C API, SDK RTC facade, SDK private Agora backend, Armino/ESP-IDF component build, host C tests, Python static guards, GNU Make host test harness.

---

## Current Evidence

- `rtc.txt` describes video as a first-class RTC path: app passes video frames to the SDK, SDK packetizes/FEC/encrypts/paces/sends, and received video frames are assembled then delivered to app callbacks.
- `rtc.txt` says Agora callback data may live in internal ring buffers; public facade docs must state RX frame pointers are only valid during the callback.
- Vendor Agora API already provides:
  - `video_frame_info_t`
  - `on_video_data`
  - `on_target_bitrate_changed`
  - `on_key_frame_gen_req`
  - `agora_rtc_send_video_data`
- `/root/agora_rtsa_sdk/example/hello_rtsa/hello_rtsa.c` sets `video_frame_info_t.frame_type`, `frame_rate`, `data_type`, and `stream_type` before calling `agora_rtc_send_video_data`.
- `/root/agora_rtsa_sdk/example/hello_rtsa/hello_rtsa.c` gates media send on channel connected state, not remote user joined; W14 video TX must follow channel-joined gating while audio TX keeps the stricter AI remote-user gate.
- `/root/agora_rtsa_sdk/example/hello_stream_message/hello_stream_message.c` creates the data stream after `agora_rtc_join_channel` and sends stream messages only after the connected flag becomes true; the existing SDK datastream flow already matches that shape.
- Current SDK public header already has `Ai_Rtc_Facade_Video_Frame_t`, `Ai_Rtc_Facade_Video_Rx_Cb`, `enable_video`, and `Ai_Rtc_Facade_Send_Video`.
- Current SDK implementation returns `AI_RTC_FACADE_ERR_UNSUPPORTED` from `Ai_Rtc_Facade_Send_Video`.
- Current Beken product bridge initializes `enable_video=false`; W14 must not turn on video in the product runtime path.

## File Structure

- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/include/ai_rtc_facade.h`
  - Add `frame_rate_hz` to `Ai_Rtc_Facade_Video_Frame_t`; Agora examples pass frame rate into `video_frame_info_t`.
  - Remove the "unsupported shell" wording for `Ai_Rtc_Facade_Send_Video`.
  - Document TX frame ownership and RX callback data lifetime.
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade_backend.h`
  - Add private backend `send_video`.
  - Add private backend notify function for video RX.
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade.c`
  - Route `Ai_Rtc_Facade_Send_Video` through backend after init/join/frame validation.
  - Dispatch video RX callbacks through `Ai_Rtc_Facade_Backend_Notify_Video_Rx`.
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade_agora_backend.c`
  - Add backend vtable entry for `send_video`.
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.h`
  - Add `Ai_Rtc_Agora_Service_Send_Video`.
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.c`
  - Map facade video formats to Agora video data/frame types.
  - Register `on_video_data`, `on_target_bitrate_changed`, and `on_key_frame_gen_req`.
  - Send video via `agora_rtc_send_video_data`.
- Modify: `/root/smp/ai_iot_sdk/test/host/stubs/agora_rtc_api.h`
  - Add video enums, `video_frame_info_t`, video callbacks, send prototype, and stub state fields.
- Modify: `/root/smp/ai_iot_sdk/test/host/stubs/agora_rtc_stub.c`
  - Implement `agora_rtc_send_video_data`.
  - Add video event emit helpers.
- Modify: `/root/smp/ai_iot_sdk/test/host/fakes/ai_rtc_facade_fake_backend.h`
  - Track fake video send calls and last frame.
- Modify: `/root/smp/ai_iot_sdk/test/host/fakes/ai_rtc_facade_fake_backend.c`
  - Implement fake backend `send_video`.
- Modify: `/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_core.c`
  - Add facade core test for video TX routing and video RX callback dispatch.
- Modify: `/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend.c`
  - Add Agora backend test for video TX/RX mapping.
- Modify: `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_public_header.py`
  - Change expectation from "video send compiles as unsupported shell" to "video send compiles as stable API".
- Modify: `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_sdk_agora_migration.py`
  - Require SDK Agora service to own video send and video callbacks.
- Modify: `/root/smp/bk_solution_ai/components/network_transfer/network_transfer.c`
  - Replace `ntwk_trans_agora_facade_video_send` unsupported stub with a facade video frame mapping.
  - Keep product `enable_video=false`; this bridge only makes existing compatibility API no longer hard-coded unsupported.

## Task 1: Facade Core Video Vtable

**Files:**
- Modify: `/root/smp/ai_iot_sdk/test/host/fakes/ai_rtc_facade_fake_backend.h`
- Modify: `/root/smp/ai_iot_sdk/test/host/fakes/ai_rtc_facade_fake_backend.c`
- Modify: `/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_core.c`
- Modify: `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_public_header.py`
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade_backend.h`
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade.c`
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/include/ai_rtc_facade.h`

- [ ] **Step 1: Add frame rate to the public video frame**

In `/root/smp/ai_iot_sdk/media/rtc_facade/include/ai_rtc_facade.h`, change:

```c
typedef struct
{
    const uint8_t *data;
    size_t len;
    Ai_Rtc_Facade_Video_Format_t format;
    uint32_t width;
    uint32_t height;
    uint64_t timestamp_ms;
    uint32_t flags;
} Ai_Rtc_Facade_Video_Frame_t;
```

to:

```c
typedef struct
{
    const uint8_t *data;
    size_t len;
    Ai_Rtc_Facade_Video_Format_t format;
    uint32_t width;
    uint32_t height;
    uint32_t frame_rate_hz;
    uint64_t timestamp_ms;
    uint32_t flags;
} Ai_Rtc_Facade_Video_Frame_t;
```

In `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_public_header.py`, update the compile sample `Ai_Rtc_Facade_Video_Frame_t video` initializer to include:

```c
.frame_rate_hz = 15,
```

- [ ] **Step 2: Extend the fake backend state**

In `/root/smp/ai_iot_sdk/test/host/fakes/ai_rtc_facade_fake_backend.h`, add these fields to `Ai_Rtc_Facade_Fake_Backend_State_t`:

```c
int send_video_result;
int send_video_calls;
Ai_Rtc_Facade_Video_Frame_t last_video;
```

- [ ] **Step 3: Add fake backend video send**

In `/root/smp/ai_iot_sdk/test/host/fakes/ai_rtc_facade_fake_backend.c`, add:

```c
static int fake_send_video(const Ai_Rtc_Facade_Video_Frame_t *frame)
{
    s_fake.send_video_calls++;
    if (frame != NULL)
    {
        s_fake.last_video = *frame;
    }
    return s_fake.send_video_result;
}
```

Add the vtable entry:

```c
.send_video = fake_send_video,
```

In `Ai_Rtc_Facade_Fake_Backend_Reset`, initialize:

```c
s_fake.send_video_result = AI_RTC_FACADE_OK;
```

- [ ] **Step 4: Add the failing facade core video TX test**

In `/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_core.c`, replace the existing unsupported assertion:

```c
CHECK(Ai_Rtc_Facade_Send_Video(NULL) == AI_RTC_FACADE_ERR_UNSUPPORTED);
```

with:

```c
uint8_t video_data[5] = {1, 2, 3, 4, 5};
const Ai_Rtc_Facade_Video_Frame_t video = {
    .data = video_data,
    .len = sizeof(video_data),
    .format = AI_RTC_FACADE_VIDEO_FORMAT_H264,
    .width = 640,
    .height = 360,
    .frame_rate_hz = 15,
    .timestamp_ms = 10,
    .flags = AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME,
};

CHECK(Ai_Rtc_Facade_Send_Video(&video) == AI_RTC_FACADE_OK);
CHECK(Ai_Rtc_Facade_Fake_Backend_State()->send_video_calls == 1);
CHECK(Ai_Rtc_Facade_Fake_Backend_State()->last_video.data == video_data);
CHECK(Ai_Rtc_Facade_Fake_Backend_State()->last_video.len == sizeof(video_data));
CHECK(Ai_Rtc_Facade_Fake_Backend_State()->last_video.format == AI_RTC_FACADE_VIDEO_FORMAT_H264);
CHECK(Ai_Rtc_Facade_Fake_Backend_State()->last_video.frame_rate_hz == 15);
CHECK(Ai_Rtc_Facade_Fake_Backend_State()->last_video.flags == AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME);
```

- [ ] **Step 5: Run the core host test and confirm it fails**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test_ai_rtc_facade_core
/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_core
```

Expected before implementation:

```text
FAIL ... Ai_Rtc_Facade_Send_Video(&video) == AI_RTC_FACADE_OK
```

- [ ] **Step 6: Add video to the private backend vtable**

In `/root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade_backend.h`, add:

```c
int (*send_video)(const Ai_Rtc_Facade_Video_Frame_t *frame);
```

after `send_datastream`, and add:

```c
void Ai_Rtc_Facade_Backend_Notify_Video_Rx(const Ai_Rtc_Facade_Video_Frame_t *frame);
```

after `Ai_Rtc_Facade_Backend_Notify_Audio_Rx`.

- [ ] **Step 7: Implement facade video routing**

In `/root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade.c`, replace `Ai_Rtc_Facade_Send_Video` with:

```c
int Ai_Rtc_Facade_Send_Video(const Ai_Rtc_Facade_Video_Frame_t *frame)
{
    const Ai_Rtc_Facade_Backend_t *backend;

    if (!s_ctx.initialized || frame == NULL || frame->data == NULL || frame->len == 0u)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }
    if (!s_ctx.config.enable_video || !s_ctx.joined)
    {
        return AI_RTC_FACADE_ERR_NOT_READY;
    }

    backend = Ai_Rtc_Facade_Get_Backend();
    if (backend == NULL || backend->send_video == NULL)
    {
        return AI_RTC_FACADE_ERR_INTERNAL;
    }
    return backend->send_video(frame);
}
```

Add this function near the audio/datastream notify functions:

```c
void Ai_Rtc_Facade_Backend_Notify_Video_Rx(const Ai_Rtc_Facade_Video_Frame_t *frame)
{
    if (s_ctx.joined && frame != NULL && s_ctx.callbacks.on_video_rx != NULL)
    {
        (void)s_ctx.callbacks.on_video_rx(frame, s_ctx.callbacks.user);
    }
}
```

- [ ] **Step 8: Update public header callback lifetime comments**

In `/root/smp/ai_iot_sdk/media/rtc_facade/include/ai_rtc_facade.h`, replace:

```c
/* TX frame data remains caller-owned and only needs to stay valid for the call duration. */
int Ai_Rtc_Facade_Send_Audio(const Ai_Rtc_Facade_Audio_Frame_t *frame);
int Ai_Rtc_Facade_Send_Datastream(const uint8_t *data, size_t len);

/* Video is a stable API shell in this phase; implementations return AI_RTC_FACADE_ERR_UNSUPPORTED. */
int Ai_Rtc_Facade_Send_Video(const Ai_Rtc_Facade_Video_Frame_t *frame);
```

with:

```c
/* TX frame/message data remains caller-owned and only needs to stay valid for the call duration. */
int Ai_Rtc_Facade_Send_Audio(const Ai_Rtc_Facade_Audio_Frame_t *frame);
int Ai_Rtc_Facade_Send_Datastream(const uint8_t *data, size_t len);
int Ai_Rtc_Facade_Send_Video(const Ai_Rtc_Facade_Video_Frame_t *frame);

/* RX callback frame/message data is valid only during the callback. Copy it before returning if it is needed later. */
```

- [ ] **Step 9: Run the core host test and confirm it passes**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test_ai_rtc_facade_core
/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_core
```

Expected after implementation:

```text
PASS: ai rtc facade core
```

- [ ] **Step 10: Commit Task 1**

Run:

```bash
git -C /root/smp/ai_iot_sdk add media/rtc_facade/include/ai_rtc_facade.h media/rtc_facade/src/ai_rtc_facade.c media/rtc_facade/src/ai_rtc_facade_backend.h test/host/fakes/ai_rtc_facade_fake_backend.c test/host/fakes/ai_rtc_facade_fake_backend.h test/host/test_ai_rtc_facade_core.c test/host/check_ai_rtc_facade_public_header.py
git -C /root/smp/ai_iot_sdk commit -m "feat(rtc): route facade video through backend"
```

## Task 2: Agora Host Stub Video Support

**Files:**
- Modify: `/root/smp/ai_iot_sdk/test/host/stubs/agora_rtc_api.h`
- Modify: `/root/smp/ai_iot_sdk/test/host/stubs/agora_rtc_stub.c`

- [ ] **Step 1: Add video types to the host Agora API stub**

In `/root/smp/ai_iot_sdk/test/host/stubs/agora_rtc_api.h`, add after `audio_codec_type_e`:

```c
typedef enum
{
    VIDEO_DATA_TYPE_H264 = 2,
    VIDEO_DATA_TYPE_GENERIC_JPEG = 20,
} video_data_type_e;

typedef enum
{
    VIDEO_FRAME_AUTO_DETECT = 0,
    VIDEO_FRAME_KEY = 3,
    VIDEO_FRAME_DELTA = 4,
} video_frame_type_e;

typedef enum
{
    VIDEO_STREAM_HIGH = 0,
    VIDEO_STREAM_LOW = 1,
} video_stream_type_e;

typedef enum
{
    VIDEO_ORIENTATION_0 = 0,
} video_orientation_e;

typedef uint16_t video_frame_rate_e;

typedef struct
{
    video_data_type_e data_type;
    video_stream_type_e stream_type;
    video_frame_type_e frame_type;
    video_frame_rate_e frame_rate;
    video_orientation_e rotation;
} video_frame_info_t;
```

- [ ] **Step 2: Add video callbacks and send prototype**

In `agora_rtc_event_handler_t`, add after `on_audio_data`:

```c
void (*on_video_data)(connection_id_t conn_id,
                      uint32_t uid,
                      uint16_t sent_ts,
                      const void *data_ptr,
                      size_t data_len,
                      const video_frame_info_t *info_ptr);
void (*on_target_bitrate_changed)(connection_id_t conn_id, uint32_t target_bps);
void (*on_key_frame_gen_req)(connection_id_t conn_id, uint32_t uid, video_stream_type_e stream_type);
```

Add after `agora_rtc_send_audio_data`:

```c
int agora_rtc_send_video_data(connection_id_t conn_id,
                              const void *data_ptr,
                              size_t data_len,
                              video_frame_info_t *info_ptr);
```

- [ ] **Step 3: Add video fields and emit helpers to stub state**

In `Agora_Rtc_Stub_State_t`, add:

```c
int send_video_calls;
int target_bitrate_callbacks;
int key_frame_callbacks;
video_data_type_e last_video_type;
video_frame_type_e last_video_frame_type;
video_stream_type_e last_video_stream_type;
const void *last_video_data;
size_t last_video_len;
video_frame_rate_e last_video_frame_rate;
uint32_t last_target_bitrate;
```

Add prototypes:

```c
void Agora_Rtc_Stub_Emit_Video(const void *data, size_t len, video_data_type_e type, video_frame_type_e frame_type);
void Agora_Rtc_Stub_Emit_Video_For_Conn(connection_id_t conn_id,
                                        const void *data,
                                        size_t len,
                                        video_data_type_e type,
                                        video_frame_type_e frame_type);
void Agora_Rtc_Stub_Emit_Target_Bitrate(uint32_t target_bitrate);
void Agora_Rtc_Stub_Emit_Key_Frame_Request(uint32_t uid);
```

- [ ] **Step 4: Implement `agora_rtc_send_video_data` in the stub**

In `/root/smp/ai_iot_sdk/test/host/stubs/agora_rtc_stub.c`, add after `agora_rtc_send_audio_data`:

```c
int agora_rtc_send_video_data(connection_id_t conn_id,
                              const void *data_ptr,
                              size_t data_len,
                              video_frame_info_t *info_ptr)
{
    (void)conn_id;
    s_stub.send_video_calls++;
    s_stub.last_video_data = data_ptr;
    s_stub.last_video_len = data_len;
    if (info_ptr != NULL)
    {
        s_stub.last_video_type = info_ptr->data_type;
        s_stub.last_video_frame_type = info_ptr->frame_type;
        s_stub.last_video_stream_type = info_ptr->stream_type;
        s_stub.last_video_frame_rate = info_ptr->frame_rate;
    }
    return 0;
}
```

- [ ] **Step 5: Implement video emit helpers**

In `/root/smp/ai_iot_sdk/test/host/stubs/agora_rtc_stub.c`, add:

```c
void Agora_Rtc_Stub_Emit_Video(const void *data, size_t len, video_data_type_e type, video_frame_type_e frame_type)
{
    Agora_Rtc_Stub_Emit_Video_For_Conn(s_stub.conn_id, data, len, type, frame_type);
}

void Agora_Rtc_Stub_Emit_Video_For_Conn(connection_id_t conn_id,
                                        const void *data,
                                        size_t len,
                                        video_data_type_e type,
                                        video_frame_type_e frame_type)
{
    video_frame_info_t info = {
        .data_type = type,
        .stream_type = VIDEO_STREAM_HIGH,
        .frame_type = frame_type,
        .frame_rate = 0,
        .rotation = VIDEO_ORIENTATION_0,
    };
    if (s_handler.on_video_data != NULL)
    {
        s_handler.on_video_data(conn_id, 77u, 321u, data, len, &info);
    }
}

void Agora_Rtc_Stub_Emit_Target_Bitrate(uint32_t target_bitrate)
{
    if (s_handler.on_target_bitrate_changed != NULL)
    {
        s_stub.target_bitrate_callbacks++;
        s_stub.last_target_bitrate = target_bitrate;
        s_handler.on_target_bitrate_changed(s_stub.conn_id, target_bitrate);
    }
}

void Agora_Rtc_Stub_Emit_Key_Frame_Request(uint32_t uid)
{
    if (s_handler.on_key_frame_gen_req != NULL)
    {
        s_stub.key_frame_callbacks++;
        s_handler.on_key_frame_gen_req(s_stub.conn_id, uid, VIDEO_STREAM_HIGH);
    }
}
```

- [ ] **Step 6: Build host tests to verify the stub compiles**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test_ai_rtc_facade_agora_backend
```

Expected:

```text
cc ... -o test_ai_rtc_facade_agora_backend
```

- [ ] **Step 7: Commit Task 2**

Run:

```bash
git -C /root/smp/ai_iot_sdk add test/host/stubs/agora_rtc_api.h test/host/stubs/agora_rtc_stub.c
git -C /root/smp/ai_iot_sdk commit -m "test(rtc): add host Agora video stubs"
```

## Task 3: Agora Service Video TX/RX

**Files:**
- Modify: `/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend.c`
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.h`
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.c`
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade_agora_backend.c`

- [ ] **Step 1: Add video callback state to the Agora backend test**

In `Test_State_t`, add:

```c
int video_rx_calls;
Ai_Rtc_Facade_Video_Frame_t last_video;
```

Add this callback:

```c
static int on_video_rx(const Ai_Rtc_Facade_Video_Frame_t *frame, void *user)
{
    Test_State_t *test = (Test_State_t *)user;
    if (frame == NULL)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }
    test->video_rx_calls++;
    test->last_video = *frame;
    return AI_RTC_FACADE_OK;
}
```

In the test callbacks struct, add:

```c
.on_video_rx = on_video_rx,
```

Set the test config video flag to true in `test_agora_backend_flow`:

```c
.enable_video = true,
```

- [ ] **Step 2: Add the failing Agora video behavior assertions**

In `test_agora_backend_flow`, after `Agora_Rtc_Stub_Emit_Joined();`, add:

```c
uint8_t video_data[6] = {9, 8, 7, 6, 5, 4};
const Ai_Rtc_Facade_Video_Frame_t video = {
    .data = video_data,
    .len = sizeof(video_data),
    .format = AI_RTC_FACADE_VIDEO_FORMAT_H264,
    .width = 640,
    .height = 360,
    .frame_rate_hz = 15,
    .timestamp_ms = 20,
    .flags = AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME,
};

CHECK(Ai_Rtc_Facade_Send_Video(&video) == AI_RTC_FACADE_OK);
CHECK(Agora_Rtc_Stub_State()->send_video_calls == 1);
CHECK(Agora_Rtc_Stub_State()->last_video_data == video_data);
CHECK(Agora_Rtc_Stub_State()->last_video_len == sizeof(video_data));
CHECK(Agora_Rtc_Stub_State()->last_video_type == VIDEO_DATA_TYPE_H264);
CHECK(Agora_Rtc_Stub_State()->last_video_frame_type == VIDEO_FRAME_KEY);
CHECK(Agora_Rtc_Stub_State()->last_video_frame_rate == 15);

Agora_Rtc_Stub_Emit_Video(video_data, sizeof(video_data), VIDEO_DATA_TYPE_H264, VIDEO_FRAME_KEY);
CHECK(test.video_rx_calls == 1);
CHECK(test.last_video.data == video_data);
CHECK(test.last_video.len == sizeof(video_data));
CHECK(test.last_video.format == AI_RTC_FACADE_VIDEO_FORMAT_H264);
CHECK(test.last_video.flags == AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME);
```

- [ ] **Step 3: Run the Agora backend test and confirm it fails**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test_ai_rtc_facade_agora_backend
/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend
```

Expected before implementation:

```text
FAIL ... Ai_Rtc_Facade_Send_Video(&video) == AI_RTC_FACADE_OK
```

- [ ] **Step 4: Add service/backend video function declarations**

In `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.h`, add:

```c
int Ai_Rtc_Agora_Service_Send_Video(const Ai_Rtc_Facade_Video_Frame_t *frame);
```

In `/root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade_agora_backend.c`, add:

```c
static int agora_backend_send_video(const Ai_Rtc_Facade_Video_Frame_t *frame)
{
    return Ai_Rtc_Agora_Service_Send_Video(frame);
}
```

Add to `s_backend`:

```c
.send_video = agora_backend_send_video,
```

- [ ] **Step 5: Add video mapping helpers to the Agora service**

In `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.c`, add after the audio mapping helpers:

```c
static video_data_type_e map_facade_video_format(Ai_Rtc_Facade_Video_Format_t format)
{
    switch (format)
    {
        case AI_RTC_FACADE_VIDEO_FORMAT_JPEG:
            return VIDEO_DATA_TYPE_GENERIC_JPEG;
        case AI_RTC_FACADE_VIDEO_FORMAT_H264:
        default:
            return VIDEO_DATA_TYPE_H264;
    }
}

static Ai_Rtc_Facade_Video_Format_t map_agora_video_format(const video_frame_info_t *info)
{
    if (info != NULL && info->data_type == VIDEO_DATA_TYPE_GENERIC_JPEG)
    {
        return AI_RTC_FACADE_VIDEO_FORMAT_JPEG;
    }
    return AI_RTC_FACADE_VIDEO_FORMAT_H264;
}

static video_frame_type_e map_facade_video_frame_type(uint32_t flags)
{
    return ((flags & AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME) != 0u)
               ? VIDEO_FRAME_KEY
               : VIDEO_FRAME_DELTA;
}

static uint32_t map_agora_video_flags(const video_frame_info_t *info)
{
    if (info != NULL && info->frame_type == VIDEO_FRAME_KEY)
    {
        return AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME;
    }
    return 0u;
}
```

- [ ] **Step 6: Add Agora video callbacks**

In `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.c`, add:

```c
static void on_video_data(connection_id_t conn_id,
                          uint32_t uid,
                          uint16_t sent_ts,
                          const void *data_ptr,
                          size_t data_len,
                          const video_frame_info_t *info_ptr)
{
    Ai_Rtc_Facade_Video_Frame_t frame;

    (void)uid;
    if (!service_accept_callback(conn_id) || !s_agora.joined)
    {
        return;
    }
    memset(&frame, 0, sizeof(frame));
    frame.data = (const uint8_t *)data_ptr;
    frame.len = data_len;
    frame.format = map_agora_video_format(info_ptr);
    frame.frame_rate_hz = (info_ptr != NULL) ? info_ptr->frame_rate : 0u;
    frame.timestamp_ms = sent_ts;
    frame.flags = map_agora_video_flags(info_ptr);
    Ai_Rtc_Facade_Backend_Notify_Video_Rx(&frame);
}

static void on_target_bitrate_changed(connection_id_t conn_id, uint32_t target_bps)
{
    (void)target_bps;
    if (!service_accept_callback(conn_id))
    {
        return;
    }
}

static void on_key_frame_gen_req(connection_id_t conn_id, uint32_t uid, video_stream_type_e stream_type)
{
    (void)uid;
    (void)stream_type;
    if (!service_accept_callback(conn_id))
    {
        return;
    }
}
```

Add these handler assignments in `make_event_handler`:

```c
handler.on_video_data = on_video_data;
handler.on_target_bitrate_changed = on_target_bitrate_changed;
handler.on_key_frame_gen_req = on_key_frame_gen_req;
```

- [ ] **Step 7: Implement Agora video send**

In `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.c`, add after `Ai_Rtc_Agora_Service_Send_Audio`:

```c
int Ai_Rtc_Agora_Service_Send_Video(const Ai_Rtc_Facade_Video_Frame_t *frame)
{
    video_frame_info_t info;

    if (frame == NULL || frame->data == NULL || frame->len == 0u)
    {
        return AI_RTC_FACADE_ERR_NOT_READY;
    }
    if (!s_agora.session_active || !s_agora.joined)
    {
        return AI_RTC_FACADE_ERR_NOT_READY;
    }

    memset(&info, 0, sizeof(info));
    info.data_type = map_facade_video_format(frame->format);
    info.stream_type = VIDEO_STREAM_HIGH;
    info.frame_type = map_facade_video_frame_type(frame->flags);
    info.frame_rate = (video_frame_rate_e)frame->frame_rate_hz;
    info.rotation = VIDEO_ORIENTATION_0;

    return (agora_rtc_send_video_data(s_agora.conn_id, frame->data, frame->len, &info) < 0)
        ? AI_RTC_FACADE_ERR_INTERNAL
        : AI_RTC_FACADE_OK;
}
```

- [ ] **Step 8: Run the Agora backend host test and confirm it passes**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test_ai_rtc_facade_agora_backend
/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend
```

Expected:

```text
PASS: ai rtc facade agora backend
```

- [ ] **Step 9: Commit Task 3**

Run:

```bash
git -C /root/smp/ai_iot_sdk add media/rtc_facade/src/agora/ai_rtc_agora_service.c media/rtc_facade/src/agora/ai_rtc_agora_service.h media/rtc_facade/src/ai_rtc_facade_agora_backend.c test/host/test_ai_rtc_facade_agora_backend.c
git -C /root/smp/ai_iot_sdk commit -m "feat(rtc): implement Agora video facade backend"
```

## Task 4: Static Guards And Public Header Check

**Files:**
- Modify: `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_public_header.py`
- Modify: `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_sdk_agora_migration.py`

- [ ] **Step 1: Update public header checker wording**

In `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_public_header.py`, remove any assertion that requires `Ai_Rtc_Facade_Send_Video` to return `AI_RTC_FACADE_ERR_UNSUPPORTED`. Keep compile coverage for:

```c
const Ai_Rtc_Facade_Video_Frame_t video = {
    .data = audio_buf,
    .len = sizeof(audio_buf),
    .format = AI_RTC_FACADE_VIDEO_FORMAT_H264,
    .width = 320,
    .height = 240,
    .frame_rate_hz = 15,
    .flags = AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME,
};
rc += Ai_Rtc_Facade_Send_Video(&video);
```

- [ ] **Step 2: Add service video static checks**

In `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_sdk_agora_migration.py`, add these required tokens to the SDK Agora service checks:

```python
for token in (
    "handler.on_video_data = on_video_data",
    "handler.on_target_bitrate_changed = on_target_bitrate_changed",
    "handler.on_key_frame_gen_req = on_key_frame_gen_req",
    "agora_rtc_send_video_data",
    "Ai_Rtc_Facade_Backend_Notify_Video_Rx",
):
    if token not in service_text:
        failures.append(f"SDK Agora service missing video token: {token}")
```

Add this public header guard:

```python
if "ERR_UNSUPPORTED" in re.sub(r"AI_RTC_FACADE_ERR_UNSUPPORTED", "", public_text):
    failures.append("public facade header must not describe video as unsupported")
```

- [ ] **Step 3: Run static guards**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host check_ai_rtc_facade_public_header check_ai_rtc_facade_sdk_agora_migration
```

Expected:

```text
PASS: ai rtc facade public header
PASS: SDK Agora RTC migration boundary check
```

- [ ] **Step 4: Commit Task 4**

Run:

```bash
git -C /root/smp/ai_iot_sdk add test/host/check_ai_rtc_facade_public_header.py test/host/check_ai_rtc_facade_sdk_agora_migration.py
git -C /root/smp/ai_iot_sdk commit -m "test(rtc): guard SDK video facade backend"
```

## Task 5: Beken Compatibility Video Mapping

**Files:**
- Modify: `/root/smp/bk_solution_ai/components/network_transfer/network_transfer.c`

- [ ] **Step 1: Replace the hard-coded unsupported video bridge**

In `/root/smp/bk_solution_ai/components/network_transfer/network_transfer.c`, replace:

```c
static int ntwk_trans_agora_facade_video_send(frame_buffer_t *frame)
{
    (void)frame;
    return AI_RTC_FACADE_ERR_UNSUPPORTED;
}
```

with:

```c
static int ntwk_trans_agora_facade_video_send(frame_buffer_t *frame)
{
    Ai_Rtc_Facade_Video_Frame_t video;
    int ret;

    if (frame == NULL || frame->frame == NULL || frame->length == 0)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }

    memset(&video, 0, sizeof(video));
    video.data = (const uint8_t *)frame->frame;
    video.len = (size_t)frame->length;
    video.width = frame->width;
    video.height = frame->height;
    video.frame_rate_hz = 0;
    video.timestamp_ms = frame->timestamp;

    if (frame->fmt == PIXEL_FMT_H264)
    {
        video.format = AI_RTC_FACADE_VIDEO_FORMAT_H264;
        if ((frame->h264_type & (1 << H264_NAL_I_FRAME)) != 0)
        {
            video.flags |= AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME;
        }
    }
    else if (frame->fmt == PIXEL_FMT_JPEG)
    {
        video.format = AI_RTC_FACADE_VIDEO_FORMAT_JPEG;
        video.flags |= AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME;
    }
    else
    {
        return AI_RTC_FACADE_ERR_UNSUPPORTED;
    }

    ret = Ai_Rtc_Facade_Send_Video(&video);
    return (ret == AI_RTC_FACADE_OK) ? (int)frame->length : ret;
}
```

- [ ] **Step 2: Keep product runtime video disabled**

Verify `/root/smp/bk_solution_ai/projects/beken_genie_rino/ap/app_rtc_facade_bridge.c` still initializes:

```c
.enable_video = false,
.on_video_rx = NULL,
```

Do not change those two fields in W14.

- [ ] **Step 3: Commit Task 5**

Run:

```bash
git -C /root/smp/bk_solution_ai add components/network_transfer/network_transfer.c
git -C /root/smp/bk_solution_ai commit -m "build(rtc): map Beken video bridge to SDK facade"
```

If `/root/smp/bk_solution_ai` is not a valid git repository in this workspace, skip the commit command and record the product diff in the W14 validation notes.

## Task 6: Full Host Verification And Cleanup

**Files:**
- Verify: `/root/smp/ai_iot_sdk/test/host/Makefile`
- Clean: generated host test binaries in `/root/smp/ai_iot_sdk/test/host/`

- [ ] **Step 1: Run complete SDK host verification**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test
```

Expected:

```text
PASS: entity interface reference template
PASS: ai rtc facade public header
PASS: ai rtc facade core boundary
PASS: SDK Agora RTC migration boundary check
PASS valid alloc/free
PASS valid realloc
PASS foreign pointer free
PASS double-free
PASS corrupt magic
PASS Bsp_Psram aliases
PASS: ai rtc facade core
PASS: ai rtc facade agora backend
PASS: ai rtc agora datastream
```

- [ ] **Step 2: Clean host test binaries**

Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host clean
```

Expected:

```text
rm -f test_bk7258_allocator_guard test_ai_rtc_facade_core test_ai_rtc_facade_agora_backend test_ai_rtc_agora_datastream
```

- [ ] **Step 3: Confirm clean SDK worktree**

Run:

```bash
git -C /root/smp/ai_iot_sdk status --short
```

Expected after all W14 SDK commits:

```text
```

If Task 5 product diff is uncommitted because `/root/smp/bk_solution_ai` is not a valid git repository, record that as the only remaining product-side diff.

## Acceptance Criteria

- `Ai_Rtc_Facade_Send_Video` validates init, frame pointer, data pointer, length, `enable_video`, and joined state.
- SDK Agora backend calls `agora_rtc_send_video_data`.
- SDK Agora backend maps H264 and JPEG formats.
- SDK Agora backend delivers `on_video_data` through `Ai_Rtc_Facade_Backend_Notify_Video_Rx`.
- Public facade header documents RX data lifetime.
- `on_target_bitrate_changed` and `on_key_frame_gen_req` are registered and accepted without public API expansion.
- Beken product keeps `enable_video=false`.
- SDK host verification passes and host binaries are cleaned.

## Closure Result

Status: accepted at SDK host verification and Beken build verification level.

SDK result:

- SDK video facade backend is implemented behind `ai_rtc_facade.h`.
- Public API remains `ai_rtc_facade.h`; Agora video types stay private to the SDK Agora backend/stub layer.
- `Ai_Rtc_Facade_Send_Video()` routes through the private backend vtable.
- SDK Agora backend maps facade H264/JPEG video frames to `video_frame_info_t` and calls `agora_rtc_send_video_data()`.
- SDK Agora backend registers `on_video_data`, `on_target_bitrate_changed`, and `on_key_frame_gen_req`.
- `on_video_data` relays received frames through `Ai_Rtc_Facade_Backend_Notify_Video_Rx()`.

SDK verification:

- Command: `make -C /root/smp/ai_iot_sdk/test/host test`
- Result: passed.
- Observed pass lines:
  - `PASS: entity interface reference template`
  - `PASS: ai rtc facade public header`
  - `PASS: ai rtc facade core boundary`
  - `PASS: SDK Agora RTC migration boundary check`
  - `PASS valid alloc/free`
  - `PASS valid realloc`
  - `PASS foreign pointer free`
  - `PASS double-free`
  - `PASS corrupt magic`
  - `PASS Bsp_Psram aliases`
  - `PASS: ai rtc facade core`
  - `PASS: ai rtc facade agora backend`
  - `PASS: ai rtc agora datastream`
- Cleanup command: `make -C /root/smp/ai_iot_sdk/test/host clean`
- Cleanup result: no host test binaries remained under `/root/smp/ai_iot_sdk/test/host`.

Beken product result:

- `components/network_transfer/network_transfer.c` maps `frame_buffer_t` video compatibility sends to `Ai_Rtc_Facade_Video_Frame_t` and `Ai_Rtc_Facade_Send_Video()`.
- The product bridge includes only `<driver/h264_types.h>` for `H264_NAL_I_FRAME`; it does not depend on the full H264 driver API header.
- Product runtime video remains disabled in `projects/beken_genie_rino/ap/app_rtc_facade_bridge.c` with `.enable_video = false` and `.on_video_rx = NULL`.
- `/root/smp/bk_solution_ai` is not a git repository in this workspace, so the Beken product diff is intentionally uncommitted and recorded in Beken progress.

Beken build verification:

- Command: `make -C /root/smp/bk_solution_ai/projects/beken_genie_rino bk7258 SDK_DIR=/root/smp/bk_avdk_smp`
- Result: passed.
- The incremental AP build recompiled `armino/network_transfer/CMakeFiles/__armino_network_transfer.dir/network_transfer.c.obj`, linked AP/CP images, and generated firmware packages.
- Firmware output: `/root/smp/bk_solution_ai/projects/beken_genie_rino/build/bk7258/beken_genie_rino/package/all-app.bin` (3,777,600 bytes).
- OTA output: `/root/smp/bk_solution_ai/projects/beken_genie_rino/build/bk7258/beken_genie_rino/package/app_pack.rbl` (2,041,904 bytes).

Beken runtime verification:

- Runtime log: `/tmp/log.txt` (16,422 bytes, 232 lines).
- User-observed result: voice conversation was normal.
- Runtime reached token-ready, starting, joining, joined, remote-user-joined, and stopped states.
- Runtime observed `[RTC_FACADE_TX]`, `[RTC_FACADE_RX]`, and `[RTC_FACADE_DS]`.
- Counts from the captured log: 30 sampled TX lines with max frame marker 7200, 19 sampled RX lines with max frame marker 11900, and 70 datastream lines.
- Stop path reached `[RTC_FACADE] event=8 state=0 detail=0` and `APP_EVT_AGORA_SESSION_STOP`.
- Fatal scan found no assert, panic, watchdog, hard fault, RTC failed state, or `APP_EVT_AGENT_START_FAIL`.
- Two prompt/player stop lines near `APP_EVT_AGENT_JOINED` were not treated as W14 RTC regressions because subsequent RTC RX/TX/DS continued normally.

## Out Of Scope

- Enabling Beken product camera/video runtime.
- Adding public bitrate or keyframe request callbacks.
- Adding video encoder control APIs.
- Changing LCD, key, speaker, mic, Wi-Fi, Bluetooth, or boot timing.
- Moving Beken Agora vendor archives into SDK; that belongs to a separate Beken vendor ownership plan.
