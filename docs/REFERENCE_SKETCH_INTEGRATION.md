# Reference serial-chat sketch integration notes

The supplied XIAO ESP32-S3 + top-mounted Wio-SX1262 sketch is a useful **interaction and hardware-reference sample** for Mission Link V1. It is not copied as the Mission Link radio implementation.

## What we can reuse

- The intended physical target is the XIAO ESP32-S3 with the Wio-SX1262 B2B expansion board mounted on top.
- The sketch's candidate signal mapping is recorded in `include/missionlink_board_xiao_esp32s3_wio_sx1262_b2b.h` for review only: SCK 7, MISO 8, MOSI 9, NSS 41, RESET 42, BUSY 40, DIO1 39.
- The non-blocking serial line-input pattern is a good direction for the local console.
- Call-sign-style sender labels and a compact command menu are useful UX ideas.
- Packet counters and RSSI/SNR belong in the eventual diagnostics/status interface.

## What must not be copied into V1

- **The Vigenere-style printable-ASCII transform is not secure encryption.** It is deterministic, does not authenticate messages, and does not prevent replay or tampering.
- The sample prints the encryption seed in both startup output and `@stats`. Mission Link must never print keys, passphrases, or derived key material.
- A shared text seed is not a key-management design. V1 needs authenticated encryption, unique nonces, replay protection, sender authentication, and a documented provisioning/rotation/recovery lifecycle before user messages are sent.
- The sample's 915 MHz channel plan and 0.1 MHz channel increments are not adopted. Its bandwidth, spreading factor, coding rate, sync word, and 22 dBm output-power choice are also not adopted. Regional authorization and the exact hardware's operating conditions must be reviewed first.
- The candidate GPIO numbers remain unverified until checked against the schematic matching the physical Wio-SX1262 revision.
- Do not copy the sample's immediate radio initialization or continuous receive startup into the default firmware.

## Mission Link command mapping

The current firmware exposes a newline-delimited serial/ BLE JSON command interface. Its implemented commands are:

| Sample sketch idea | Current Mission Link state |
| --- | --- |
| `@help` | Use `help`; alias not yet implemented |
| `@stats` | Use `status`; reports actual radio state and RF-disabled state |
| `@name <name>` | Not implemented; sender identity is currently the node ID |
| `@<number>` | Not implemented; no user-selectable RF channel plan is configured |
| `@encrypt on/off` | Not implemented; no insecure fallback cipher will be added |
| `@seed <text>` | Deliberately unsupported; secrets must not be echoed or stored as plaintext |
| Plain text message | Direct/group send is currently rejected as not ready until the authenticated message path exists |

The existing command interface is documented in the repository README and client protocol documentation. This note records the compatibility target without claiming unsupported commands work.

## Planned integration order

1. Keep the current serial and BLE command framing bounded and non-blocking.
2. Add a presentation-layer callsign/contact label that remains separate from stable node identity.
3. Add message composition and delivery only after the authenticated packet envelope and key lifecycle are implemented.
4. Add channel selection only after a regionally compliant, hardware-verified radio profile exists.
5. Add TX/RX diagnostics from actual adapter counters and measured radio state; never fabricate values.
6. Keep the default build radio-inert until the hardware, regulatory, security, and maintainer-approval gates are complete.

## Safety and validation status

- `ML_RADIO_PINMAP_VERIFIED=0`.
- `ML_RADIO_RX_ENABLED=0`.
- `ML_RF_ENABLED=0`.
- No radio initialization, receive, transmit, or over-the-air test is enabled by this integration note.
- This document is a design/compatibility review; it does not claim a firmware build or hardware test.
