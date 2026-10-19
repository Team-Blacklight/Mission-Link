# MissionLink client control protocol

Version 1 of the local USB serial/BLE command interface uses UTF-8 commands from clients and JSON objects from firmware. Serial output is newline-delimited JSON (JSON Lines). BLE notifications carry one JSON event each. This is a local control protocol, not the encrypted LoRa wire format.

## Commands

Commands are case-sensitive, trimmed UTF-8 strings. Firmware bounds command input to 256 bytes:

- USB serial input is processed as newline-delimited commands. An overlong line is discarded in full; the truncated prefix is never dispatched.
- Each BLE write must be 256 bytes or fewer. Oversized writes are rejected.

Currently implemented diagnostic/control commands:

- `help`
- `status`
- `nodes`
- `presets`
- `preset client`
- `preset repeater`
- `preset gateway`

Placeholder command families (return `type=error`, code `not_ready`):

- `send <node-id> <text>`
- `group <channel-id> <text>`
- `join <channel-id>`
- `leave <channel-id>`

Preset selection is persisted in ESP32 NVS (internal flash) and restored at boot when NVS is available. If NVS cannot initialize, firmware emits a `settings_unavailable` warning and changes are runtime-only; the preset response includes `persisted:false`. If a write fails, the command returns `settings_write_failed` and does not apply the new preset. This is a small system setting, not TF/microSD storage. Preset selection changes the RF-independent routing policy object. The guarded radio receive-to-router and forward-to-radio calls are wired into the main loop, but the adapter stays inert with the default unverified pin map, RF transmission is disabled, and local application delivery is not implemented.

## Events

Status response (firmware 0.2.1; fields may evolve). `radioReady` is true only when the adapter is initialized and reports `ready`; `radioState` gives the adapter state. With the default unverified pin map and disabled RF, expect `pinmap_unverified` and `radioReady:false`. `settingsReady` reports whether internal-flash NVS initialized; `presetPersisted` reports whether the active preset matches the saved NVS value:

```json
{"type":"status","firmware":"0.2.1","protocolVersion":1,"rfEnabled":false,"bleReady":true,"serialReady":true,"radioReady":false,"radioInitialized":false,"radioReceiving":false,"radioState":"pinmap_unverified","settingsReady":true,"presetPersisted":true,"preset":"client"}
```

Preset list response:

```json
{"type":"presets","items":[{"id":"client","description":"End device; never relays packets."},{"id":"repeater","description":"Relays traffic without presenting group traffic locally."},{"id":"gateway","description":"Relays traffic and delivers group traffic locally."}]}
```

Preset selection response:

```json
{"type":"preset","active":"repeater","persisted":true}
```

Node discovery response:

```json
{"type":"nodes","items":[],"ready":false,"reason":"discovery_not_implemented"}
```

Error response:

```json
{"type":"error","code":"not_ready","message":"Direct messaging is unavailable until radio and encryption are implemented."}
```

Mesh routing events (emitted only if the radio adapter actually returns a decoded packet) include a stable human-readable `actionName` alongside the numeric `action` for compatibility, plus `packetType`, `addressMode`, `source`, `destination`, `messageId`, `hopCount`, `hopLimit`, `payloadBytes`, `rssi`, and `snr`. IDs are lowercase hexadecimal. Action names are `drop_invalid`, `drop_duplicate`, `deliver_local`, `deliver_and_forward`, and `forward_only`. These are routing/diagnostic metadata only: this event does not expose message plaintext or mean that encrypted application delivery, authentication, or reliable delivery is implemented.

Boot and BLE events report firmware startup/advertising. Clients should tolerate unknown event types and optional fields for forward compatibility. BLE notification delivery is best-effort; clients should not treat a command write as proof that a response was received.

## BLE identifiers

- Device name: `MissionLink`
- Service: `3f4a0001-7c2e-4a5b-9d11-5b7d6e100001`
- RX write characteristic: `3f4a0002-7c2e-4a5b-9d11-5b7d6e100001`
- TX read/notify characteristic: `3f4a0003-7c2e-4a5b-9d11-5b7d6e100001`

## Security boundaries

- USB serial/BLE control commands are not the over-the-air encryption protocol.
- Do not send secrets over BLE until an authenticated pairing/provisioning flow is implemented.
- The LoRa transmitter remains disabled. Never treat the current scaffold as a production secure messenger.
- Future radio payloads must use authenticated encryption and replay protection; routing relays should not need message plaintext keys.


## Python USB serial client

The host-side diagnostic client is available in `host/missionlink_serial_client.py`.
It supports only the currently implemented control commands (`help`, `status`,
`nodes`, `presets`, and the three preset choices). It reads newline-delimited JSON,
skips unrelated asynchronous events, validates response shape and size, and times out
when no matching response arrives.

Install PySerial only if opening a real port:

```sh
python -m pip install pyserial
```

Example:

```python
from host.missionlink_serial_client import open_serial

client = open_serial("COM5")  # On Linux, typically /dev/ttyACM0 or /dev/ttyUSB0
try:
    print(client.status())
    print(client.presets())
finally:
    client.close()
```

The port name is platform-specific. Close other serial monitors before connecting.
The client intentionally rejects `send`, `group`, `join`, and `leave`: firmware
reports those operations as not ready, and exposing them as a working messaging API
would be misleading. Node discovery is also still unimplemented. This client is a
local diagnostic/control adapter only; it does not enable RF or prove physical
device connectivity.
