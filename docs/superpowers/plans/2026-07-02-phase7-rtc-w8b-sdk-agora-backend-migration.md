# Phase 7 RTC W8b SDK Agora Backend Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move the ESP32S3 Agora RTC integration from the product `network_transfer/agora_rtc` component into the SDK-owned RTC facade backend while keeping `ai_rtc_facade.h` as the only public product API.

**Architecture:** The SDK keeps the public facade chip-neutral and hides Agora-specific types, lifecycle, datastream framing, memory policy, and platform hooks behind private backend files. The ESP32S3 product implementation under `/mnt/c/Users/harold.chen/esp32s3_ai_alarm/ai_components/network_transfer/agora_rtc` is the reference source for Agora lifecycle, network notification, datastream segmentation, PSRAM allocation policy, and AOSL memory wrapping; BK7258 remains a product-specific validation target that consumes the SDK facade rather than owning the common Agora integration.

**Tech Stack:** C11, Agora IoT RTC SDK, SDK `media/rtc_facade`, SDK `chip_esp32s3` port layer, ESP-IDF component build, Armino/BK compatibility build, Python 3 static boundary checkers, host stubs.

---

## Scope

W8b does:

- preserve `/root/smp/ai_iot_sdk/media/rtc_facade/include/ai_rtc_facade.h` as the only public RTC integration header;
- move the reusable Agora RTC service behavior from ESP32S3 product `network_transfer/agora_rtc/*` into SDK-private facade backend files;
- use BK7258 W8b product parser requirements as validation input for SDK datastream envelope parsing and state/user message mapping;
- introduce a private SDK Agora platform port interface so chip-specific RTOS, memory, task, network, and AOSL-wrap behavior is not embedded in product `network_transfer`;
- add an ESP32S3 SDK port implementation based on `rtc_port_esp32.c` and `aosl_mem_psram.c`;
- keep BK7258 consuming the existing SDK facade backend path without depending on ESP32S3-only headers;
- provide static guards that reject product direct dependencies on `network_transfer/agora_rtc`;
- keep the existing host facade tests passing.

W8b does not:

- change the public facade API shape;
- delete the original ESP32S3 product files before the SDK replacement is compiled and validated;
- move product UI, key, prompt, DP, display, or board policy into the SDK;
- discard BK product parser validation; BK remains the product proof that SDK datastream behavior is usable;
- force BK7258 to use ESP32S3 port files;
- implement final product control semantics for every datastream message.

## Source Reference

Reference product files to migrate from:

- `/mnt/c/Users/harold.chen/esp32s3_ai_alarm/ai_components/network_transfer/agora_rtc/agora_rtc.c`
- `/mnt/c/Users/harold.chen/esp32s3_ai_alarm/ai_components/network_transfer/agora_rtc/agora_rtc.h`
- `/mnt/c/Users/harold.chen/esp32s3_ai_alarm/ai_components/network_transfer/agora_rtc/agora_rtc_app.c`
- `/mnt/c/Users/harold.chen/esp32s3_ai_alarm/ai_components/network_transfer/agora_rtc/agora_rtc_app.h`
- `/mnt/c/Users/harold.chen/esp32s3_ai_alarm/ai_components/network_transfer/agora_rtc/agora_rtc_msg_process.c`
- `/mnt/c/Users/harold.chen/esp32s3_ai_alarm/ai_components/network_transfer/agora_rtc/agora_rtc_msg_process.h`
- `/mnt/c/Users/harold.chen/esp32s3_ai_alarm/ai_components/network_transfer/agora_rtc/agora_config.h`
- `/mnt/c/Users/harold.chen/esp32s3_ai_alarm/ai_components/network_transfer/agora_rtc/aosl_mem_psram.c`
- `/mnt/c/Users/harold.chen/esp32s3_ai_alarm/ai_components/network_transfer/agora_rtc/rtc_port.h`
- `/mnt/c/Users/harold.chen/esp32s3_ai_alarm/ai_components/network_transfer/agora_rtc/rtc_port_esp32.c`

Current SDK facade files:

- `/root/smp/ai_iot_sdk/media/rtc_facade/include/ai_rtc_facade.h`
- `/root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade.c`
- `/root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade_backend.h`
- `/root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade_agora_backend.c`
- `/root/smp/ai_iot_sdk/media/rtc_facade/CMakeLists.txt`

## Target File Structure

Create SDK-private Agora backend files:

