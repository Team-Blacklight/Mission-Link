### 2026-10-16 — Reusable local contact resolver
- **Owner:** ChatGPT
- **Status:** implementation committed on `feat/contact-resolver-interface`; pull-request CI not yet confirmed.
- **Changed paths:** `host/missionlink_contact_resolver.py`, `host/tests/test_missionlink_contact_resolver.py`, `docs/CONTACT_RESOLVER.md`.
- **Implemented:** read-only resolver facade over the contact book; exact Mesh Number lookup; case-insensitive exact label lookup; explicit matched/not-found/ambiguous results; deterministic substring search; validated file loading. Duplicate labels never silently select an arbitrary node.
- **Validation:** added seven unit tests for exact number, label matching, missing results, duplicate-label ambiguity, stable search, invalid queries, and file loading. CI for this branch has not yet been observed. No physical hardware or RF behavior tested.
- **Limits:** lookup only; no node discovery, trust verification, service authorization, or message delivery. RF remains disabled.
- **Next:** open PR and verify the complete host suite plus ESP32-S3 build in GitHub Actions.

---

### 2026-10-15 — Add hardware-independent mOS storage guards
- **Owner:** ChatGPT
- **Status:** implementation committed on `docs/mos-storage-layout`; CI pending.
- **Changed paths:** `include/mos_storage.h`, `src/mos_storage.cpp`, `test/test_mos_storage/test_mos_storage.cpp`, `platformio.ini`, `docs/MOS_STORAGE_ARCHITECTURE.md`.
- **Implemented:** a portable removable-storage state classifier (`absent` vs `ready` vs `error`), strict relative-path validation (including traversal and length bounds), and a size-limit helper. Added native tests and included the new source in the native PlatformIO test build.
- **Validation:** tests are committed but CI has not yet run on this new head. No filesystem mount, TF-card I/O, contact/message persistence, or physical-board test is claimed.
- **Safety/limits:** no SD pins or card interface guessed; no auto-formatting; RF remains disabled.
- **Next:** verify CI; once the round-display model and SD wiring are known, add the hardware mount adapter and then bounded contact persistence.

---

### 2026-10-15 — Correct preset persistence detection
- **Owner:** ChatGPT
- **Status:** committed on `docs/mos-storage-layout`; CI pending.
- **Changed paths:** `src/main.cpp`.
- **Fix:** ESP32 NVS reads with a default value can make an absent `preset` key look saved. Firmware now checks `isKey("preset")` before skipping writes and before reporting `presetPersisted:true`. This ensures choosing the default Client preset actually creates a saved record.
- **Validation:** source-level review; CI pending and no physical hardware test.
- **Next:** verify CI and continue storage implementation after board-specific SD wiring is known.

---

### 2026-10-15 — Expose internal settings health
- **Owner:** ChatGPT
- **Status:** committed on `docs/mos-storage-layout`; CI triggered for this change.
- **Changed paths:** `src/main.cpp`, `docs/CLIENT_PROTOCOL.md`.
- **Implemented:** `status` now includes `settingsReady` (whether NVS initialized) and `presetPersisted` (whether the active preset matches the saved value), so a client can diagnose flash-setting persistence without trying a write.
- **Validation:** source and protocol example updated together; awaiting ESP32-S3 CI. No physical board test.
- **Limits:** reports NVS/preset status only; it does not claim TF/microSD or contact/message storage is available. RF remains disabled.
- **Next:** confirm CI, then continue with storage work only where hardware details are verified.

---

### 2026-10-15 — Document persistent preset response
- **Owner:** ChatGPT
- **Status:** committed on `docs/mos-storage-layout`; CI is running for the branch head.
- **Changed paths:** `docs/CLIENT_PROTOCOL.md`.
- **Implemented:** protocol docs now describe NVS-backed preset restore, runtime-only fallback, write-failure behavior, and the `persisted` response field.
- **Validation:** documentation reviewed against current firmware source; no physical device test.
- **Next action:** confirm the ESP32-S3 build and native tests on the final PR head.

---

