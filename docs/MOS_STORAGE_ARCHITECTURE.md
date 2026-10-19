# mOS storage architecture

## Purpose

Mission Link's mOS should keep the minimum system needed to boot and provide basic networking on internal flash, while using the round display's TF/microSD slot for user data and larger optional assets. A missing or damaged card must not make the device unbootable.

This is an architecture decision, not a claim that card support is already implemented. The exact round-display model, SD interface, chip-select pin, and available flash/partition sizes must be confirmed before enabling a hardware driver.

## Storage tiers

### Internal flash: boot-critical

Keep these components on internal flash:

- Bootloader, partition table, firmware image, and recovery/update path.
- Core mOS startup, display/input integration, and watchdog/error recovery.
- LoRa driver and radio/network state machine.
- Packet codec, mesh routing, and protocol validation.
- Security primitives, identity/key-handling code, and essential defaults.
- A small configuration record needed to start safely when the card is absent.

Use the board's supported partition layout. Preserve space for OTA/recovery only if the chosen partition scheme supports it; do not assume all flash capacity is available to the application. Never store private keys as ordinary plaintext files on the card.

### TF/microSD: user and bulk data

Use the card for data that can be recreated or that should survive normal firmware updates:

- Contact book and friendly labels for Mesh Numbers such as `(777)-XXX-XXXX`.
- Message history and delivery receipts once the messaging layer exists.
- Bounded diagnostics and event logs.
- User preferences, themes, and optional UI assets.
- Optional offline maps or other large datasets, if later supported.

Suggested logical layout:

```text
/mos/
  config/
    preferences.json
  contacts/
    contacts.json
  messages/
    inbox/
    outbox/
  logs/
    events.log
  assets/
```

This is a proposed layout, not a format that current firmware reads. Key material should not be placed in these files. File permissions on embedded FAT filesystems are not a substitute for encryption.

## Reliability rules

1. Boot and provide a minimal status screen with no card inserted.
2. Mount the card as an optional service; report `absent`, `ready`, or `error` distinctly.
3. Never erase or format a card automatically to recover from a mount error.
4. Bound file sizes, contact counts, message sizes, and log growth.
5. For structured files, validate schema/version and parse into a temporary object before replacing in-memory state.
6. Prefer write-to-temporary-file, flush/close, then rename where the filesystem supports the operation; preserve the previous valid file until the replacement is complete.
7. Treat interrupted writes and corrupt data as recoverable errors. Keep a backup or journal for important contact-book updates.
8. Batch log writes and avoid writing flash/card on every UI redraw.
9. Keep firmware, radio initialization, and safe local UI operation independent of card availability.
10. Do not report a message as delivered merely because it was written to local storage; delivery requires a protocol-level acknowledgement.

## Implementation sequence

1. Identify the exact round-display board and its SD wiring; verify the card interface and library support.
2. Add a storage manager with explicit mount state and a small internal-flash settings store.
3. Implement versioned, bounded contact-book persistence with atomic replacement and tests.
4. Add message history/outbox only after message IDs, ACK semantics, retry policy, and encryption boundaries are specified.
5. Test card removal, full card, corrupt files, power loss during writes, and no-card boot on real hardware.

## Current status

The firmware now stores the selected client/repeater/gateway preset in ESP32 NVS (internal flash) and restores it at boot. If NVS is unavailable, the firmware reports a warning and preset changes are runtime-only; a failed NVS write prevents changing the active preset. This is the first small internal-flash setting, not a complete OS settings service.

The firmware now also has a bounded, hardware-independent contact-book core in `include/mos_contact_book.h` / `src/mos_contact_book.cpp`. It supports up to 64 contacts, fixed 8-byte node IDs, printable ASCII labels up to 32 bytes, lookup, update, and deletion. It rejects zero IDs, invalid labels, and additions beyond capacity. This is an in-memory data model only; it does not yet serialize or persist contacts.

The firmware also has a small hardware-independent storage-safety contract in `include/mos_storage.h` / `src/mos_storage.cpp`: it distinguishes an absent card from a mount error, rejects unsafe relative paths (absolute paths, traversal segments, repeated separators, control characters, Windows separators/drive prefixes, and overlong paths), and provides a size-limit check for callers to apply before reading files. Native tests cover these rules. This is deliberately not a filesystem driver: it does not mount the TF/microSD card, read/write contacts, or persist messages. The exact round-display model and SD wiring remain required before implementing the hardware adapter. The repository also has a local host-side contact book, but it is not yet persisted on the device. LoRa transmission remains disabled.