- Create: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.h`
- Create: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.c`
- Create: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_datastream.h`
- Create: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_datastream.c`
- Create: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_port.h`
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade_agora_backend.c`
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/CMakeLists.txt`

Create ESP32S3 SDK port files:

- Create: `/root/smp/ai_iot_sdk/chip_esp32s3/rtc/ai_rtc_agora_port_esp32.c`
- Create: `/root/smp/ai_iot_sdk/chip_esp32s3/rtc/ai_rtc_agora_aosl_psram_wrap.c`
- Modify: `/root/smp/ai_iot_sdk/chip_esp32s3/CMakeLists.txt`

Add SDK checks and tests:

- Create: `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_sdk_agora_migration.py`
- Modify: `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_core_boundary.py`
- Modify: `/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend.c`
- Modify: `/root/smp/ai_iot_sdk/test/host/Makefile`

Product follow-up files after SDK backend validation:

- Modify: `/mnt/c/Users/harold.chen/esp32s3_ai_alarm/ai_components/network_transfer/CMakeLists.txt`
- Modify: ESP32S3 product call sites that include `agora_rtc/*.h`, replacing them with `ai_rtc_facade.h` or product-local bridge code.

## Private Interface Contracts

### SDK Agora Port Interface

Create `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_port.h` with this private contract:

```c
#ifndef AI_RTC_AGORA_PORT_H
#define AI_RTC_AGORA_PORT_H

#include <stddef.h>
#include <stdint.h>

typedef void *Ai_Rtc_Agora_Port_Sema_t;
typedef void *Ai_Rtc_Agora_Port_Queue_t;
typedef void *Ai_Rtc_Agora_Port_Task_t;

typedef enum
{
    AI_RTC_AGORA_PORT_NETWORK_DOWN = 0,
    AI_RTC_AGORA_PORT_NETWORK_UP,
    AI_RTC_AGORA_PORT_NETWORK_CHANGE,
} Ai_Rtc_Agora_Port_Network_Event_t;

typedef void (*Ai_Rtc_Agora_Port_Network_Cb)(Ai_Rtc_Agora_Port_Network_Event_t event,
                                             void *user);

typedef struct
{
    int (*sema_create)(Ai_Rtc_Agora_Port_Sema_t *out, int max_count);
    void (*sema_destroy)(Ai_Rtc_Agora_Port_Sema_t *sema);
    int (*sema_give)(Ai_Rtc_Agora_Port_Sema_t *sema);
    int (*sema_take)(Ai_Rtc_Agora_Port_Sema_t *sema, uint32_t timeout_ms);

    int (*queue_create)(Ai_Rtc_Agora_Port_Queue_t *out, uint16_t depth, uint32_t item_size);
    void (*queue_destroy)(Ai_Rtc_Agora_Port_Queue_t *queue);
    int (*queue_send)(Ai_Rtc_Agora_Port_Queue_t *queue,
                      const void *item,
                      uint32_t item_size,
                      uint32_t timeout_ms);
    int (*queue_recv)(Ai_Rtc_Agora_Port_Queue_t *queue,
                      void *item,
                      uint32_t *item_size,
                      uint32_t timeout_ms);

    int (*task_create)(Ai_Rtc_Agora_Port_Task_t *out,
                       const char *name,
                       uint32_t stack_size,
                       uint32_t priority,
                       void (*entry)(void *),
                       void *arg);
    void (*task_delete)(Ai_Rtc_Agora_Port_Task_t *task);

    void *(*mem_alloc)(size_t size);
    void *(*mem_zalloc)(size_t size);
    void (*mem_free)(void *ptr);

    void (*sleep_ms)(uint32_t ms);
    uint32_t (*timestamp_ms)(void);
    void (*debug_heap)(const char *stage);

    int (*network_register)(Ai_Rtc_Agora_Port_Network_Cb cb, void *user);
    void (*network_unregister)(void);
    int (*network_refresh)(void);
} Ai_Rtc_Agora_Port_t;

const Ai_Rtc_Agora_Port_t *Ai_Rtc_Agora_Port_Get(void);

#endif /* AI_RTC_AGORA_PORT_H */
```

### SDK Agora Datastream Parser Boundary

Create `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_datastream.h` with this private contract:

```c
#ifndef AI_RTC_AGORA_DATASTREAM_H
#define AI_RTC_AGORA_DATASTREAM_H

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    AI_RTC_AGORA_DS_OBJECT_UNKNOWN = 0,
    AI_RTC_AGORA_DS_OBJECT_MESSAGE_USER,
    AI_RTC_AGORA_DS_OBJECT_MESSAGE_STATE,
} Ai_Rtc_Agora_Datastream_Object_t;

