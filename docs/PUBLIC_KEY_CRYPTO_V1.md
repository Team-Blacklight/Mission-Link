# Mission Link V1 public-key and session-key architecture

**Status:** Design specification only. This document does not implement cryptography, establish device trust, or enable radio operation.

## Selected primitives

| Purpose | Primitive | Parameters | Status |
|---|---|---|---|
| End-to-end message encryption | AES-256-GCM | 256-bit key, 96-bit nonce, 128-bit tag | Selected; implementation pending |
| Key agreement | X25519 | 32-byte public and private key material | Proposed for V1 |
| Device identity signatures | Ed25519 | 32-byte public key, 64-byte signature | Proposed for V1 |
| Key derivation | HKDF-SHA-256 | SHA-256 extract/expand | Proposed for V1 |
| Transcript / context hashing | SHA-256 | 256-bit digest | Proposed for V1 |

These are complementary functions, not encryption layers. Do not encrypt AES ciphertext again with RSA or another cipher merely to stack algorithms. Public-key cryptography authenticates peers and establishes/derives symmetric session keys; AES-GCM protects message contents efficiently.

## Intended establishment flow

This is a protocol outline, not a wire-level handshake specification:

1. Each enrolled device has a long-term Ed25519 identity key pair. Its public identity key is distributed through an authenticated enrollment process and bound to a stable device identity.
2. For a new session, each side creates a fresh X25519 ephemeral key pair.
3. Peers exchange protocol-versioned handshake messages containing their device identifiers, ephemeral X25519 public keys, fresh handshake nonces, and the required suite identifiers.
4. Each peer signs the canonical handshake transcript with its Ed25519 identity private key. The transcript must bind both identities, both ephemeral public keys, roles/direction, protocol version, selected algorithms, and handshake nonces to prevent substitution and downgrade attacks.
5. Each side verifies the other device's Ed25519 signature against a previously authenticated/pinned identity public key, then computes the X25519 shared secret.
6. HKDF-SHA-256 derives distinct directional traffic keys and nonce namespaces from the shared secret and transcript hash. Never use the raw X25519 output directly as an AES key.
7. The peers confirm possession of the derived session keys with an authenticated key-confirmation step before treating the session as established.
8. Application messages use AES-256-GCM with the appropriate directional key and the reviewed packet AAD profile.

The final handshake must define exact byte encodings, transcript canonicalization, role separation, key-confirmation messages, timeouts, retransmission behavior, and state transitions before implementation.

## Identity and trust requirements

- A callsign or editable display name is not an identity credential.
- A public key received over an unauthenticated LoRa broadcast is not trusted merely because it was received.
- Define out-of-band enrollment / QR or physical-presence verification, trust-on-first-use policy (if any), key fingerprint display, revocation, replacement, and recovery before deployment.
- Bind the stable Mission Link device ID to the enrolled Ed25519 public key; reject conflicting bindings.
- Private identity keys must not be logged, exported to the TF card, or committed to source control. Assess ESP32-S3 secure boot, flash encryption, NVS protection, debug access, and physical extraction limitations for the exact deployment configuration.
- Identity-key rotation must have an authenticated transition and recovery procedure.

## Session and nonce lifecycle

X25519 ephemeral keys provide fresh key-agreement input, but they do not replace traffic-key nonce management. Each derived traffic key needs its own nonce namespace and persistent uniqueness guarantee. Use separate send and receive keys (or otherwise rigorously separated key/nonce domains); do not let both directions allocate counters in the same namespace.

The AES-GCM nonce allocator must remain safe across concurrent sends, reboot, power loss, flash restore, factory reset, and session/key rotation. A session must not be resumed with a counter state that could reuse a nonce. Define session expiry and rekey thresholds. If durable counter state cannot be trusted after restore, discard the affected traffic keys and establish a new authenticated session.

Replay protection is still required. Authenticate packets before exposing plaintext or committing accepted replay state. Define bounded windows, duplicate behavior, out-of-order delivery, and persistence/expiry.

## Group messaging

Do not assume a pairwise X25519 handshake automatically solves group key management. Group messaging needs a separately specified authenticated group-key distribution and membership-change protocol, including sender-specific key/nonce separation, member removal, key rotation, and offline members. Until that design and its tests exist, keep group-encryption implementation blocked; do not reuse one AES key with independent counters on multiple senders.

## RSA decision

RSA is not selected for V1. It has larger public keys and signatures than the selected elliptic-curve primitives and offers no useful advantage for this constrained LoRa packet budget. Do not add RSA as a second encryption layer or silently negotiate it as a fallback.

## Required validation before implementation is considered ready

- Select and pin a vetted ESP-IDF / mbedTLS version and verify support for X25519, Ed25519, HKDF-SHA-256, SHA-256, and AES-GCM on the actual build target.
- Use published test vectors for every primitive and the integrated key schedule.
- Test invalid signatures, altered transcript fields, identity/key mismatch, malformed public keys, and handshake role confusion.
- Test that distinct sessions and directions derive distinct traffic keys and nonce namespaces.
- Test replay, duplicate, out-of-order, timeout, retransmission, rekey, and key-revocation behavior.
- Test nonce persistence and fault cases; never treat randomness alone as a substitute for a defined uniqueness strategy.
- Review packet overhead and handshake fragmentation against the actual radio payload budget before any RF work.
- Complete authenticated enrollment and secure-storage threat review.

## Current safety boundary

This is documentation only. No private keys, provisioning secrets, radio settings, pin mappings, or transmission behavior are added. RF remains disabled until the project's hardware, Canadian regulatory, cryptographic, replay, and mesh-forwarding readiness gates are independently satisfied.
