# AES-256-GCM V1 packet profile

**Selection:** AES-256-GCM is the sole planned V1 production cipher suite. This is a design specification, not an implementation or claim of production security.

## Wire envelope

The encrypted payload is encoded as:

| Field | Size |
|---|---:|
| Nonce | 12 bytes |
| Ciphertext | variable |
| Authentication tag | 16 bytes |

The tag is always 128 bits; do not truncate it. The nonce is 96 bits. The AES key is exactly 256 bits. The nonce is transmitted with the ciphertext and tag, but must never repeat under the same key.

The current packet codec allows at most 180 payload bytes. With the 12-byte nonce and 16-byte tag, that leaves at most 152 bytes of plaintext if the existing ceiling is retained. This is only a codec calculation; actual LoRa frame budget, radio overhead, and any fragmentation scheme still need a separate review. Oversized messages must be rejected or explicitly fragmented, never silently truncated.

## Authenticated associated data

The intended end-to-end AAD contains immutable fields: protocol version, cipher suite ID, packet type, address mode, source ID, destination/group ID, and message ID.

Hop count is mutable during forwarding and therefore cannot simply be included in end-to-end AAD. Keep forwarding metadata in a separately validated outer envelope, or design a separate hop-authentication layer. Relays must forward the inner ciphertext unchanged and must not need its decryption key.

A callsign is presentation metadata only. Authenticated sender identity must be bound to the cryptographic key, not trusted from a user-editable name.

## Nonce allocation — unresolved implementation gate

A random nonce by itself is not accepted as the V1 uniqueness strategy. The implementation must use a persistent allocator and prove uniqueness across concurrent sends, reboot, power loss, flash restore, factory reset, and key rotation.

Candidate design for review: a per-sender key (or independently derived sender key), a provisioned unique 32-bit nonce prefix per key, and a persistent 64-bit monotonic counter. Reserve counter ranges durably before using them so an interrupted boot cannot reuse a range. Counter exhaustion requires key rotation. The prefix must be unique for each key.

Do not encrypt multiple senders' traffic with one shared group key and independently initialized counters. Group messaging requires a rigorously specified unique per-sender nonce namespace or sender-specific derived keys. This decision must be made before group encryption is implemented.

## Replay protection

Nonce uniqueness does not prevent an attacker from replaying a previously valid packet. Receivers need bounded per-sender replay state, duplicate rejection, defined out-of-order behavior, and a persistence/expiry policy. Authenticate first; only then commit replay state and expose plaintext.

## Key lifecycle

Before production use, define secure provisioning, device identity binding, direct and group key derivation, rotation, revocation, recovery, factory reset, and lost-device handling. Never log or print keys. Do not store keys as plaintext on the removable TF card or commit keys to source control. Document the platform's secure-storage capabilities and physical-access limitations.

## Implementation and tests

Use the vetted ESP-IDF mbedTLS AES-GCM implementation on ESP32-S3; do not write AES/GHASH/GCM primitives. Tests must include known-answer vectors, successful round trips, modified ciphertext/AAD/tag rejection, malformed and boundary inputs, no plaintext on authentication failure, concurrent nonce allocation, and persistence/reboot fault cases.

## Current status

- AES-256-GCM selected for V1 design.
- Firmware encryption/decryption not implemented by this document.
- Nonce allocator, sender/group key architecture, replay policy, and key lifecycle still require design and tests.
- RF remains disabled. No radio initialization or over-the-air messaging is enabled.
