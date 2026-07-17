# Phase 7 RTC W16b Runtime Acceptance

Date: 2026-07-04

SDK code baseline:

- `/root/smp/ai_iot_sdk`
- Commit: `43ba0fd5 feat(rtc): add private Agora control transport`

Accepted product runtime results:

- ESP32S3 `esp32s3_ai_alarm`: SDK submodule updated to `43ba0fd5`; user build passed; user reported normal AI conversation runtime.
- xiaozhi-esp32 M5Stack CoreS3: SDK submodule updated to `43ba0fd5`; user build passed; user reported normal AI conversation runtime.

Transport status:

- Public facade remains `ai_rtc_facade.h`.
- Product-visible RTC control transport remains datastream by default.
- RTM is integrated as SDK-private Agora backend capability only; no public RTM API is exposed.
- ESP32S3 and xiaozhi runtime acceptance used default datastream behavior.

Beken status:

- Beken consumes `/root/smp/ai_iot_sdk` directly, so no product SDK gitlink update is required.
- W16b C facade/control transport sources compile in the Beken build path.
- Beken build is accepted after the BK7258 Agora/AOSL vendor baseline update.
- Verification command:
  `make -C /root/smp/bk_solution_ai/projects/beken_genie_rino bk7258 SDK_DIR=/root/smp/bk_avdk_smp`
- Output firmware:
  `/root/smp/bk_solution_ai/projects/beken_genie_rino/build/bk7258/beken_genie_rino/package/all-app.bin`
- Output OTA binary:
  `/root/smp/bk_solution_ai/projects/beken_genie_rino/build/bk7258/beken_genie_rino/package/app_pack.rbl`

Decision:

- W16b SDK code baseline `43ba0fd5` is accepted for ESP32S3 and xiaozhi default datastream runtime.
- Product repo commits should contain only SDK gitlink/progress updates and must not include unrelated dirty files.
