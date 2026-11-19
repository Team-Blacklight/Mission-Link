# Radio bring-up checklist

This checklist tracks the work required before MissionLink may use the SX1262 radio. It intentionally does not guess GPIO assignments or enable RF.

## Gate 0 — identify the exact hardware

- [ ] Record the exact XIAO ESP32-S3 board revision.
- [ ] Record the exact Wio-SX1262 module revision and connector arrangement.
- [ ] Verify the official pin map for the exact combination: SPI SCK/MISO/MOSI, chip select, reset, busy, DIO1/interrupt, and RF switch controls if present.
- [ ] Confirm voltage levels and power requirements from the hardware documentation.
- [ ] Confirm the antenna is the correct type and securely attached before any authorized RF testing.
- [ ] Confirm applicable Canadian radio requirements, permitted frequency/channel, bandwidth, spreading factor, output power, and duty-cycle constraints.

**Do not copy pin numbers from an unrelated SX1262 board.** Board variants can wire radio control pins differently.

## Gate 1 — compile-only integration

- [ ] Add a board-specific radio configuration header with verified pin assignments and source references.
- [ ] Keep `ML_RF_ENABLED` set to `0` by default.
- [ ] Add a compile-only RadioLib adapter behind the radio feature flag.
- [ ] Ensure the default build has no code path that initializes or transmits with the radio.
- [ ] Add host-side tests for packet framing and radio-independent state transitions.

A successful compile proves only that the code builds for the selected toolchain; it does not validate wiring or RF operation.

## Gate 2 — receive-only bench validation

Only proceed after the exact pinout and regional configuration have been independently verified.

- [ ] Initialize the SX1262 and report a structured status/error code.
- [ ] Read the radio chip/version information if supported by the module.
- [ ] Configure a compliant receive channel and verify that receive mode starts.
- [ ] Test reception using a known compatible, authorized test transmitter.
- [ ] Record configuration and results; do not claim a distance guarantee.

## Gate 3 — controlled packet test

- [ ] Review the complete over-the-air frame size, including nonce, authentication tag, and radio overhead.
- [ ] Implement authenticated encryption and replay protection before transmitting user messages.
- [ ] Add message IDs, duplicate suppression, acknowledgements, and timeout/retry behavior.
- [ ] Begin with a controlled point-to-point test under a compliant configuration.
- [ ] Test malformed packets and loss/retry cases before testing relays.
- [ ] Keep relay forwarding disabled until hop-limit enforcement and duplicate suppression are validated.

## Current status

The SX1262 RadioLib adapter exists, including receive and transmit methods, but the default build is intentionally inert:
- `ML_RADIO_PINMAP_VERIFIED=0`
- `ML_RADIO_RX_ENABLED=0`
- `ML_RF_ENABLED=0`

No radio initialization, receive mode, or transmission should occur in the default build. The previous hard-coded pin/frequency/modulation/power values were removed from PlatformIO defaults because their verification against the exact physical hardware and Canadian authorization had not been demonstrated.

Production authenticated encryption, nonce lifecycle, replay protection, key provisioning, and physical hardware validation remain incomplete. Mesh forwarding must remain blocked until the authenticated packet design and its tests are complete.

See [RF enablement readiness](RF_ENABLEMENT_READINESS.md) for the evidence required before any future enablement review.