typedef enum
{
    AI_RTC_AGORA_DS_STATE_UNKNOWN = 0,
    AI_RTC_AGORA_DS_STATE_LISTENING,
    AI_RTC_AGORA_DS_STATE_THINKING,
    AI_RTC_AGORA_DS_STATE_SPEAKING,
    AI_RTC_AGORA_DS_STATE_SILENT,
} Ai_Rtc_Agora_Datastream_State_t;

typedef struct
{
    Ai_Rtc_Agora_Datastream_Object_t object;
    Ai_Rtc_Agora_Datastream_State_t state;
    const char *raw_json;
    size_t raw_json_len;
} Ai_Rtc_Agora_Datastream_Event_t;

typedef void (*Ai_Rtc_Agora_Datastream_Event_Cb)(const Ai_Rtc_Agora_Datastream_Event_t *event,
                                                 void *user);

void Ai_Rtc_Agora_Datastream_Reset(void);
int Ai_Rtc_Agora_Datastream_On_Message(const uint8_t *data,
                                       size_t len,
                                       Ai_Rtc_Agora_Datastream_Event_Cb cb,
                                       void *user);

#endif /* AI_RTC_AGORA_DATASTREAM_H */
```

The public `Ai_Rtc_Facade_Datastream_Rx_Cb` remains the raw datastream callback. Parsed SDK-private events are used by backend tests and future product bridge code; they must not add public product policy to `ai_rtc_facade.h`.

## BK Validation Feedback Loop

BK7258 W8b product parser work is part of SDK hardening, not a competing product-only direction:

- SDK owns the reusable Agora datastream envelope parser: segmentation, base64 decode, JSON object/state extraction, bounds, and malformed-message behavior.
- BK product validates the SDK behavior against real board logs and maps parsed state/user/control information to product logs/events.
- If BK discovers a parser requirement that is not product policy, move that requirement into SDK tests and SDK private parser code.
- Keep BK-specific UI, prompt, key, display, and DP behavior in the product.
- Do not expose Agora vendor types or legacy `network_transfer/agora_rtc` headers to make BK validation work.

## Tasks

### Task 1: Add SDK Migration Boundary Checker

**Files:**

- Create: `/root/smp/ai_iot_sdk/test/host/check_ai_rtc_facade_sdk_agora_migration.py`
- Modify: `/root/smp/ai_iot_sdk/test/host/Makefile`

- [ ] Create the checker with these rules:

```python
#!/usr/bin/env python3
"""Static checks for SDK-owned Agora RTC facade migration."""

from __future__ import annotations

import re
from pathlib import Path


SDK = Path("/root/smp/ai_iot_sdk")
ESP_PRODUCT = Path("/mnt/c/Users/harold.chen/esp32s3_ai_alarm")

PUBLIC_HEADER = SDK / "media/rtc_facade/include/ai_rtc_facade.h"
RTC_FACADE = SDK / "media/rtc_facade"
SDK_AGORA_DIR = RTC_FACADE / "src/agora"
ESP_PRODUCT_AGORA = ESP_PRODUCT / "ai_components/network_transfer/agora_rtc"
ESP_PRODUCT_NETWORK_CMAKE = ESP_PRODUCT / "ai_components/network_transfer/CMakeLists.txt"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace") if path.exists() else ""


