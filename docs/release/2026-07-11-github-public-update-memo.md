# GitHub Public SDK Update Memo

This memo records how to update the public `ai_iot_sdk` GitHub repository
without damaging the full local development history.

## Repository Roles

Use two separate repositories:

- Full development SDK repo: keeps complete local history, internal notes, and
  working branches.
- Public snapshot repo: `/root/ai_iot_sdk_pub`, used only for the sanitized
  GitHub release snapshot.

Do not create an orphan branch inside the full SDK repo. Orphan branches replace
the visible working tree with a new empty history and can make the SDK directory
look empty until the previous branch is restored.

## Update Flow

From WSL, refresh the public snapshot from the full SDK repo:

```sh
rsync -a --delete \
  --exclude='.git/' \
  --exclude='.claude/' \
  --exclude='docs/superpowers/' \
  --exclude='docs/rtc-logging-diagnosability-remediation.md' \
  --exclude='docs/rtc-best-practices-context-prompt.md' \
  <full-sdk-dir>/ \
  /root/ai_iot_sdk_pub/
```

Keep public documentation files that are useful for developers:

- `README.md`
- `LICENSE`
- `NOTICE`
- `docs/porting-guide.md`
- `docs/rtc-facade-api.md`
- `docs/rtc-porting-contract.md`
- `docs/rtc-ai-dialog-and-emotion.md`
- `docs/vendor-binaries.md`
- `docs/portable-sdk-baseline.md`
- `docs/release/*.md`

Do not publish internal review/planning notes from `docs/superpowers/`.

## Pre-Push Checks

Run host/static checks:

```sh
make -C /root/ai_iot_sdk_pub/test/host test
make -C /root/ai_iot_sdk_pub/test/host clean
```

Check whitespace and accidental private paths:

```sh
git -C /root/ai_iot_sdk_pub diff --check

cd /root/ai_iot_sdk_pub
rg -n --fixed-strings \
  -e '<internal-linux-workspace-prefix>' \
  -e '<internal-wsl-windows-user-prefix>' \
  -e '<internal-windows-user-prefix>' \
  -e '<old-sdk-name>' \
  .
```

Replace the placeholders with the local private path prefixes before running
the scan. Check that no unexpected large generated files are included:

```sh
find /root/ai_iot_sdk_pub \
  -path /root/ai_iot_sdk_pub/.git -prune \
  -o -type f -size +20M -printf '%s %p\n'
```

If `rg` returns no matches, it exits with code `1`; that is acceptable for this
specific scan.

## Commit Strategy

The public repository should keep a clean summary history. For a snapshot-style
release, update the single public commit:

```sh
git -C /root/ai_iot_sdk_pub add -A
git -C /root/ai_iot_sdk_pub commit --amend -m "Initial public release"
```

If a normal incremental public history is preferred later, use a regular commit
instead:

```sh
git -C /root/ai_iot_sdk_pub add -A
git -C /root/ai_iot_sdk_pub commit -m "Update public SDK snapshot"
```

## Push Strategy

Check whether the remote already has a branch:

```sh
git -C /root/ai_iot_sdk_pub ls-remote --heads origin
```

If the remote is empty:

```sh
git -C /root/ai_iot_sdk_pub push -u origin main
```

If the remote already has the previous public snapshot and the local commit was
amended intentionally:

```sh
git -C /root/ai_iot_sdk_pub push --force-with-lease -u origin main
```

Use `--force-with-lease`, not plain `--force`, unless the remote repository was
just recreated and you intentionally want to replace everything.

## Current Baseline

The public GitHub repository was pushed successfully at:

```text
0489066 Initial public release
```

Remote:

```text
git@github.com:harold-2022-cloud/ai_iot_sdk.git
```

The full development SDK repo should remain on its normal development branch or
backup branch and should not be rewritten for public release.
