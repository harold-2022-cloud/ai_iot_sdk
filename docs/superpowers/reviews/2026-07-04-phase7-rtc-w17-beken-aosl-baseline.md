# Phase 7 RTC W17 Beken AOSL Baseline

Date: 2026-07-04

## Scope

This note records the BK7258 AOSL vendor archive captured for the SDK after the
Beken Agora RTC vendor update.

This is a vendor baseline update, not a public RTC facade API change.

## SDK Artifact

Added SDK artifact:

- `chip_bk7258/vendor/aosl/libs/libaosl.a`

The SDK `.gitignore` keeps generic `*.a` files ignored, but now explicitly
allows this BK7258 AOSL vendor archive.

Archive hash:

- `f30a3d1602e00e3bf3f57ee92ec52c1ce7be1f9fb7edd6542b79cb7eacf89bca`

## Source

The archive was packaged from the Beken build objects generated under:

- `/root/smp/bk_solution_ai/projects/beken_genie_rino/build/bk7258/beken_genie_rino/bk7258_ap/armino/agora-iot-sdk/CMakeFiles/__armino_agora-iot-sdk.dir/hal/aosl`

Those objects correspond to the AOSL source tree integrated in:

- `/root/smp/bk_avdk_smp/ap/components/bk_thirdparty/agora-iot-sdk/hal/aosl`

## Symbol Coverage

The captured archive exports the BK7258 AOSL symbols required by the newer
Agora RTC vendor baseline:

- `aosl_bind_device`
- `aosl_bind_port_only`
- `aosl_hwrng_available`
- `aosl_rand_bytes`

Verification command:

```sh
/opt/gcc-arm-none-eabi-10.3-2021.10/bin/arm-none-eabi-nm -A --defined-only \
  /root/smp/ai_iot_sdk/chip_bk7258/vendor/aosl/libs/libaosl.a | \
  rg 'aosl_(bind_device|bind_port_only|hwrng_available|rand_bytes)$'
```

## Beken Build Acceptance

Verification command:

```sh
make -C /root/smp/bk_solution_ai/projects/beken_genie_rino bk7258 SDK_DIR=/root/smp/bk_avdk_smp
```

Result:

- AP build passed.
- CP build passed.
- Package generation passed.
- Firmware produced:
  `/root/smp/bk_solution_ai/projects/beken_genie_rino/build/bk7258/beken_genie_rino/package/all-app.bin`
- OTA binary produced:
  `/root/smp/bk_solution_ai/projects/beken_genie_rino/build/bk7258/beken_genie_rino/package/app_pack.rbl`

## Beken Runtime Acceptance

Runtime log:

- `/tmp/log.txt`

User result:

- AI conversation works.

Observed RTC sequence:

- GPIO12 started an inactive AI session:
  `[RTC] GPIO_12 pressed: session_active=0`
- Token result entered the RTC facade:
  `[RTC] >>> Ai_Rtc_Facade_On_Token_Result`
- AOSL DNS resolved the Agora edge host:
  `[aosl][DNS] End dns request ap3.agora.io:8000 cnt=1`
- RTC facade reached joined/remote-agent state:
  `[RTC_FACADE] event=1 state=4 detail=2232`
  `[RTC_FACADE] event=4 state=4 detail=16565`
- Datastream/control frames were received:
  `[RTC_FACADE_DS] stream=49153 uid=16565`
- Downlink audio frames were received:
  `[RTC_FACADE_RX] frame=600 size=640 format=0`
- Uplink microphone frames were sent:
  `[RTC_FACADE_TX] frame=800 size=160 format=0`
- GPIO12 stopped the active AI session:
  `[RTC] GPIO_12 pressed: session_active=1`
  `[RTC_FACADE] event=8 state=0 detail=0`
  `APP_EVT_AGORA_SESSION_STOP`

No runtime evidence of RTC/AOSL crash was observed in the log:

- no `panic`
- no `assert`
- no `abort`
- no `reboot`
- no `rand_bytes`
- no `mbedtls`
- no RTC socket failure

Notes:

- `play_pipeline stop fail` and `Element already stopped` appeared around
  player stop handling but did not block RTC join, datastream, audio TX, or
  audio RX.
- The final `Error reading from serial device` is treated as a host terminal
  session interruption after AI stop evidence, not as a device panic.

## Boundary

The public RTC facade remains:

- `media/rtc_facade/include/ai_rtc_facade.h`

This baseline does not expose AOSL, Agora vendor, RTM, datastream transport
selection, Beken RTOS, codec, DMA, or board peripheral details through the
public RTC facade.

Beken still owns product and platform integration details outside the public
facade boundary.
