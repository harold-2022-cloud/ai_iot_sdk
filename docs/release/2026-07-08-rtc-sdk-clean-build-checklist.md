# RTC SDK Clean Build Checklist

> Date: 2026-07-08
> Scope: SDK release prep before stale SDK removal and GitHub publication

## 1. SDK Host Verification

Run from the SDK repo:

```bash
make -C test/host test
make -C test/host clean
git diff --check
```

Expected result:

- host tests pass,
- host test binaries are removed by `clean`,
- `git diff --check` reports no whitespace errors.

## 2. SDK Boundary Verification

Run the static guards explicitly when diagnosing release failures:

```bash
python3 test/host/check_ai_rtc_facade_public_header.py
python3 test/host/check_ai_rtc_facade_core_boundary.py
python3 test/host/check_ai_rtc_facade_sdk_agora_migration.py
```

Expected result:

- `ai_rtc_facade.h` has no Agora/vendor/product peripheral leakage,
- Agora/vendor APIs stay in SDK private layer,
- datastream/RTM selector stays private,
- RTC porting contract and vendor baseline docs are present.

## 3. Product Baseline Verification

Run these only after SDK host verification passes.

ESP32S3 AI alarm:

- update the product SDK pointer or SDK vendor copy to the tagged baseline,
- build from the product's normal ESP-IDF environment,
- runtime check: first and repeated AI voice dialogs work,
- runtime check: no panic, allocator corruption, or RTC failed state.

xiaozhi-esp32:

- update the product SDK pointer to the tagged baseline,
- build from the product's normal ESP-IDF environment,
- runtime check: voice dialog works with default datastream mode,
- runtime check: video/RTM private support does not affect voice dialog.

Beken BK7258:

- build product with the intended Beken SDK version,
- confirm Agora/AOSL/mbedtls archive baseline has no link conflicts,
- runtime check: greeting, mic uplink, speaker downlink, and repeated dialog,
- runtime check: LCD/AVI workload does not starve audio/RTC hot path beyond
  product acceptance.

## 4. Beken SDK 3.1.1.8 A/B Checklist

Use the same product code and same RTC SDK baseline for each build:

1. old accepted Beken SDK build,
2. Beken SDK 3.1.1.8 baseline build,
3. Beken SDK 3.1.1.8 with visual workload reduced or disabled.

Compare:

- `RTC_FACADE_TX_DIAG` gap timing,
- mic PCM energy or equivalent capture diagnostic,
- speaker playback warnings,
- `AEL_IO_ABORT` occurrence and call path,
- AVI/JPEG/DMA2D/display warnings,
- subjective AI dialog smoothness.

Do not classify a Beken 3.1.1.8 regression as an RTC SDK facade regression until
mic capture, speaker playback, RTC TX/RX, and visual workload starvation have
been separated.

## 5. Stale SDK Removal Gate

Before deleting or renaming old SDK directories:

- record old SDK path and commit/hash if available,
- confirm the new GitHub repo can be cloned cleanly,
- confirm SDK host tests pass from the clean clone,
- confirm at least one product build succeeds from the clean clone,
- preserve any non-committed local notes or product patches.

Recommended stale directory handling:

```text
<old-sdk-name>.stale_<date>_<short-hash-or-reason>
```

Prefer rename/archive first; delete only after a clean clone and product runtime
validation are accepted.

## 6. Release Tag Checklist

After memo/checklist commit:

```bash
git status --short
git tag -a phase7-rtc-sdk-contract-baseline -m "Phase 7 RTC SDK contract baseline"
git show --stat --oneline phase7-rtc-sdk-contract-baseline
```

Expected result:

- only known unrelated dirty files remain,
- tag points at the memo/checklist commit,
- tag output includes the release docs.
