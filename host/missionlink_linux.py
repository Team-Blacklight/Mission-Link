#!/usr/bin/env python3
"""Ubuntu/Linux command-line client for a MissionLink node over USB serial.

This is a host-side control/status client, not a LoRa radio driver. It uses the
firmware's existing newline-delimited UTF-8 command interface. Messaging is
intentionally not exposed while the firmware reports it as unavailable.
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from typing import Any, Callable

BAUD_RATE = 115200
DEFAULT_TIMEOUT = 2.0
MAX_LINE_BYTES = 4096
PRESETS = ("client", "repeater", "gateway")


class MissionLinkError(RuntimeError):
    """Raised for serial, protocol, or firmware-reported errors."""


def parse_response(line: bytes | str) -> dict[str, Any]:
    """Decode one bounded JSON response line from firmware."""
    if isinstance(line, bytes):
        if len(line) > MAX_LINE_BYTES:
            raise MissionLinkError("response_too_large")
        try:
            text = line.decode("utf-8").strip()
        except UnicodeDecodeError as exc:
            raise MissionLinkError("response_not_utf8") from exc
    elif isinstance(line, str):
        if len(line.encode("utf-8")) > MAX_LINE_BYTES:
            raise MissionLinkError("response_too_large")
        text = line.strip()
    else:
        raise MissionLinkError("invalid_response_type")

    if not text:
        raise MissionLinkError("empty_response")
    try:
        payload = json.loads(text)
    except json.JSONDecodeError as exc:
        raise MissionLinkError("invalid_json_response") from exc
    if not isinstance(payload, dict) or not isinstance(payload.get("type"), str):
        raise MissionLinkError("unexpected_response_shape")
    if payload["type"] == "error":
        code = payload.get("code", "firmware_error")
        raise MissionLinkError(f"{code}: {payload.get('message', 'command rejected')}")
    return payload


def _open_serial(port: str, timeout: float):
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError as exc:
        raise MissionLinkError(
            "pyserial is required; install it with: python3 -m pip install --user pyserial"
        ) from exc
    try:
        return serial.Serial(port=port, baudrate=BAUD_RATE, timeout=timeout,
                             write_timeout=timeout)
    except Exception as exc:
        raise MissionLinkError(f"cannot_open_serial_port: {exc}") from exc


def request(port: str, command: str, timeout: float = DEFAULT_TIMEOUT,
            serial_factory: Callable[..., Any] | None = None) -> dict[str, Any]:
    """Send one known firmware command and read one JSON response."""
    if command not in ("status", "presets") and not (
        command.startswith("preset ") and command[7:] in PRESETS
    ):
        raise MissionLinkError("unsupported_or_unavailable_command")
    opener = serial_factory or _open_serial
    try:
        with opener(port, timeout) as device:
            # Discard stale boot/status lines before asking our question.
            device.reset_input_buffer()
            device.write((command + "\n").encode("utf-8"))
            device.flush()
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                line = device.readline()
                if not line:
                    continue
                try:
                    return parse_response(line)
                except MissionLinkError as exc:
                    # Ignore non-JSON boot chatter, but never hide a valid
                    # firmware error response (parse_response raises it too).
                    if str(exc).startswith(("invalid_json_response", "empty_response")):
                        continue
                    raise
            raise MissionLinkError("response_timeout")
    except MissionLinkError:
        raise
    except Exception as exc:
        raise MissionLinkError(f"serial_io_error: {exc}") from exc


def monitor(port: str, timeout: float = 1.0,
            serial_factory: Callable[..., Any] | None = None) -> None:
    """Print newline-delimited JSON events as received; Ctrl-C exits."""
    opener = serial_factory or _open_serial
    try:
        with opener(port, timeout) as device:
            print(f"Monitoring {port} at {BAUD_RATE} baud (Ctrl-C to stop).")
            while True:
                line = device.readline()
                if not line:
                    continue
                try:
                    event = parse_response(line)
                    print(json.dumps(event, ensure_ascii=False, sort_keys=True))
                except MissionLinkError as exc:
                    print(f"[ignored serial line: {exc}]", file=sys.stderr)
    except KeyboardInterrupt:
        print("\nStopped.")
    except MissionLinkError:
        raise
    except Exception as exc:
        raise MissionLinkError(f"serial_io_error: {exc}") from exc


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Control and inspect a MissionLink node from Ubuntu/Linux over USB serial."
    )
    parser.add_argument("--port", required=True, help="serial device, e.g. /dev/ttyACM0")
    sub = parser.add_subparsers(dest="action", required=True)
    sub.add_parser("status", help="show firmware, radio, RF, and preset status")
    sub.add_parser("presets", help="list presets reported by firmware")
    set_preset = sub.add_parser("set-preset", help="select a supported runtime preset")
    set_preset.add_argument("preset", choices=PRESETS)
    sub.add_parser("monitor", help="print JSON lines emitted by the node")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.action == "monitor":
            monitor(args.port)
            return 0
        command = {
            "status": "status",
            "presets": "presets",
            "set-preset": f"preset {args.preset}",
        }[args.action]
        response = request(args.port, command)
        print(json.dumps(response, indent=2, ensure_ascii=False))
        return 0
    except MissionLinkError as exc:
        print(f"missionlink: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
