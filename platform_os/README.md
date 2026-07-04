# platform_os

ESP-IDF-backed OS/platform services shared by SDK and product components.

This component owns task, queue, semaphore, mutex, timer, logging, network,
atomic, synchronization, ISR, DMA, and SPSC primitives. Public header names
remain unchanged so legacy callers can continue including `bsp_system.h`,
`bsp_log.h`, and related headers through `platform_adapter`.

Migration stages:

- B3b-2: `platform_adapter` publicly depends on `platform_os`; existing callers
  remain unchanged.
- B3b-4: SDK/core consumers may change their direct dependency from
  `platform_adapter` to `platform_os`.

Peripheral and board-specific implementations do not belong in this component.
