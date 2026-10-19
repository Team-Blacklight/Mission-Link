"""Synchronous client for MissionLink's newline-delimited USB serial protocol.

This is a diagnostic/control client for commands currently implemented by the
firmware. It deliberately does not claim message delivery or implement a radio
transport. PySerial is imported only by open_serial(), so the protocol client
can be tested without hardware or third-party packages.
"""
from __future__ import annotations

import json
import math
import time
from typing import Any, Protocol

MAX_COMMAND_BYTES = 256
MAX_EVENT_BYTES = 4096
DEFAULT_TIMEOUT_SECONDS = 2.0


class SerialPort(Protocol):
    """Small subset of the PySerial API used by this client."""

    timeout: float | None

    def write(self, data: bytes) -> int: ...
    def readline(self) -> bytes: ...


class SerialClientError(RuntimeError):
    """Base exception for local serial-client failures."""


class SerialTimeout(SerialClientError):
    """No matching firmware response arrived before the deadline."""


class SerialProtocolError(SerialClientError):
    """The firmware returned an oversized or invalid JSON-line event."""


class FirmwareError(SerialClientError):
    """The firmware explicitly returned a type=error event."""

    def __init__(self, event: dict[str, Any]) -> None:
        self.event = event
        super().__init__(str(event.get("code", "firmware_error")))


def _expected_response(command: str) -> str:
    if command == "help":
        return "help"
    if command == "status":
        return "status"
    if command == "nodes":
        return "nodes"
    if command == "presets":
        return "presets"
    if command.startswith("preset "):
        preset = command[7:].strip()
        if preset not in {"client", "repeater", "gateway"}:
            raise ValueError("invalid_preset")
        return "preset"
    raise ValueError("unsupported_command")


class MissionLinkSerialClient:
    """Issue one command at a time and match its JSON response event."""

    def __init__(
        self,
        port: SerialPort,
        *,
        timeout_seconds: float = DEFAULT_TIMEOUT_SECONDS,
        max_event_bytes: int = MAX_EVENT_BYTES,
    ) -> None:
        if not callable(getattr(port, "write", None)) or not callable(getattr(port, "readline", None)):
            raise TypeError("port_must_support_write_and_readline")
        if isinstance(timeout_seconds, bool) or not isinstance(timeout_seconds, (int, float)) or not math.isfinite(timeout_seconds) or timeout_seconds <= 0:
            raise ValueError("invalid_timeout")
        if isinstance(max_event_bytes, bool) or not isinstance(max_event_bytes, int) or not 2 <= max_event_bytes <= MAX_EVENT_BYTES:
            raise ValueError("invalid_event_limit")
        self._port = port
        # Keep each readline bounded so a port timeout cannot indefinitely
        # overrun the overall response deadline. None means blocking forever.
        if hasattr(port, "timeout"):
            port_timeout = getattr(port, "timeout", None)
            try:
                read_timeout = min(0.25, float(timeout_seconds))
                port.timeout = read_timeout if port_timeout is None else min(float(port_timeout), read_timeout)
            except (AttributeError, TypeError, ValueError):
                pass
        self._timeout_seconds = float(timeout_seconds)
        self._max_event_bytes = max_event_bytes
        self._pending = bytearray()

    def request(self, command: str) -> dict[str, Any]:
        """Send a supported command and return its matching JSON event.

        Unsolicited boot/advertising/routing events are skipped. A firmware
        error event is returned to the caller rather than raised, so callers
        can handle documented states such as discovery_not_implemented.
        """
        if not isinstance(command, str):
            raise TypeError("command_must_be_string")
        command = command.strip()
        try:
            encoded = command.encode("utf-8", errors="strict")
        except UnicodeError as exc:
            raise ValueError("invalid_command_encoding") from exc
        if not encoded or len(encoded) > MAX_COMMAND_BYTES or b"\n" in encoded or b"\r" in encoded:
            raise ValueError("invalid_command_length_or_framing")
        expected_type = _expected_response(command)

        written = self._port.write(encoded + b"\n")
        if written != len(encoded) + 1:
            raise SerialClientError("incomplete_serial_write")

        deadline = time.monotonic() + self._timeout_seconds
        while True:
            if time.monotonic() >= deadline:
                raise SerialTimeout("response_timeout")
            if b"\n" in self._pending:
                line, _, remaining = self._pending.partition(b"\n")
                self._pending = bytearray(remaining)
            else:
                raw = self._port.readline()
                if not raw:
                    continue
                self._pending.extend(raw)
                if len(self._pending) > self._max_event_bytes:
                    raise SerialProtocolError("event_too_large")
                if b"\n" not in self._pending:
                    # PySerial can return a partial line when its per-read
                    # timeout expires; retain bytes until a complete line.
                    continue
                line, _, remaining = self._pending.partition(b"\n")
                self._pending = bytearray(remaining)
            if len(line) > self._max_event_bytes:
                raise SerialProtocolError("event_too_large")
            if line.endswith(b"\r"):
                line = line[:-1]
            try:
                event = json.loads(line.decode("utf-8", errors="strict"))
            except (UnicodeError, json.JSONDecodeError) as exc:
                raise SerialProtocolError("invalid_json_event") from exc
            if not isinstance(event, dict) or not isinstance(event.get("type"), str):
                raise SerialProtocolError("invalid_event_shape")
            if event["type"] == "error" or event["type"] == expected_type:
                return event

    def status(self) -> dict[str, Any]:
        return self.request("status")

    def nodes(self) -> dict[str, Any]:
        return self.request("nodes")

    def presets(self) -> dict[str, Any]:
        return self.request("presets")

    def set_preset(self, preset: str) -> dict[str, Any]:
        if preset not in {"client", "repeater", "gateway"}:
            raise ValueError("invalid_preset")
        return self.request("preset " + preset)

    def close(self) -> None:
        """Close the underlying serial port if it exposes close()."""
        close = getattr(self._port, "close", None)
        if callable(close):
            close()


def open_serial(
    port_name: str,
    *,
    baudrate: int = 115200,
    timeout_seconds: float = DEFAULT_TIMEOUT_SECONDS,
) -> MissionLinkSerialClient:
    """Open a USB serial port using optional PySerial.

    Install PySerial separately using: python -m pip install pyserial.
    This helper only opens the local control interface; it does not enable RF.
    """
    if not isinstance(port_name, str) or not port_name.strip():
        raise ValueError("invalid_port_name")
    if isinstance(baudrate, bool) or not isinstance(baudrate, int) or baudrate <= 0:
        raise ValueError("invalid_baudrate")
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError as exc:
        raise RuntimeError("pyserial_required_install_with_python_m_pip_install_pyserial") from exc
    port = serial.Serial(port_name, baudrate=baudrate, timeout=min(float(timeout_seconds), 0.25))
    return MissionLinkSerialClient(port, timeout_seconds=timeout_seconds)