def main() -> int:
    failures: list[str] = []

    required_sdk_files = (
        SDK_AGORA_DIR / "ai_rtc_agora_service.h",
        SDK_AGORA_DIR / "ai_rtc_agora_service.c",
        SDK_AGORA_DIR / "ai_rtc_agora_datastream.h",
        SDK_AGORA_DIR / "ai_rtc_agora_datastream.c",
        SDK_AGORA_DIR / "ai_rtc_agora_port.h",
        SDK / "chip_esp32s3/rtc/ai_rtc_agora_port_esp32.c",
        SDK / "chip_esp32s3/rtc/ai_rtc_agora_aosl_psram_wrap.c",
    )
    for path in required_sdk_files:
        if not path.exists():
            failures.append(f"missing SDK Agora migration file: {path}")

    public_text = read(PUBLIC_HEADER)
    forbidden_public_tokens = (
        "agora_rtc",
        "agora_iot",
        "connection_id_t",
        "audio_frame_info_t",
        "rtc_port",
        "FreeRTOS",
        "esp_",
        "bk_",
        "network_transfer",
    )
    for token in forbidden_public_tokens:
        if token in public_text:
            failures.append(f"public facade header exposes forbidden token: {token}")

    facade_backend = read(RTC_FACADE / "src/ai_rtc_facade_agora_backend.c")
    if '#include "agora/ai_rtc_agora_service.h"' not in facade_backend:
        failures.append("facade Agora backend does not use SDK Agora service wrapper")
    if "agora_rtc_api.h" in facade_backend:
        failures.append("facade Agora backend still includes vendor Agora API directly")

    service_text = read(SDK_AGORA_DIR / "ai_rtc_agora_service.c")
    if "agora_rtc_api.h" not in service_text:
        failures.append("SDK Agora service does not own the vendor Agora API include")
    if "Ai_Rtc_Agora_Port_Get" not in service_text:
        failures.append("SDK Agora service does not use private platform port")

    datastream_text = read(SDK_AGORA_DIR / "ai_rtc_agora_datastream.c")
    for token in ("message.state", "message.user", "base64_decode", "cJSON_Parse"):
        if token not in datastream_text:
            failures.append(f"SDK Agora datastream parser missing token: {token}")

    esp_cmake = read(ESP_PRODUCT_NETWORK_CMAKE)
    if "agora_rtc/*.c" in esp_cmake or re.search(r"file\\s*\\(\\s*GLOB\\s+C_FILES\\s+.*agora_rtc/\\*\\.c", esp_cmake, re.S):
        failures.append("ESP32S3 product network_transfer still glob-builds agora_rtc/*.c")

    if not ESP_PRODUCT_AGORA.exists():
        failures.append("ESP32S3 reference agora_rtc directory missing; migration cannot be audited")

    if failures:
        print("FAIL: SDK Agora RTC migration boundary check")
        for failure in failures:
            print(f"- {failure}")
        return 1

    print("PASS: SDK Agora RTC migration boundary check")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] Add this target to `/root/smp/ai_iot_sdk/test/host/Makefile`:

```make
check_ai_rtc_facade_sdk_agora_migration:
	python3 check_ai_rtc_facade_sdk_agora_migration.py
```

