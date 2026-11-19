# Cross-platform host services and mesh gateways

MissionLink's radio firmware is not the same thing as a service host. A machine does **not** need to be an ESP32-S3, run Arduino firmware, or have its own LoRa radio to offer storage or AI services.

## Architecture

```text
Windows / Linux / macOS / NAS / local-AI workstation
         |  authenticated LAN API (bulk data stays here)
         v
Cross-platform MissionLink host service
         |  local gateway API / USB serial (future adapter)
         v
MissionLink gateway node (e.g. XIAO ESP32-S3 + Wio-SX1262)
         |  short control messages, discovery, job IDs
         v
          LoRa mesh
```

A host joins by connecting to a MissionLink gateway that it can reach over a supported local transport. The gateway bridges service discovery and small control messages between the host API and mesh. The host and the gateway have separate identities: a mesh node ID identifies a radio endpoint; a host ID identifies a computer; a service ID identifies one capability on that computer.

The platform-independent registry lives in `host/missionlink_services.py`. `host/missionlink_store.py` provides bounded versioned JSON persistence with atomic replacement. `host/missionlink_gateway.py` adds a **loopback-only** HTTP control API. This is still not a radio bridge, automatic discovery system, or production service; don't describe a registered record as a reachable service.

## Service model

Each service record includes:

- stable service ID and host ID;
- display name and bounded capability list (examples: `file.index`, `file.read`, `ai.inference`);
- available transports;
- optional direct endpoint, health flag, and maximum inline request size.

The registry bounds service count, ID lengths, capability count, endpoint length, and mesh-inline request size. Endpoint metadata is local registry data; do not broadcast LAN addresses, access tokens, or credentials in public mesh announcements.



## Local gateway API (experimental)

Run from the repository root with Python 3.10+:

```sh
python -m host.missionlink_gateway --host 127.0.0.1 --port 8765 --store ~/.missionlink/services.json
```

The server refuses non-loopback bind addresses. It prints a newly generated bearer token once at startup; keep it private. Registry records are saved to the selected JSON path, loaded on startup, and updated through atomic file replacement. On POSIX systems the file is restricted to owner read/write permissions. Treat the registry as local metadata, not encrypted secret storage. Requests other than `GET /v1/health` require `Authorization: Bearer <token>`. The API supports listing services (`GET /v1/services`), optional capability/health filters, reading one record, registering a record (`POST /v1/services`), and unregistering one (`DELETE /v1/services/{service_id}`). Request JSON is bounded and unknown fields are rejected.

Example (replace the token with the one printed by the server):

```sh
curl -H "Authorization: Bearer YOUR_TOKEN" http://127.0.0.1:8765/v1/services
```

This initial adapter uses plain HTTP only on loopback and has no remote-LAN mode, per-service roles, TLS termination, or operation proxy. Do not forward its port, bind it to a LAN address, or treat its bearer token as production identity. Registered endpoints are metadata only: this API does not contact or execute them. The API does not enable RF or send mesh messages.

## What belongs on LoRa vs. LAN

**LoRa mesh:** discovery summaries, service IDs, short prompts/commands, status, job IDs, progress summaries, and small responses.

**Authenticated direct connection:** files, model weights, large prompts/outputs, embeddings, and other bulk payloads. The current protocol v1 payload ceiling is 180 bytes before future encryption overhead, and the conservative service-registry default is only 96 bytes for inline requests. Neither value is a promise that every real radio configuration supports the full amount.

Do not automatically chunk and send large files through LoRa. If a future use case absolutely requires store-and-forward transfer over radio, it needs a separately designed protocol with strict quotas, resumability, hashes, timeouts, expiry, congestion controls, and explicit user approval.

## Security requirements before production

- Authenticate host-to-gateway and client-to-service connections.
- Authorize every service operation; a mesh node being reachable does not grant file or AI access.
- Use TLS or another reviewed authenticated channel for remote service endpoints. Do not expose plain HTTP endpoints beyond loopback.
- Restrict file operations to explicitly configured roots; reject traversal, symlink escapes, and untrusted path input.
- Give AI services request/token/time limits and explicit opt-in before allowing tools or filesystem access.
- Do not put secrets, private file contents, raw prompts, or model data in mesh discovery packets.
- Add end-to-end authenticated encryption and replay protection to MissionLink before sending sensitive data over RF.
- Keep `ML_RF_ENABLED=0` until board configuration and Canadian compliance gates are complete.

## Current Python component and tests

From the repository root, with Python 3.10+:

```sh
python -m unittest discover -s host/tests -v
```

The registry is a data/validation layer with optional atomic local persistence; the experimental gateway wraps it with a loopback-only HTTP API. A subsequent milestone should add reviewed authenticated transport and stronger authorization before any remote access, then implement service plugins for file storage and local AI.
