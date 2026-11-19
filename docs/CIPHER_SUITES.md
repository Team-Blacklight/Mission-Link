# MissionLink cipher suites and glyph transport

## Goals

MissionLink separates cryptographic protection from how encrypted bytes are represented. A cipher suite is security-sensitive; a glyph view is only a presentation/obfuscation feature.

## Cipher suite registry

| ID | Name | Intended use | Status |
|---|---|---|---|
| 0x01 | AES-256-GCM | **Required V1 production suite** | Design selected; implementation pending |
| 0x02 | ChaCha20-Poly1305 | Possible future alternative | Not enabled |
| 0x80 | Classical-cipher demo | Educational Caesar/Vigenere demonstrations only | Optional future demo; never for private traffic |

V1 selects **AES-256-GCM only**. Do not implement algorithm negotiation or fallback in the first version. Peers with unsupported suite IDs must reject the packet. Do not implement a home-grown cipher or accept unauthenticated plaintext as a fallback.

## AES-256-GCM V1 profile

- AES key: exactly 256 bits (32 bytes).
- Nonce/IV: exactly 96 bits (12 bytes).
- Authentication tag: exactly 128 bits (16 bytes); do not truncate.
- Plaintext: UTF-8 message bytes, subject to the final frame-size budget.
- Output: nonce || ciphertext || tag in the encrypted payload envelope.
- Implementation: vetted ESP-IDF mbedTLS AES-GCM API on the ESP32-S3. Do not write AES, GHASH, or GCM primitives ourselves.
- Decryption must verify the tag before returning any plaintext. On any error, clear/reject the output and expose no partial plaintext.

### Nonce uniqueness — hard requirement

A nonce MUST NEVER repeat under the same AES key. A random 96-bit nonce alone is not accepted as the V1 uniqueness strategy.

Before implementation, select and test a persistent allocator. Candidate architecture: a sender-specific key (or independently derived sender key) plus a 96-bit nonce made from a provisioned per-key 32-bit prefix and a persistent 64-bit monotonic counter. Reserve counter ranges durably before use so power loss cannot roll the counter backward into a previously used range. Counter exhaustion requires key rotation. The prefix must be unique for every key; it is not a node ID unless the provisioning system guarantees uniqueness and binds it to that key.

A shared group key complicates uniqueness across senders. V1 must either provision independent sender encryption keys with an authenticated group-key hierarchy, or use a rigorously specified globally unique sender-prefix allocation. Do not ship group encryption until this is resolved. Reboots, factory reset, flash erase/restore, key rotation, and concurrent send paths must all be covered by tests.

### Associated data and relay compatibility

Authenticate immutable packet context as GCM additional authenticated data (AAD): protocol version, cipher suite ID, packet type, address mode, source ID, destination/group ID, and message ID. The authenticated sender ID must be cryptographically bound to the key identity; a callsign is display-only.

Hop count and other fields that a relay must mutate cannot be included directly in end-to-end AAD. Keep mutable forwarding metadata in a separately validated outer envelope, or add a separately authenticated hop layer only after its key lifecycle is designed. Relays forward the end-to-end ciphertext unchanged and never receive its message key.

### Replay protection

Nonce uniqueness prevents nonce reuse by the sender; it does not alone prevent replay. Receivers must maintain bounded per-sender replay state and reject duplicate message IDs/nonces before delivering plaintext. Define persistence/expiry and out-of-order behavior before implementation. Authentication must succeed before replay state is committed as accepted.

### Key lifecycle

Before production use, define secure provisioning, per-device identity binding, direct and group key derivation, rotation, revocation, recovery, factory reset, and lost-device handling. Keys must not be printed, logged, committed, or stored as plaintext on the TF card. Use the platform's secure storage capabilities where available and document residual physical-access risks.

## Required security rules

- Use a vetted platform crypto implementation (ESP-IDF mbedTLS).
- Never reuse a nonce with the same key.
- Verify authentication before exposing plaintext.
- Reject duplicate/replayed messages within a defined replay policy.
- Keep private-message keys separate from group keys.
- Do not put keys or plaintext secrets in logs, test fixtures, or source control.
- Never silently downgrade or accept plaintext as a fallback.

## Defense-in-depth layers

1. **End-to-end message layer:** encrypt/authenticate for the destination or group; ciphertext remains protected across relays.
2. **Optional hop-security layer:** separately authenticate/encrypt outer hop frames only after link-key provisioning and relay trust are documented.
3. **Replay protection and identity:** nonce allocation, authenticated sender identity, and duplicate detection.
4. **Transport representation:** compact binary by default; optional glyph display is not cryptography.

Do not blindly stack AES, ChaCha, or other ciphers. These are alternative suites, not sequential layers.

## Wingdings-style transport mode

LoRa carries bytes, not font glyphs. Wingdings is a font mapping, not encryption. A future glyph mode may encode already-encrypted bytes using a documented stable symbol-ID alphabet, but security comes entirely from AES-256-GCM.

Default transport remains compact binary. Glyph mode requires an explicit encoding ID, strict size limits, round-trip tests, and fragmentation/reassembly rules. Unknown encoding IDs must be rejected.

## Implementation gates

Do not enable RF until all of the following exist and are tested:

- AES-256-GCM encrypt/decrypt implementation using a vetted library.
- Known-answer test vectors, tamper tests, invalid-tag tests, and boundary tests.
- Proven nonce uniqueness across concurrent sends, reboot, power loss, restore, and key rotation.
- Reviewed AAD/header design compatible with relays.
- Replay protection and authenticated sender identity.
- Key provisioning and direct/group key lifecycle.
- Final packet-size budget including 12-byte nonce and 16-byte tag.
- Verified pin mapping, radio configuration, and applicable Canadian radio compliance.

**Status:** AES-256-GCM is selected for V1 design. Cryptography is not yet implemented, and no RF messaging is enabled.