- [ ] Keep `check_ai_rtc_facade_sdk_agora_migration` as a standalone red target until the SDK migration is implemented.
- [ ] Do not include the red migration checker in the aggregate `test` target during Task 1, because the rest of the SDK host tests must remain usable while the migration is in progress.
- [ ] Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host check_ai_rtc_facade_sdk_agora_migration
```

Expected pre-migration result:

```text
FAIL: SDK Agora RTC migration boundary check
```

### Task 2: Add Private SDK Agora Port Contract

**Files:**

- Create: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_port.h`
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/CMakeLists.txt`

- [ ] Create `ai_rtc_agora_port.h` exactly as defined in the "SDK Agora Port Interface" section.
- [ ] Update `/root/smp/ai_iot_sdk/media/rtc_facade/CMakeLists.txt` include dirs so private source can include `src/agora`:

```cmake
set(incs
    include
    src
    src/agora
)
```

- [ ] Run:

```bash
make -C /root/smp/ai_iot_sdk/test/host test
```

Expected result after only this task: existing host tests still pass; the new migration checker remains red because service and port implementations are not present yet.

### Task 3: Move ESP32S3 Port Into SDK Chip Layer

**Files:**

- Create: `/root/smp/ai_iot_sdk/chip_esp32s3/rtc/ai_rtc_agora_port_esp32.c`
- Create: `/root/smp/ai_iot_sdk/chip_esp32s3/rtc/ai_rtc_agora_aosl_psram_wrap.c`
- Modify: `/root/smp/ai_iot_sdk/chip_esp32s3/CMakeLists.txt`

- [ ] Create `ai_rtc_agora_port_esp32.c` by adapting the behavior of:

```text
/mnt/c/Users/harold.chen/esp32s3_ai_alarm/ai_components/network_transfer/agora_rtc/rtc_port_esp32.c
```

- [ ] The file must include SDK private port header:

```c
#include "ai_rtc_agora_port.h"
```

- [ ] The implementation must provide:

```c
const Ai_Rtc_Agora_Port_t *Ai_Rtc_Agora_Port_Get(void);
```

- [ ] Preserve these ESP32S3-specific behaviors from the reference implementation:

```text
semaphore wrappers through Bsp_Semaphore_*
queue wrappers through Bsp_Msg_Queue_*
PSRAM task creation through Bsp_Pthread_Create_Ex
PSRAM allocation through Bsp_Psram_Malloc/Bsp_Psram_Zalloc/Bsp_Psram_Free
timestamp through Bsp_Get_Time_Stamp
heap debug through Bsp_Debug_Heap_Info
```

- [ ] Move ESP32S3 network event forwarding from `agora_rtc.c` into `network_register`, `network_unregister`, and `network_refresh`.
- [ ] `network_register` must subscribe to:

```text
WIFI_EVENT_STA_CONNECTED
WIFI_EVENT_STA_DISCONNECTED
WIFI_EVENT_STA_BEACON_TIMEOUT
IP_EVENT_STA_GOT_IP
IP_EVENT_STA_LOST_IP
```

- [ ] `network_refresh` must report `AI_RTC_AGORA_PORT_NETWORK_UP` when STA has a non-zero IP, otherwise `AI_RTC_AGORA_PORT_NETWORK_DOWN`.
- [ ] Create `ai_rtc_agora_aosl_psram_wrap.c` by moving the linker-wrap behavior from:

```text
/mnt/c/Users/harold.chen/esp32s3_ai_alarm/ai_components/network_transfer/agora_rtc/aosl_mem_psram.c
```

- [ ] Modify `/root/smp/ai_iot_sdk/chip_esp32s3/CMakeLists.txt` and add these sources:

```cmake
"rtc/ai_rtc_agora_port_esp32.c"
"rtc/ai_rtc_agora_aosl_psram_wrap.c"
```

- [ ] Add `"rtc"` to `INCLUDE_DIRS`.
- [ ] Add the required private dependencies:

```cmake
PRIV_REQUIRES lwip esp_common heap mi_mqtt mqtt platform_adapter esp_timer log freertos esp_event esp_netif esp_wifi
```

- [ ] Add target link options equivalent to the ESP32S3 product reference:

```cmake
target_link_options(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=aosl_hal_malloc"
    "-Wl,--wrap=aosl_hal_calloc"
    "-Wl,--wrap=aosl_hal_realloc"
    "-Wl,--wrap=aosl_hal_free"
    "-Wl,-u,__wrap_aosl_hal_malloc"
    "-Wl,-u,__wrap_aosl_hal_calloc"
    "-Wl,-u,__wrap_aosl_hal_realloc"
    "-Wl,-u,__wrap_aosl_hal_free"
)
```

If this SDK CMake context uses `${COMPONENT_TARGET}` instead of `${COMPONENT_LIB}`, use the variable already used by adjacent ESP-IDF component files in this repository and record the exact variable in validation notes.

### Task 4: Split SDK Agora Service From Facade Backend

**Files:**

- Create: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.h`
- Create: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.c`
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/ai_rtc_facade_agora_backend.c`

- [x] Create `ai_rtc_agora_service.h` with SDK-private lifecycle functions:

```c
#ifndef AI_RTC_AGORA_SERVICE_H
#define AI_RTC_AGORA_SERVICE_H

#include "ai_rtc_facade_backend.h"

int Ai_Rtc_Agora_Service_Start(const Ai_Rtc_Facade_Backend_Start_Config_t *config);
int Ai_Rtc_Agora_Service_Stop(void);
int Ai_Rtc_Agora_Service_Wait_Joined(uint32_t timeout_ms);
int Ai_Rtc_Agora_Service_Send_Audio(const Ai_Rtc_Facade_Audio_Frame_t *frame);
int Ai_Rtc_Agora_Service_Send_Datastream(const uint8_t *data, size_t len);

#endif /* AI_RTC_AGORA_SERVICE_H */
```

- [x] Move direct vendor `agora_rtc_api.h` usage from `ai_rtc_facade_agora_backend.c` into `ai_rtc_agora_service.c`.
- [x] Preserve W2b facade behavior:

```text
start validates app id, token, channel, uid
joined callback creates a reliable ordered data stream
reconnecting callback emits AI_RTC_FACADE_EVENT_RECONNECTING
rejoined callback emits AI_RTC_FACADE_EVENT_REJOINED
token will-expire callback emits AI_RTC_FACADE_EVENT_TOKEN_WILL_EXPIRE
remote user joined/offline callbacks emit facade remote-user events
audio callback relays Ai_Rtc_Facade_Audio_Frame_t
stream message callback relays raw Ai_Rtc_Facade_Datastream_Message_t
send audio maps facade audio format to Agora audio data type
send datastream uses the stream id created after join
stop leaves channel, destroys connection, and clears session state
```

