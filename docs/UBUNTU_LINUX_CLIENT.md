# Ubuntu/Linux USB serial client

The first Linux client is a small Python CLI for controlling and inspecting a MissionLink node connected over USB. It uses the firmware's existing newline-delimited UTF-8 command interface and JSON responses. It does **not** make the Linux computer a LoRa radio by itself; a compatible MissionLink radio node is still required.

## Install on Ubuntu

From the repository root:

```sh
sudo apt update
sudo apt install python3 python3-venv
python3 -m venv .venv
. .venv/bin/activate
python -m pip install pyserial
```

Find the connected device:

```sh
python -m serial.tools.list_ports -v
```

Common device names are `/dev/ttyACM0` and `/dev/ttyUSB0`. Use the port actually reported by your system.

If access is denied, check the device's group:

```sh
ls -l /dev/ttyACM0
groups
```

On typical Ubuntu installs, add your own account to `dialout` if that is the group shown, then sign out and sign back in:

```sh
sudo usermod -aG dialout "$USER"
```

Only do this for your own trusted account; serial access can control attached hardware.

## Commands

Activate the virtual environment first, then replace `/dev/ttyACM0` with your device:

```sh
python host/missionlink_linux.py --port /dev/ttyACM0 status
python host/missionlink_linux.py --port /dev/ttyACM0 presets
python host/missionlink_linux.py --port /dev/ttyACM0 set-preset client
python host/missionlink_linux.py --port /dev/ttyACM0 set-preset repeater
python host/missionlink_linux.py --port /dev/ttyACM0 set-preset gateway
python host/missionlink_linux.py --port /dev/ttyACM0 monitor
```

The preset selection is currently held in firmware RAM and resets on reboot. The current firmware accepts `client`, `repeater`, and `gateway`; combined capability presets such as `Clipeater` and `CliPeatWay` are not implemented in the firmware yet.

## Current limits

- The client exposes status, preset listing/selection, and a JSON event monitor only.
- It deliberately does not offer a send command: firmware currently rejects messaging because end-to-end security and the full radio messaging path are not ready.
- The monitor can display events only when firmware emits them; it does not create packet events that the firmware does not yet provide.
- This is a USB serial adapter, not a live LAN API or background gateway daemon.
- Do not treat `radioReady` or a successful serial command as proof of a working over-the-air link.
- Keep RF transmission disabled until the exact board/revision, antenna path, regional compliance, and maintainer approval gates are satisfied.

## Tests

From the repository root:

```sh
python -m unittest discover -s host/tests -v
```

The Linux CLI protocol tests use a fake serial device and do not require a physical node or transmit RF.
