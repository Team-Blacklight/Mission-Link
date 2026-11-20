# Mission Link cryptography implementation readiness audit

**Audit date:** 2026-11-20  
**Scope:** repository build configuration and the proposed AES-256-GCM / ECC architecture.  
**Result:** implementation is not ready to begin safely; resolve the dependency and protocol decisions below first.

## What the repository currently establishes

- The ESP32-S3 firmware uses PlatformIO with the Arduino framework (board = seeed_xiao_esp32s3).
- The espressif32 PlatformIO platform is currently unpinned in platformio.ini.
- The firmware environment does not declare an explicit cryptography dependency. Arduino-ESP32 commonly exposes ESP-IDF / mbedTLS components, but the exact version and enabled algorithms must be established from a reproducible build.
- The native test environment is a separate C++17 build and currently does not compile a crypto provider or mbedTLS-backed code.
- ML_RF_ENABLED is hard-coded to zero, while the PlatformIO radio pin-map and RX flags are also disabled. Preserve this fail-closed configuration.

## Why implementation is blocked

A design that names AES-GCM, X25519, Ed25519 and HKDF is not enough to select safe API calls. The exact Arduino-ESP32 / ESP-IDF / mbedTLS versions, compile-time feature configuration, supported curve operations, and error semantics must be known. Do not assume all algorithms are available just because an mbedTLS header exists.

The current espressif32 floating platform can change the bundled Arduino core and crypto library between builds. Pinning a platform version without first checking the project's CI toolchain and compatibility would also be an unsupported guess.

## Required dependency audit

1. Record the exact PlatformIO Core version and resolved espressif32 platform version used by CI.
2. Record the Arduino-ESP32 core, ESP-IDF, and mbedTLS versions from that resolved toolchain.
3. Confirm the build configuration provides AES-GCM, SHA-256, HKDF, X25519, and Ed25519 (including required modules and curve support).
4. Confirm the documented APIs and their failure/cleanup semantics from the exact bundled headers and upstream version documentation.
5. Pin the PlatformIO platform and any crypto dependency to reproducible versions only after a successful baseline build.
6. Add CI checks that fail if the expected crypto modules are absent; do not silently fall back to a different implementation.

If the bundled mbedTLS does not provide a required primitive, evaluate a maintained, vetted compatible library and its ESP32-S3 footprint before selecting it. Do not write cryptographic primitives or substitute an unreviewed implementation.

## Recommended implementation sequence

### Stage A — provider and test boundary (no protocol or RF integration)
- Define a narrow crypto-provider interface with fixed-size types and explicit status codes.
- Keep private key material and plaintext out of logs and error strings.
- Add native tests for pure serialization/validation helpers only; do not pretend native tests validate ESP32 cryptographic operations.
- Add ESP32-S3 known-answer tests using the exact provider and bundled library.

### Stage B — primitive adapters
- AES-256-GCM: known-answer vector, round trip, AAD tampering, ciphertext/tag tampering, malformed lengths, and no plaintext release on authentication failure.
- HKDF-SHA-256: published vectors and distinct context/direction output checks.
- X25519: published vectors, invalid/all-zero shared-secret handling, and ephemeral-key lifecycle.
- Ed25519: published vectors, invalid signature rejection, and identity-key binding tests.
- Clear temporary key/secret buffers on all exit paths where the platform API permits.

### Stage C — protocol security state
- Freeze canonical handshake encoding and transcript fields before implementing the handshake.
- Complete authenticated enrollment and key-fingerprint verification.
- Specify key confirmation, timeout/retry behavior, session expiry and rekey.
- Resolve directional nonce namespaces and persistent uniqueness across reboot, power loss, restore, concurrent sends, and factory reset.
- Specify replay windows and commit replay state only after successful authentication.
- Keep group key distribution blocked until sender-specific nonce/key separation and member removal are specified.

### Stage D — integration, still radio-inert
- Integrate only with the application message path behind a disabled-by-default feature flag.
- Ensure the radio adapter cannot receive or transmit as a side effect of crypto tests.
- Test encrypted packet sizing against the actual protocol/frame budget and explicit fragmentation rules.
- Require review of all cryptographic and protocol changes before any hardware RF work.

## Acceptance criteria for the first implementation PR

- Reproducible, pinned toolchain with recorded ESP-IDF and mbedTLS versions.
- Verified availability of all selected primitives on the ESP32-S3 target.
- Vetted provider APIs only; no home-grown primitives.
- Known-answer and tamper tests for each implemented primitive.
- No keys, secrets, or plaintext in logs, source control, or removable storage.
- No firmware messaging path marked ready until enrollment, key lifecycle, nonce uniqueness, and replay protection are implemented and tested.
- Default RF pin-map, RX, and TX settings remain disabled.

## Current validation status

This audit is based on source inspection of platformio.ini, include/missionlink_config.h, src/main.cpp, and src/missionlink_radio.cpp. No PlatformIO build, toolchain resolution, crypto API probe, unit test, or hardware test was run during this audit. CI has not reported check runs for the inspected PR head at audit time.

**Safety:** documentation only. No cryptographic implementation, dependency change, key material, radio configuration, receive path, or transmit path is added.