- [x] Preserve ESP32S3 reference improvements:

```text
SDK init stays resident across sessions unless app id changes
stale callbacks are dropped by active flag, connection id, and generation
network events are forwarded into agora_rtc_notify_network_event
hot-path logs are compiled out unless enabled
Agora SDK memory allocations use PSRAM on ESP32S3 through AOSL wrap
```

- [x] Reduce `ai_rtc_facade_agora_backend.c` to the backend vtable wrapper:

```c
#include "ai_rtc_facade_backend.h"
#include "agora/ai_rtc_agora_service.h"

static const Ai_Rtc_Facade_Backend_t s_backend = {
    .start = Ai_Rtc_Agora_Service_Start,
    .stop = Ai_Rtc_Agora_Service_Stop,
    .wait_joined = Ai_Rtc_Agora_Service_Wait_Joined,
    .send_audio = Ai_Rtc_Agora_Service_Send_Audio,
    .send_datastream = Ai_Rtc_Agora_Service_Send_Datastream,
};

const Ai_Rtc_Facade_Backend_t *Ai_Rtc_Facade_Get_Backend(void)
{
    return &s_backend;
}
```

### Task 5: Move Datastream Envelope Parser Into SDK Private Layer

**Files:**

- Create: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_datastream.h`
- Create: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_datastream.c`
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/src/agora/ai_rtc_agora_service.c`
- Modify: `/root/smp/ai_iot_sdk/media/rtc_facade/CMakeLists.txt`

- [x] Create the datastream header exactly as defined in the "SDK Agora Datastream Parser Boundary" section.
- [x] Implement the parser in `ai_rtc_agora_datastream.c` using the reference behavior from:

```text
/mnt/c/Users/harold.chen/esp32s3_ai_alarm/ai_components/network_transfer/agora_rtc/agora_rtc_msg_process.c
```

- [x] Parser must accept the wire format:

```text
msg_id|cur_index|total_num|base64_payload
```

- [x] Parser must support up to four segments and reject larger segment counts.
- [x] Parser must decode no more than 8192 bytes of JSON payload.
- [x] Parser must decode base64 with an SDK-private helper because ESP32S3 reference and BK Armino expose incompatible base64 symbol names.
- [x] Parser must parse JSON with cJSON and recognize:

```text
object == "message.user"
object == "message.state"
state == "listening"
state == "thinking"
state == "speaking"
state == "silent"
```

- [x] Parser tests must cover the BK W8b product log contract:

```text
[RTC_FACADE_DS_STATE] state=listening
[RTC_FACADE_DS_STATE] state=thinking
[RTC_FACADE_DS_STATE] state=speaking
[RTC_FACADE_DS_STATE] state=silent
[RTC_FACADE_DS_STATE] state=unknown raw=<value>
[RTC_FACADE_DS_USER] len=<decoded_json_len>
```

- [x] Parser must not include product, ESP-IDF, BK, display, key, prompt, or DP headers.
- [x] Keep raw datastream callback relay through `Ai_Rtc_Facade_Backend_Notify_Datastream_Rx()` before invoking private parsed-event handling.
- [x] Update `/root/smp/ai_iot_sdk/media/rtc_facade/CMakeLists.txt` to build:

```cmake
src/agora/ai_rtc_agora_service.c
src/agora/ai_rtc_agora_datastream.c
```

- [x] Add private dependency for `json`; base64 is self-contained in the parser to avoid cross-SDK symbol mismatch.

### Task 6: Strengthen SDK Host Tests

**Files:**

- Modify: `/root/smp/ai_iot_sdk/test/host/test_ai_rtc_facade_agora_backend.c`
- Modify: `/root/smp/ai_iot_sdk/test/host/stubs/agora_rtc_stub.c`
- Modify: `/root/smp/ai_iot_sdk/test/host/stubs/agora_rtc_api.h`
- Modify: `/root/smp/ai_iot_sdk/test/host/Makefile`

- [x] Extend Agora stubs to record network notify, renew token, stale callback, datastream create, datastream send, and stop cleanup calls.
- [x] Add tests for these behaviors:

```text
start keeps public facade API unchanged
first join creates one reliable ordered data stream
send datastream fails before join and succeeds after join
token will-expire event reaches facade callback
reconnect and rejoin events reach facade callback
stale user/audio/datastream callbacks from old generation are ignored
stop clears active generation and connection id
raw datastream callback still receives stream id, sender uid, data pointer, length, and sent timestamp
```

- [x] Add a host parser test target when cJSON/base64 are available in the host test environment:

```make
RTC_AGORA_DATASTREAM_TARGET := test_ai_rtc_agora_datastream
```

- [x] Host cJSON/base64 stubs are available; parser coverage is implemented as `test_ai_rtc_agora_datastream`.

### Task 7: Rewire ESP32S3 Product To Consume SDK Facade

**Files:**

- Modify: `/mnt/c/Users/harold.chen/esp32s3_ai_alarm/ai_components/network_transfer/CMakeLists.txt`
- Modify: ESP32S3 product files that include `agora_rtc/agora_rtc.h`, `agora_rtc/agora_rtc_app.h`, or `agora_rtc/agora_rtc_msg_process.h`

- [x] Stop glob-building:

```cmake
"${CURRENT_DIR}/agora_rtc/*.c"
```

- [x] Replace the product `network_transfer` Agora branch with SDK facade dependency.
- [x] Keep the product-level `network_transfer` API only as a compatibility wrapper when existing product code still calls it.
- [x] Replace direct product includes of legacy Agora headers with `rtc_facade_compat.h`, which includes the SDK public facade:

```c
#include "ai_rtc_facade.h"
```

- [x] Add a product static checker under `/mnt/c/Users/harold.chen/esp32s3_ai_alarm/test/host/` or the closest existing test tools directory that fails on:

```text
ai_components/network_transfer/agora_rtc/*.c in active build
#include "agora_rtc.h"
#include "agora_rtc_app.h"
#include "agora_rtc_msg_process.h"
net_agora_rtc_
Register_Agora_Recv_Msg_Cbs
```

- [x] Keep the legacy ESP32S3 source directory as reference until the SDK migration is board-validated.

**Task 7 result (2026-07-02):**

- Product active `network_transfer` now builds `rtc_facade_compat.c` only and depends on SDK `rtc_facade`.
- Product top-level CMake includes `sdk/media/rtc_facade`.
- Product SDK copy includes `media/rtc_facade` and ESP32S3 Agora port sources under `sdk/chip_esp32s3/rtc/`.
- Active product includes were rewired from `agora_rtc*.h` to `rtc_facade_compat.h`.
- Product datastream callbacks now register through `Rtc_Facade_Compat_Register_Datastream_Cbs`.
- Controller no longer calls `net_agora_rtc_mute_local_audio`; it uses `Agora_Rtc_Mute_Local_Audio` compatibility glue until the public facade grows a mute API.
- Product guard added: `/mnt/c/Users/harold.chen/esp32s3_ai_alarm/test/host/check_rtc_facade_product_rewire.py`.
- Verification passed:

```bash
python3 /mnt/c/Users/harold.chen/esp32s3_ai_alarm/test/host/check_rtc_facade_product_rewire.py
make -C /mnt/c/Users/harold.chen/esp32s3_ai_alarm/test/host guard
make -C /root/smp/ai_iot_sdk/test/host check_ai_rtc_facade_sdk_agora_migration
make -C /root/smp/ai_iot_sdk/test/host test
git -C /root/smp/ai_iot_sdk diff --check
git -C /mnt/c/Users/harold.chen/esp32s3_ai_alarm/sdk diff --check
git -C /mnt/c/Users/harold.chen/esp32s3_ai_alarm diff --check -- <Task 7 touched files>
```

- ESP32S3 firmware build: user reported local product build passed after Task 7 rewire. Build log was not inspected in this session.
- ESP32S3 runtime debug on `/tmp/phase7-task7-esp32s3-runtime.log`: token callback succeeded, but RTC start was rejected before entering SDK facade because `AI_SESSION_RTC_WORKER` enqueue failed with `reason=not_ready`. Root cause: product `rlink_app_main.c` initialized the RTC command worker only when `audio_engine_ready` was true; the runtime also showed degraded audio init (`voice is already deinit`). Fix: RTC command worker now initializes unconditionally as control-plane infrastructure, with audio readiness logged separately. Added product guard `/mnt/c/Users/harold.chen/esp32s3_ai_alarm/test/host/check_rtc_worker_bootstrap.py`.
- ESP32S3 runtime debug on `/tmp/phase7-task7-esp32s3-runtime-v2.log`: worker bootstrap was fixed and `START_QUEUED` reached SDK facade, but join wait failed in about 30 ms despite `timeout_ms=15000`. Root cause: SDK `Ai_Rtc_Agora_Service_Wait_Joined()` returned immediately when the async Agora join callback had not fired yet. Fix: SDK Agora service now polls through the private port `sleep_ms/timestamp_ms` until joined, stopped, or timeout. Added host coverage in `test_ai_rtc_facade_agora_backend.c`; synced the same service fix into the ESP32S3 product SDK copy.

### Task 8: Verification

**Commands:**

- [x] After the SDK migration checker turns green, include `check_ai_rtc_facade_sdk_agora_migration` in the `/root/smp/ai_iot_sdk/test/host/Makefile` aggregate `test` target.
- [x] Run SDK host tests:

```bash
make -C /root/smp/ai_iot_sdk/test/host test
```

Expected output includes:

```text
PASS: ai rtc facade public header
PASS: ai rtc facade core boundary
PASS: ai rtc facade core
PASS: ai rtc facade agora backend
PASS: SDK Agora RTC migration boundary check
```

- [x] Run SDK diff check:

```bash
git -C /root/smp/ai_iot_sdk diff --check
```

Expected result: no output.

- [ ] Build ESP32S3 product after rewire with its normal build command.
- [ ] Build BK7258 product to confirm SDK migration did not break the BK facade consumer:

```bash
make -C /root/smp/bk_solution_ai/projects/beken_genie_rino bk7258 SDK_DIR=/root/smp/bk_avdk_smp
```

- [ ] Run BK W8a runtime checker on accepted log as a regression:

```bash
python3 /root/smp/bk_solution_ai/projects/beken_genie_rino/tools/check_rtc_facade_runtime_log.py <(sed '/Error reading from serial device/,$d' /tmp/phase7-w8a-rtc-facade-runtime.log) --require-datastream
```

Expected output:

```text
PASS: RTC facade runtime log
observed RTC_FACADE_TX frames: 10900
observed RTC_FACADE_RX frames: 7900
observed RTC_FACADE_DS
```

## Acceptance Criteria

- `ai_rtc_facade.h` remains the only public SDK RTC integration API.
- SDK `media/rtc_facade` owns the common Agora backend service and datastream envelope parser.
- SDK `chip_esp32s3` owns the ESP32S3 Agora port and AOSL PSRAM wrap behavior.
- ESP32S3 product no longer builds `ai_components/network_transfer/agora_rtc/*.c` as its active RTC integration.
- Products do not directly include or call legacy `network_transfer/agora_rtc` APIs.
- BK7258 product still builds and consumes the SDK facade.
- SDK host tests and migration static checker pass.

## Status Correction: Runtime Policy Gap Found After W8b

2026-07-02 follow-up runtime analysis found that W8b should be treated as "SDK Agora backend migration implemented with host/static coverage", not as cross-product runtime complete.

Evidence from `/tmp/log.txt`:

- ESP32S3 `sid=2` successfully reached SDK facade, joined Agora RTC, and received `APP_EVT_AGENT_JOINED`.
- Immediately after product opened mic uplink, the device panicked with a Core 0 double exception and corrupted backtrace.
- Heap evidence showed `dma_largest` fell from `6400` before RTC start to `416` after JOIN. A 20 ms mono PCM16 frame is `640` bytes, so post-JOIN `dma_largest=416` is not a safe uplink state.

Objective correction:

- `JOINED` is not equivalent to `UPLINK_READY`.
- SDK currently lacks a runtime resource policy layer for start/send/stop safety.
- Product-side ESP32S3 heap gates are useful evidence, but the portable SDK fix should be a private port/runtime policy hook, not public API leakage or product-only tuning.

Tracked gap analysis:

- `/root/smp/ai_iot_sdk/docs/superpowers/reviews/2026-07-02-phase7-rtc-sdk-gap-analysis.md`

Open validation items:

- ESP32S3 alarm: re-test after SDK runtime policy prevents post-JOIN uplink panic and confirm repeat stop recovers internal/DMA largest.
- Beken: rebuild and re-run RTC facade runtime checker on the current SDK RTC baseline.
- xiaozhi M5Stack CoreS3: sync/compare SDK baseline, decide RTC facade rewire plan, then build and runtime-check AI/AEC conversation.
