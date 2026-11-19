# MissionLink 

Custom encrypted LoRa messaging for XIAO ESP32-S3 + Wio-SX1262.

## Project status

This is an actively developing scaffold, **not yet a validated radio firmware build or production messaging system**. RF transmission is deliberately disabled. The exact Wio-SX1262 pin mapping, RF switch setup, and regional radio configuration must be confirmed for the exact hardware revision before enabling RF transmission. Verify Canadian compliance before transmitting.

## Client applications

- **Web/PWA:** `web/` — responsive local dashboard, installable web app manifest, offline static-asset caching, and a USB serial console in supported desktop Chromium browsers.
- **Visual test bench:** [`web/testbench.html`](web/testbench.html) — point-and-click USB diagnostics, preset switching, and a firmware response test suite without needing a terminal.
- **Web flasher:** [`web/flasher.html`](web/flasher.html) — flash CI-built binaries from a supported desktop Chromium browser; download the firmware artifact from [Firmware Builds](https://github.com/Innygr/Mission-Link/actions/workflows/firmware-ci.yml) after a successful build.
- **Apple app:** `apple/MissionLink/` — SwiftUI + Core Bluetooth starter targeting iOS 16.7 and later. It uses the GATT UUIDs currently in `src/main.cpp`.
- **Cross-platform host services:** [`docs/HOST_SERVICES.md`](docs/HOST_SERVICES.md), [`host/missionlink_services.py`](host/missionlink_services.py), [`host/missionlink_store.py`](host/missionlink_store.py), and [`host/missionlink_gateway.py`](host/missionlink_gateway.py) — bounded service registry with atomic local persistence and an experimental token-authenticated HTTP API restricted to loopback. It is a local control-plane API only, not a radio bridge or production service transport.
- **Architecture notes:** `docs/CLIENT_ARCHITECTURE.md`.
- **Security and glyph transport design:** `docs/CIPHER_SUITES.md` (design proposal; not implemented in firmware yet).
- **Shared command/event schema:** `docs/CLIENT_PROTOCOL.md`.
- **Binary packet format:** `docs/PACKET_FORMAT.md` (protocol v1 codec layout and validation rules).
- **Radio bring-up checklist:** `docs/RADIO_BRINGUP.md` (staged hardware validation; RF remains disabled).
- **Bench testing guide:** `docs/TESTING_GUIDE.md` — step-by-step native tests, compile, USB serial, BLE, expected responses, and a test-report template.
- **Radio adapter:** `src/missionlink_radio.cpp` and `include/missionlink_radio.h` (feature-gated SX1262/RadioLib adapter; hardware setup and RF operation remain unverified).
- **Mesh routing module:** `src/mesh_router.cpp` and `include/mesh_router.h` (RF-independent routing policy and native simulation tests; real mesh delivery is unverified).
- **Node presets:** `docs/NODE_PRESETS.md` — Client, Repeater, and Gateway roles.

Safari on iOS does not support Web Bluetooth, so the PWA cannot directly connect to the radio over BLE. Use the native Apple app for direct BLE access. The iOS app source is a starter. An iOS simulator build check is defined in GitHub Actions; physical-device BLE behavior still needs testing.

A GitHub Actions workflow validates and deploys `web/` to GitHub Pages. The latest logo integration deployment succeeded; installed PWAs may need a reload to refresh cached assets.

## Firmware scaffold (0.2.1)

- USB serial command interface
- BLE GATT command interface (ESP32-S3)
- Direct node addressing and group/channel addressing in the packet model
- Versioned packet codec with bounded fields
- Relay-ready packet metadata (hop limit, hop count, message ID)
- Feature-gated SX1262 radio adapter interface with receive/decode path and a compile-time RF transmit guard (disabled by default)
- RF-independent mesh forwarding policy with duplicate suppression, bounded hop counts, and forwarding rate limits (simulator only)
- Client / Repeater / Gateway routing presets (volatile selection; radio integration pending)
- PlatformIO host-independent protocol tests, including malformed-frame rejection
- JSON Lines status/help/error events shared by USB serial and BLE
- Bounded command input: oversized USB lines are discarded whole, and oversized BLE writes are rejected
- GitHub Actions workflows for web validation, packet tests, ESP32 compile, Python host-service tests, and iOS simulator compile

## Cross-platform service hosts

MissionLink is designed so an ESP32-S3 radio node is **not required on every computer**. A Windows, Linux, or macOS host, NAS, Raspberry Pi-class system, or local-AI workstation can provide a service through a reachable MissionLink gateway. The host can keep bulk data and compute on its own local network; the mesh is for discovery and small control messages, not model-weight or large-file transport.

The first cross-platform component is the bounded service registry. Run its tests from the repository root with Python 3.10+:

```sh
python -m unittest discover -s host/tests -v
```

A network gateway API, authentication flow, live service discovery, file-storage plugin, and AI-inference plugin remain future work. A registry entry does not mean that a service is reachable. See [Host services architecture](docs/HOST_SERVICES.md).

## Build

Install PlatformIO Core, then from the repository root:

```sh
pio test -e native
pio run -e xiao_esp32s3
```

The ESP32-S3 compile target does not guarantee that the XIAO/Wio board wiring is configured correctly. CI compiles the firmware scaffold and tests host-independent code; exact hardware configuration and RF operation still require bench testing.

## Serial/BLE commands

- `help`
- `status`
- `nodes`
- `presets`
- `preset client`
- `preset repeater`
- `preset gateway`
- `send <node-id> <text>` (placeholder; returns `not_ready`)
- `group <channel-id> <text>` (placeholder; returns `not_ready`)
- `join <channel-id>` (placeholder; returns `not_ready`)
- `leave <channel-id>` (placeholder; returns `not_ready`)

Commands are parsed through one common dispatcher so USB serial and BLE share behavior. Responses use JSON Lines on serial and one JSON event per BLE notification. Node discovery, group membership, and message delivery remain placeholders; see `docs/CLIENT_PROTOCOL.md`. Preset selection is volatile and resets to Client on reboot. Presets do not turn on the radio or cause real over-the-air relaying.

## Encryption and identity status

Production encryption/key provisioning is not implemented yet. The planned cipher suites and optional Wingdings-style ciphertext representation are documented in `docs/CIPHER_SUITES.md`. Before RF tests, implement authenticated encryption, persistent per-device identity, nonces that cannot repeat under a key, authenticated headers, replay protection, and documented key provisioning. Never commit a shared production key to source control.

## Network model

- **Implementation order:** mesh routing first, then direct P2P optimisation. See [mesh-first routing plan](docs/MESH_FIRST_ROUTING.md), [mesh routing test plan](docs/MESH_ROUTING_TEST_PLAN.md), and [mesh simulator implementation notes](docs/MESH_ROUTER_SIMULATOR.md).
- **Direct node connection:** unicast to one node ID.
- **Group channel:** group ID in the packet; group members decrypt with that group's key.
- **Relay:** relays forward ciphertext based on routing metadata, without group keys.

## Contributing and AI collaboration

This repository uses a shared handoff protocol for human and AI-assisted contributors:

- [Contributor instructions](AGENTS.md)
- [AI collaboration workflow](docs/AI_COLLABORATION.md)
- [Current AI work status and handoff log](docs/AI_STATUS.md)
- [AI task / handoff issue template](.github/ISSUE_TEMPLATE/ai-task.yml)
- [Coordination issue #1](https://github.com/Innygr/Mission-Link/issues/1)
- [Cross-platform host-services task #3](https://github.com/Innygr/Mission-Link/issues/3)

The repository and its issue tracker provide a shared record, not an automatic live bridge between ChatGPT and Claude. Confirm that each assistant's current integration can access the relevant files or issue before relying on a handoff. RF remains disabled until the hardware configuration and regional requirements are verified.