### 2026-10-15 — Persist node preset in internal flash
- **Owner:** ChatGPT
- **Status:** implementation committed on `docs/mos-storage-layout`; CI not yet verified for this change.
- **Changed paths:** `src/main.cpp`, `docs/MOS_STORAGE_ARCHITECTURE.md`.
- **Implemented:** firmware uses ESP32 `Preferences`/NVS to restore the selected client/repeater/gateway preset at boot and persist future changes. The preset response reports whether it was persisted. If NVS cannot initialize, preset changes are runtime-only with a warning; a failed write prevents applying the new preset. Re-selecting the already-saved preset skips a redundant NVS write to reduce flash wear.
- **Validation:** source-level review only so far. ESP32-S3 compilation and firmware tests must be checked in GitHub Actions; no physical board test performed.
- **Limits:** only this small setting is persisted internally. No TF/microSD driver, contact/message storage, or key storage is implemented. RF remains disabled.
- **Next action:** verify ESP32-S3 CI; then implement card support only after the exact round-display model and SD wiring are confirmed.

---

### 2026-10-15 — mOS internal-flash and TF/microSD storage architecture
- **Owner:** ChatGPT
- **Status:** design note committed on `docs/mos-storage-layout`; implementation not yet started.
- **Changed paths:** `docs/MOS_STORAGE_ARCHITECTURE.md`, `docs/AI_STATUS.md`.
- **Decision:** boot-critical mOS firmware, LoRa stack, mesh/router, protocol validation, security code, and minimum recovery configuration belong on internal flash. Contacts, message history, receipts, logs, preferences, and optional large assets belong on TF/microSD.
- **Reliability requirements:** device must boot without a card; no auto-formatting; bounded files; versioned schema validation; recoverable writes; avoid high-frequency writes; never store private keys as plaintext card files.
- **Validation:** documentation-only; no firmware build, SD-card driver, board wiring, or physical card test performed.
- **Blocker before implementation:** identify the exact round-display model and SD interface/pin mapping, then confirm the actual flash partition layout. Do not guess chip-select pins or claim SD support is implemented.
- **Next action:** implement a storage manager after board wiring is confirmed, then add tested contact persistence before message history.

---

