# RF enablement readiness

## Current decision

**RF remains disabled. This branch prepares the firmware for a future review; it does not authorize or perform radio operation.**

The default PlatformIO build is intentionally compile-only:
- `ML_RADIO_PINMAP_VERIFIED=0`
- `ML_RADIO_RX_ENABLED=0`
- `ML_RF_ENABLED=0` in `include/missionlink_config.h`
- No board-specific GPIO or regional RF parameters are supplied by the default build.
- `MissionLinkRadio::beginReceive()` must return without initializing the SX1262 while the pin map is unverified.

This is deliberately stricter than merely blocking transmit: neither receive nor transmit should start in the default build.

## Readiness gates

### A. Hardware identity — not yet evidenced

Record and independently verify:
- Exact XIAO ESP32-S3 board revision.
- Exact Wio-SX1262 module/kit revision and connector arrangement.
- Official manufacturer pin mapping for that exact combination, including SPI and radio control/switch signals.
- Electrical and power requirements.
- Correct antenna and connector for the module.

Do not infer a pin map from a similar-looking board or from old build flags. Store the source document/revision and reviewer in the hardware test record.

### B. Canadian configuration review — not complete

Before any over-the-air test, identify the applicable ISED Canada equipment authorization and operating conditions for the exact module and intended use. Have the permitted band, channelization, bandwidth, output limits, antenna conditions, and duty-cycle/airtime restrictions checked against the applicable authorization and current rules.

The old 915 MHz / modulation / power values were not evidence of compliance and have been removed from the default build. This document intentionally does not prescribe operating values.

### C. Protocol security — blocking

The current packet codec and routing metadata are not a secure messaging protocol. Before user traffic can be sent over RF, implement and test:
- A vetted authenticated-encryption implementation; no home-grown cipher and no plaintext fallback.
- A nonce strategy that prevents reuse under a key, including after reset or loss of power.
- Authenticated sender identity and immutable packet-header fields.
- Replay-window enforcement and duplicate handling.
- Direct and group key provisioning, rotation, revocation, and recovery.
- A relay-compatible authentication design so mutable hop metadata cannot silently invalidate end-to-end protection.
- Bounded frame sizing that accounts for all cryptographic overhead.

Do not treat BLE pairing, a sync word, a node ID, CRC, or glyph encoding as message encryption/authentication.

### D. Radio adapter verification — not complete

After A and B are evidenced, review the adapter against the exact hardware documentation and selected RadioLib version. Compile-only success is not hardware validation. Bench validation must be separately authorized and recorded; no range or reliability claim follows from initialization alone.

### E. Mesh forwarding — blocked

Keep forwarding disabled until authenticated packets, hop-limit enforcement, duplicate suppression, route validation, and failure behavior have tests that exercise malformed, replayed, duplicated, and expired traffic. The current mesh simulator is not proof of over-the-air interoperability.

## Required evidence before changing any gate

A future RF-enablement change must include:
1. Hardware identification and official pinout reference.
2. A reviewed Canadian compliance/configuration record for the exact radio module.
3. Security design and passing cryptographic/replay test vectors.
4. Build and native-test results for the exact commit.
5. A separate, explicit authorization for the specific hardware test stage.

No secrets, production keys, or private identifiers belong in Git. Keep RF disabled in ordinary CI and release artifacts until the project owner explicitly approves a separately reviewed enablement change.

## Present implementation facts

- The SX1262 RadioLib adapter exists in `src/missionlink_radio.cpp`.
- The adapter contains receive and transmit methods, but the default configuration must not initialize the chip or transmit.
- `ML_RF_ENABLED` remains hard-coded to zero in `include/missionlink_config.h`.
- Production authenticated encryption and key lifecycle are not implemented.
- No physical hardware validation or over-the-air test is claimed.
