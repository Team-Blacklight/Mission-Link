"""Platform-independent MissionLink host service registry.

This module is transport-agnostic: a non-ESP32-S3 computer can advertise local
services to a MissionLink gateway without pretending that the host itself is a
LoRa radio node. It deliberately does not open sockets or enable RF.
"""
from __future__ import annotations

from dataclasses import dataclass
import ipaddress
import re
from typing import Iterable
from urllib.parse import urlsplit

SERVICE_ID_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{0,63}$")
CAPABILITY_RE = SERVICE_ID_RE
# MissionLink protocol v1 currently allows at most 180 payload bytes before
# any future encryption overhead. This is a policy ceiling, not a guarantee
# that every radio configuration can carry that many bytes.
CURRENT_MESH_PAYLOAD_MAX = 180
DEFAULT_MESH_INLINE_MAX = 96
MAX_SERVICE_COUNT = 128
MAX_CAPABILITIES = 16
ALLOWED_TRANSPORTS = frozenset({"lan", "usb-serial", "bluetooth"})


class RegistryError(ValueError):
    """Raised when a service registration violates the bounded schema."""


def _is_valid_endpoint(endpoint: str) -> bool:
    """Accept loopback HTTP or HTTPS base URLs without ambiguous URL syntax.

    This validates metadata only; it does not make the endpoint trusted or
    authorize a caller to connect to it. Any future connector must separately
    implement authentication, authorization, and network-access policy.
    """
    # urlsplit tolerates or normalizes some control characters. Reject them
    # before parsing, and reject backslashes to avoid parser disagreement.
    if not endpoint or any(ord(ch) <= 0x20 or ord(ch) == 0x7F for ch in endpoint) or "\\" in endpoint:
        return False
    try:
        parsed = urlsplit(endpoint)
        # Accessing .port validates the port syntax/range.
        _ = parsed.port
    except (ValueError, UnicodeError):
        return False

    # Service endpoints are base URLs, not URLs carrying fragments or
    # per-request query parameters (which can accidentally contain secrets).
    if parsed.fragment or parsed.query:
        return False
    if not parsed.hostname or parsed.username is not None or parsed.password is not None:
        return False

    if parsed.scheme == "https":
        return True

    if parsed.scheme != "http" or parsed.port is None:
        return False
    hostname = parsed.hostname.rstrip(".").lower()
    if hostname == "localhost":
        return True
    try:
        return ipaddress.ip_address(hostname).is_loopback
    except ValueError:
        return False


@dataclass(frozen=True)
class ServiceRecord:
    service_id: str
    host_id: str
    display_name: str
    capabilities: tuple[str, ...]
    transports: tuple[str, ...] = ("lan",)
    endpoint: str | None = None
    healthy: bool = True
    max_inline_request_bytes: int = DEFAULT_MESH_INLINE_MAX

    def validate(self) -> None:
        if not isinstance(self.service_id, str) or not SERVICE_ID_RE.fullmatch(self.service_id):
            raise RegistryError("invalid_service_id")
        if not isinstance(self.host_id, str) or not SERVICE_ID_RE.fullmatch(self.host_id):
            raise RegistryError("invalid_host_id")
        if not isinstance(self.display_name, str) or not self.display_name.strip() or len(self.display_name) > 80:
            raise RegistryError("invalid_display_name")
        if not isinstance(self.capabilities, tuple) or not self.capabilities or len(self.capabilities) > MAX_CAPABILITIES:
            raise RegistryError("invalid_capabilities")
        if any(not isinstance(cap, str) for cap in self.capabilities):
            raise RegistryError("invalid_capability")
        if len(set(self.capabilities)) != len(self.capabilities):
            raise RegistryError("duplicate_capability")
        if any(not CAPABILITY_RE.fullmatch(cap) for cap in self.capabilities):
            raise RegistryError("invalid_capability")
        if not isinstance(self.transports, tuple) or not self.transports:
            raise RegistryError("invalid_transport")
        if any(not isinstance(transport, str) or transport not in ALLOWED_TRANSPORTS for transport in self.transports):
            raise RegistryError("invalid_transport")
        if len(set(self.transports)) != len(self.transports):
            raise RegistryError("duplicate_transport")
        if isinstance(self.max_inline_request_bytes, bool) or not isinstance(self.max_inline_request_bytes, int):
            raise RegistryError("invalid_inline_limit")
        if not 1 <= self.max_inline_request_bytes <= DEFAULT_MESH_INLINE_MAX:
            raise RegistryError("inline_limit_out_of_range")
        if not isinstance(self.healthy, bool):
            raise RegistryError("invalid_health")
        if self.endpoint is not None:
            if not isinstance(self.endpoint, str) or len(self.endpoint) > 512:
                raise RegistryError("endpoint_too_long")
            # Mesh announcements must not include arbitrary endpoint URLs.
            # Remote endpoints require HTTPS; plaintext HTTP is loopback-only.
            if not _is_valid_endpoint(self.endpoint):
                raise RegistryError("endpoint_must_be_local_http_or_https")


class ServiceRegistry:
    """Bounded in-memory registry for services reachable through a host bridge."""

    def __init__(self, max_services: int = MAX_SERVICE_COUNT) -> None:
        if isinstance(max_services, bool) or not isinstance(max_services, int) or not 1 <= max_services <= MAX_SERVICE_COUNT:
            raise ValueError("max_services_out_of_range")
        self._max_services = max_services
        self._records: dict[str, ServiceRecord] = {}

    def register(self, record: ServiceRecord) -> None:
        if not isinstance(record, ServiceRecord):
            raise RegistryError("invalid_service_record")
        record.validate()
        if record.service_id not in self._records and len(self._records) >= self._max_services:
            raise RegistryError("registry_full")
        self._records[record.service_id] = record

    def unregister(self, service_id: str) -> bool:
        return self._records.pop(service_id, None) is not None

    def get(self, service_id: str) -> ServiceRecord | None:
        return self._records.get(service_id)

    def list(self, capability: str | None = None, healthy_only: bool = True) -> tuple[ServiceRecord, ...]:
        if not isinstance(healthy_only, bool):
            raise RegistryError("invalid_health_filter")
        records: Iterable[ServiceRecord] = self._records.values()
        if healthy_only:
            records = (record for record in records if record.healthy)
        if capability is not None:
            if not isinstance(capability, str) or not CAPABILITY_RE.fullmatch(capability):
                raise RegistryError("invalid_capability_filter")
            records = (record for record in records if capability in record.capabilities)
        return tuple(sorted(records, key=lambda item: item.service_id))


def mesh_inline_allowed(payload_bytes: int, service: ServiceRecord) -> bool:
    """Whether a small request fits the conservative mesh-inline policy.

    Bulk file data, model weights, and large AI inputs/results should use a
    separately authenticated direct transport, not be tunneled through LoRa.
    """
    return (
        isinstance(payload_bytes, int)
        and not isinstance(payload_bytes, bool)
        and isinstance(service, ServiceRecord)
        and 0 <= payload_bytes <= service.max_inline_request_bytes
        and payload_bytes <= CURRENT_MESH_PAYLOAD_MAX
    )