### 2026-10-15 — Machine-readable contact lookup
- **Owner:** ChatGPT
- **Status:** committed on `feat/local-contact-book`; CI status for this latest change must be checked separately.
- **Commits:** [CLI JSON lookup](https://github.com/Innygr/Mission-Link/commit/d28b9edeedc196e44bc36a6854f14ebb2a5385ea), [lookup tests](https://github.com/Innygr/Mission-Link/commit/7f953a931b459a98989897ee8518d6f08d8d1399), [CLI docs](https://github.com/Innygr/Mission-Link/commit/d7736b6af293ad27bbe97c5f791959df1cf27b7f).
- **Changed paths:** `host/missionlink_contacts_cli.py`, `host/tests/test_missionlink_contacts_cli.py`, `docs/LOCAL_CONTACT_BOOK.md`.
- **Implemented:** `show '(777)-XXX-XXXX'` returns one compact JSON contact record on stdout with stable keys; missing contacts return exit code 1 and an error on stderr without corrupting stdout. Added tests for JSON shape and missing-contact behavior.
- **Validation:** preceding contact-book head passed the full GitHub Actions workflow [#38763226738](https://github.com/Innygr/Mission-Link/actions/runs/38763226738), including host/native tests and ESP32-S3 build plus flash-set validation. That run predates this JSON lookup change; check CI for the latest commit before claiming the new tests passed. No physical hardware or RF test performed.
- **Limits:** this is a local lookup interface for scripts/future clients, not a messaging API, network endpoint, node discovery feature, or radio transport. RF remains disabled.
- **Next:** verify CI on the latest branch head, then add a small reusable Python contact-resolver interface for future GUI/gateway clients without spawning a subprocess or exposing the contact file format directly.

---

### 2026-10-15 — Local Mesh Number contact book
- **Owner:** ChatGPT
- **Status:** initial implementation and tests committed on `feat/local-contact-book`; CI pending.
- **Changed paths:** `host/missionlink_contacts.py`, `host/tests/test_missionlink_contacts.py`, `docs/LOCAL_CONTACT_BOOK.md`.
- **Implemented:** strict `(777)-XXX-XXXX` address validation; local labels; separate optional routing node ID, public-key fingerprint, and last-seen timestamp; bounded 512-contact book; strict versioned JSON import/export; duplicate rejection; atomic local save and bounded load.
- **Tests added:** validation, lookup/rename/removal, identity-field separation, duplicate address rejection, stable round-trip, schema rejection, count/file-size bounds, and atomic persistence.
- **Security limits:** fingerprints are stored but not verified; no address allocation/anti-collision authority, node discovery, sync, encryption-at-rest, UI, or messaging. Local OS permissions remain the caller's responsibility.
- **Validation:** no CI result yet; no hardware or RF behavior is involved.

## AI-to-AI collaboration handoff (read before making changes)

### 2026-10-14 — Bounded service request/response envelopes
- **Owner:** ChatGPT
- **Status:** implementation committed on a feature branch; PR #12 open; first CI run found an oversized error envelope, fixed with compact wire keys; rerun pending.
- **Task:** advance Issue #10's service protocol groundwork.
- **PR:** https://github.com/Innygr/Mission-Link/pull/12
- **Changed paths:** `host/missionlink_envelopes.py`, `host/tests/test_missionlink_envelopes.py`, `host/service-envelope.schema.json`, `docs/SERVICE_ENVELOPES.md`
- **Implemented:** version-1 request/response/error envelope codec with compact wire keys; strict identifier and timestamp bounds; five-minute maximum lifetime; bounded JSON payload depth; allowlisted error codes; rejection of unknown fields and unsupported versions; 180-byte encoded envelope ceiling; JSON Schema and unit tests.
- **Validation:** first GitHub Actions run caught one failing test because the verbose error-envelope fields exceeded 180 bytes; compact wire keys now reduce overhead, and the test has been updated. Awaiting CI rerun to verify the fix. No live gateway, network listener, authentication, replay protection, encryption, radio routing, or hardware test added.
- **Safety:** RF remains disabled; the size ceiling does not guarantee a given radio configuration can carry a full envelope.
- **Next:** review CI output, fix any test issues, then use the codec as a validation layer in a later authenticated gateway/API implementation.


- **Coordination issue:** https://github.com/Innygr/Mission-Link/issues/4
- **Shared source of truth:** this repository, its commits/branches/PRs, and this status file. Claude and ChatGPT do not have a direct private-chat channel to each other; use commits, PRs, and this log as the handoff mechanism.
- **Before work:** inspect recent commits, open PRs/issues, and this file. Do not assume another assistant's work is finished just because the plan mentions it.
- **Avoid collisions:** Claude owns `web/flasher.html`, `web/flasher.js`, and `web/flasher.css` by default. ChatGPT focuses on host services, firmware, mesh protocol/routing, tests, and documentation, after checking current changes. Ask the maintainer before crossing ownership boundaries.
- **After work:** record owner, task, changed paths, commit/PR URLs, validation actually run, limitations/blockers, next action, and files that should not be edited concurrently.
- **Truthful validation:** distinguish source review, local tests, CI results, and physical hardware tests. Never claim a check passed without seeing its result.
- **RF safety gate:** keep `ML_RF_ENABLED=0` until the exact hardware/pinout/RF switch/antenna path and Canadian compliance are verified and the maintainer approves.
- **Recent ChatGPT host-service hardening:** [validation changes](https://github.com/Innygr/Mission-Link/commit/81bd7a827741cf085a4d32d7d5e4a9f65ec84d77) and [edge-case tests](https://github.com/Innygr/Mission-Link/commit/a4d67c43a95d3a7ba8154ddfd468f5407094355d). CI has not yet been confirmed for these commits.

---

### 2026-10-14 — Cross-platform host services foundation
- **Owner:** ChatGPT, requested by maintainer.
- **Status:** initial code and docs committed; CI execution not yet confirmed.
- **Task:** [Issue #3 — platform-independent mesh gateway compatibility](https://github.com/Innygr/Mission-Link/issues/3).
- **Commits:** [service registry](https://github.com/Innygr/Mission-Link/commit/4c6168ec20b88578230d11375d98fc6d8064c690), [registry tests](https://github.com/Innygr/Mission-Link/commit/87cd025ce45c32534062957f085799b0ba11e581), [test import fix](https://github.com/Innygr/Mission-Link/commit/9da4e9ae205b6ce1d263a5655b95090925a4dc31), [host package marker](https://github.com/Innygr/Mission-Link/commit/8467ea46f55a48ac08d45484488a74d01a289f66), [host-services architecture](https://github.com/Innygr/Mission-Link/commit/95faaa1bab6b45035e4cbd5f9c773d06a9cffd90), [CI tests](https://github.com/Innygr/Mission-Link/commit/c76d7b1cf4fe0a453b7c0c84e666a185eeb4e2b1), [README](https://github.com/Innygr/Mission-Link/commit/57aab985ccd2934b3f4da31f69198b60c85eef69).
- **Changed paths:** `host/missionlink_services.py`, `host/tests/test_missionlink_services.py`, `host/__init__.py`, `docs/HOST_SERVICES.md`, `.github/workflows/firmware-ci.yml`, `README.md`.
- **Implemented:** platform-independent bounded service registry with stable host/service IDs, capability filtering, health filtering, transport metadata, and conservative mesh-inline payload policy. Added unittest coverage and a CI step. Documented that large file/model data should use an authenticated direct transport while LoRa carries discovery/control metadata.
- **Not implemented yet:** a live gateway API/server, LAN or USB transport adapter, host discovery, file-storage plugin, local-AI plugin, production authentication, or end-to-end security. This is not yet usable as a live service bridge.
- **Safety:** `ML_RF_ENABLED` remains `0`; no RF setting was changed and no hardware test is claimed.
- **Next:** verify CI tests, harden endpoint/schema validation, then design a versioned authenticated gateway API with loopback-first binding and explicit permissions.

### 2026-10-14 — Enable receive-only Wio-SX1262 radio path
- **Owner:** ChatGPT, requested by maintainer.
- **Status:** configuration committed; ESP32-S3 CI build and native tests running.
- **Commits:** [radio initialization and RF switch/TCXO setup](https://github.com/Innygr/Mission-Link/commit/37146eaa7d2bf45ac490fcc94ff279a7d87c98ff), [enable receive-only board config](https://github.com/Innygr/Mission-Link/commit/6bced826e214cd04e02f7c20ac73aba15e9a498c).
- **Changed paths:** `src/missionlink_radio.cpp`, `platformio.ini`.
- **Implemented:** enables the Wio-SX1262 adapter's verified-config branch and continuous receive mode for the XIAO ESP32-S3 B2B kit; configures the GPIO 38 RF switch, SPI pins, SX1262 TCXO voltage (1.6 V), and initial 915 MHz / 125 kHz / SF9 / CR 4/7 receive parameters.
- **Transmit safety:** `ML_RF_ENABLED` remains `0`; `transmit()` remains blocked. The TX-power setting is only a RadioLib initialization parameter and does not enable transmission. No over-the-air test has been performed; received packets and real RF operation remain unverified. Confirm the exact kit revision, antenna connection, matching settings on both nodes, and Canadian compliance before enabling TX.
- **Validation:** GitHub Actions run [#38629762715](https://github.com/Innygr/Mission-Link/actions/runs/38629762715) was in progress at handoff. No physical hardware test performed.


### 2026-10-14 — Validate the exact build before flashing/publishing
- **Owner:** ChatGPT
- **Status:** committed; GitHub Actions validation running
- **Commits:** [built-firmware validator](https://github.com/Innygr/Mission-Link/commit/5ba84ced612e99876c384f826eea5e910160f5cf), [Pages preflight gate](https://github.com/Innygr/Mission-Link/commit/f2a78421112f8e318c7e00d98631313fa7c20a29), [firmware artifact preflight gate](https://github.com/Innygr/Mission-Link/commit/243ad40472201f8352c9adfbce251425e383cf58), [PR path filters](https://github.com/Innygr/Mission-Link/commit/05c17bae713df919916f3941a85090847c381c4f), [full packet codec round-trip assertions](https://github.com/Innygr/Mission-Link/commit/cb41b82dd9687d06e59d8b1f5e50fefb5be876f7)
- **Changed paths:** `web/tests/validate-built-firmware.mjs`, `.github/workflows/deploy-web.yml`, `.github/workflows/firmware-ci.yml`, `test/test_packet_codec/test_packet_codec.cpp`
- **Implemented:** the actual four files staged from PlatformIO are now passed through the same pre-flash validator used by the browser before either Pages deployment or the 14-day firmware artifact can be published. CI checks image headers/chip IDs, partition table and application slot fit—not just synthetic fixtures. Firmware CI also triggers when the shared validator or its tests change. Expanded packet codec round-trip assertions to compare every header field, all IDs, hops, and payload bytes.
- **Validation:** prior firmware CI run [#38628397453](https://github.com/Innygr/Mission-Link/actions/runs/38628397453) passed native tests and the default ESP32-S3 build; the new real-artifact validation step is pending in newer runs. No physical flash/radio test performed.
- **Next:** verify the new preflight step passes and Pages deploy completes; confirm all four deployed binaries are fetchable. Do not enable RF based on compile success alone.


### 2026-10-14 — Flasher initialization/cache fixes and radio state recovery
- **Owner:** ChatGPT
- **Status:** committed; CI running
- **Commits:** [load defaults at page startup](https://github.com/Innygr/Mission-Link/commit/0a4d12ba0b74e45904cfd74ded31afa8a8f1d20f), [bypass service-worker cache for firmware binaries](https://github.com/Innygr/Mission-Link/commit/8a61da6fd2240d2770bfa97bce6776dabef3eda6), [radio receive-state recovery](https://github.com/Innygr/Mission-Link/commit/d5c42fcf591d620c085e78201d2e55af5a9f846b)
- **Changed paths:** `web/flasher.js`, `web/sw.js`, `src/missionlink_radio.cpp`
- **Implemented:** official firmware loading now starts during flasher page initialization rather than waiting for Connect; service worker no longer intercepts/caches `/firmware/` requests, which continue to use `cache: "no-store"`; radio adapter updates `receiving_` based on actual receive restart results and reports receive errors rather than claiming RX is active after a failed restart. `beginReceive()` now initializes the radio for an explicitly configured TX-only build without entering receive mode; the repository defaults still keep RF disabled.
- **Validation:** inspected current source and checked the expected call paths; GitHub Actions runs are in progress. No physical USB flash or radio test performed. The default firmware build does not compile the pinmap-verified RadioLib branch, so that guarded branch still needs a dedicated compile check before any future activation.
- **Next:** confirm CI/deploy, verify the deployed firmware URLs, and add a safe compile-only validation path for the guarded radio adapter without enabling RF.

# MissionLink AI Work Status

> Update this file when task ownership, progress, test results, or blockers materially change. This is a snapshot, not a live agent connection. Check the latest commits, branches, issues, and PRs before relying on it.

## Active ownership

| Area | Starting owner | Coordination note |
|---|---|---|
| Web flasher: `web/flasher.html`, `web/flasher.js`, `web/flasher.css` | Claude | Avoid editing these paths concurrently; inspect the latest work before review or reassignment. |
| Firmware, radio adapter, mesh routing, and protocol docs | ChatGPT / available contributor | Inspect current code and CI before changes; keep RF disabled by default. |
| Priorities, device facts, acceptance, merge, RF enablement | Human maintainer | Human approval required for high-risk decisions. |

Ownership above records the intended starting split; it does not assert that a task is currently running or that an assistant can see every GitHub issue/PR.

## Current project gates

- [ ] Confirm exact XIAO ESP32-S3 + Wio-SX1262 hardware revision and radio pin mapping from authoritative board documentation.
- [ ] Verify RF switch, BUSY/DIO1/reset wiring, and antenna path for that exact revision.
- [ ] Confirm Canadian operating parameters/compliance before any transmission.
- [ ] Build current firmware and resolve adapter/API compile errors.
- [ ] Test receive/send only on suitable hardware after the preceding gates.
- [ ] Verify actual encryption, acknowledgements, discovery, and relay behavior before describing them as implemented.

**Safety default:** RF transmission remains disabled until the maintainer explicitly approves a verified configuration. A successful CI run is not hardware validation.

## Handoff log

### 2026-10-14 — Flasher defaults to this repository's own firmware build
- **Owner:** ChatGPT, requested by maintainer; overlaps Claude's flasher ownership, so surfaced in the shared handoff.
- **Status:** committed; GitHub Actions deployment/build validation pending.
- **Commits:** [flasher UI](https://github.com/Innygr/Mission-Link/commit/713ecf9366da51ea2be45c1aceb3f3809a5926d8), [default file loading](https://github.com/Innygr/Mission-Link/commit/5071c058fb5e15df79c67e2f971814a8b61a23e9), [service-worker cache bump](https://github.com/Innygr/Mission-Link/commit/d0fc52fa97d2786a60b01cbd61d4052665491fdc), [build/deploy workflow](https://github.com/Innygr/Mission-Link/commit/3215d46088667c1669dfc84f433e376037a5b4ff).
- **Changed paths:** `web/flasher.html`, `web/flasher.js`, `web/sw.js`, `.github/workflows/deploy-web.yml`.
- **Implemented:** the Pages workflow builds the current `main` source for `seeed_xiao_esp32s3`, stages `bootloader.bin`, `partitions.bin`, `boot_app0.bin`, and `firmware.bin` into the site artifact, and the flasher fetches those four same-origin files as its defaults. Users can still override files individually. Existing pre-flash checks still run before writing. Workflow runs for firmware source changes as well as web changes.
- **Validation:** source changes pushed; checking CI. No physical flash performed.
- **Next action:** confirm Pages build/deploy succeeds, verify all four files are published and served, then let Claude know this intentionally changes their flasher paths.


### 2026-10-14 — Mesh event diagnostics and CI deliverables
- **Owner:** ChatGPT
- **Status:** committed; CI validation running
- **Commits:** [mesh event metadata](https://github.com/Innygr/Mission-Link/commit/ddc075ea023e34a2845efdeb0cdd62753d14ddfc), [protocol docs](https://github.com/Innygr/Mission-Link/commit/fc5888afc9bc76d30930cb7f0963055b1694642f), [firmware artifacts workflow](https://github.com/Innygr/Mission-Link/commit/36a7c406001946208a0dad31042f4bc238eab8cc), [web validation workflow](https://github.com/Innygr/Mission-Link/commit/12edf3fb8808bc05b48e703bf299b0e6e5879d9a).
- **Changed paths:** `src/main.cpp`, `docs/CLIENT_PROTOCOL.md`, `.github/workflows/firmware-ci.yml`, `.github/workflows/deploy-web.yml`.
- **Implemented:** mesh events now retain the numeric action and add stable action names, packet type/address mode, source/destination/message IDs, hop counters, payload byte count, RSSI, and SNR. Protocol docs explicitly say this is diagnostic metadata, not decrypted message delivery. Firmware CI now packages bootloader, partition table, boot_app0, and firmware binaries as a 14-day artifact. Web CI now runs flasher syntax checks and its Node tests on PRs; deploy is skipped for pull requests.
- **Validation:** earlier native tests passed on the same firmware source; new firmware/web workflow runs are pending. No physical hardware or RF test performed.
- **Next action:** verify the new workflow runs and artifact contents; then tackle an RF-independent local-delivery/application boundary without exposing plaintext or pretending encryption is implemented.


### 2026-10-14 — Firmware status reporting
- **Owner:** ChatGPT
- **Status:** implemented in source; native tests and ESP32-S3 scaffold build passed in GitHub Actions.
- **Changed paths:** `src/main.cpp`, `docs/CLIENT_PROTOCOL.md`, `docs/TESTING_GUIDE.md`, `README.md`.
- **Change:** the `status` command now reports the adapter's actual `radioReady`, `radioInitialized`, `radioReceiving`, and `radioState` values instead of hardcoding `radioReady:false`; docs clarify that guarded receive-to-router wiring exists but real RF/mesh delivery is unverified.
- **Validation:** fetched changed files back from GitHub and inspected the updated status code and documentation. GitHub Actions native packet/mesh tests passed on the firmware-status commit. The ESP32-S3 compile completed successfully in GitHub Actions (run [#38625875338](https://github.com/Innygr/Mission-Link/actions/runs/38625875338)); no physical hardware test was performed.
- **Next action:** next firmware task should target application-level message delivery and explicit radio-state handling, while keeping RF disabled.


Add newest entries at the top. Link to the commit, PR, or issue whenever possible.

### Handoff — 2026-10-14 (web flasher, round 2: ready for a bench test)
- **From / to:** Claude / ChatGPT + maintainer
- **Task / PR:** https://github.com/Innygr/Mission-Link/pull/2 (branch `claude/zen-cerf-x5xdd9`)
- **Status:** ready for a first hardware flash test; review requested
- **Changed paths:** `web/flasher.js`, `web/flasher-validate.js`, `web/tests/flasher-validate.test.mjs`, `web/README.md`, `docs/AI_STATUS.md`
- **Implemented:** checked against the esptool-js 0.5.6 package source. `writeFlash` now passes `flashMode`/`flashFreq: "keep"` (required by `FlashOptions`, previously omitted) and a `calculateMD5Hash` function, so esptool-js runs its read-back MD5 check after each file (previously never ran). Added tester steps to `web/README.md`.
- **Validation:** `node --test web/tests/flasher-validate.test.mjs` → 9/9 pass (MD5 cross-checked with node:crypto). Called esptool-js 0.5.6's `_updateImageFlashParams` in Node with both option sets: the image was unchanged both times (the old call was not corrupting the bootloader, because the library's own magic check bails out). Local `pio run -e xiao_esp32s3` → SUCCESS (RAM 9.5%, flash 15.0%). Its real `bootloader.bin`, `partitions.bin`, `firmware.bin` plus the framework's `boot_app0.bin` pass `validateFirmwareSet` with no errors or warnings; the table is the 8 MB layout (app0 0x10000/0x330000).
- **Not validated:** any flash over Web Serial, the MD5 read-back on a device, reset behaviour, any hardware/radio behaviour.
- **Requests for ChatGPT (shared files, not edited by Claude):** (1) consider an `actions/upload-artifact` step in `firmware-ci.yml` for the four `.bin` files so testers don't need a local build; (2) consider adding `node --check web/flasher.js web/flasher-validate.js` and `node --test web/tests/flasher-validate.test.mjs` to `deploy-web.yml` (it currently runs on push to main only, not on PRs); (3) confirm `board = esp32-s3-devkitc-1` and its 8 MB layout match the XIAO ESP32-S3.
- **Next action:** maintainer flashes a spare XIAO per `web/README.md` and reports the log (connect message, MD5 lines, serial output after reset).

### Handoff — 2026-10-14 (web flasher pre-flash validation)
- **From / to:** Claude / ChatGPT (integration review)
- **Task / PR:** Harden `web/flasher.*`; branch `claude/zen-cerf-x5xdd9` (no PR opened)
- **Status:** review requested
- **Changed paths:** `web/flasher.js`, `web/flasher-validate.js` (new), `web/tests/flasher-validate.test.mjs` (new), `web/sw.js` (cache v4 + new asset only), `docs/AI_STATUS.md`
- **Implemented:** refuses to flash unless the connected chip is ESP32-S3; checks image magic 0xE9 and ESP32-S3 chip ID in bootloader/app; slot-size limits (flags merged images); parses the partition table and requires an app partition at 0x10000 large enough for the app; logs SHA-256 of each file; confirmation lists address/file/size; separate confirmation for erase-all; esptool-js now loaded on demand so a blocked CDN gives an error instead of a dead page; reports USB unplug during flashing.
- **Validation:** `node --test web/tests/flasher-validate.test.mjs` → 8/8 pass (host-only, synthetic images). Headless Chromium loaded `flasher.html` with no page errors (esptool-js not fetched, no Web Serial).
- **Not validated:** actual flashing, esptool-js 0.5.6 API behaviour with these changes, real PlatformIO build output against the checks, any hardware or radio behaviour. No firmware, protocol, or GATT changes. Flash success still means only "write completed".
- **Risks / compatibility:** esptool-js still loads from unpkg without SRI (module imports can't take integrity attributes); vendoring a pinned copy is the safer next step. Checks assume the standard S3 layout (bootloader 0x0, table 0x8000, app 0x10000).
- **Next action:** ChatGPT to confirm the PlatformIO output layout matches these checks; maintainer to try a real flash on a spare board.

### Initial coordination setup
- **Status:** coordination files added; no firmware or flasher behavior changed by this setup.
- **Validation:** documentation-only change; no firmware, browser, or hardware tests claimed.
- **Next action:** each contributor reads `AGENTS.md` and this file, then claims a specific task and records a handoff.

## Handoff template

```markdown
### Handoff — YYYY-MM-DD
- **From / to:**
- **Task / PR:**
- **Status:** done / in progress / blocked / review requested
- **Changed paths:**
- **Validation:** exact tests/builds run and results
- **Not validated:**
- **Risks / compatibility:**
- **Next action:**
```
