"""Bounded, atomic JSON persistence for the MissionLink host service registry."""
from __future__ import annotations

import json
import os
from pathlib import Path
import tempfile
from typing import Any

from host.missionlink_services import MAX_SERVICE_COUNT, RegistryError, ServiceRecord, ServiceRegistry

SCHEMA_VERSION = 1
MAX_STORE_BYTES = 256 * 1024


class RegistryStoreError(ValueError):
    """Raised when the persistent registry cannot be safely read or written."""


def record_to_dict(record: ServiceRecord) -> dict[str, Any]:
    return {
        "service_id": record.service_id,
        "host_id": record.host_id,
        "display_name": record.display_name,
        "capabilities": list(record.capabilities),
        "transports": list(record.transports),
        "endpoint": record.endpoint,
        "healthy": record.healthy,
        "max_inline_request_bytes": record.max_inline_request_bytes,
    }


def record_from_dict(value: Any) -> ServiceRecord:
    if not isinstance(value, dict):
        raise RegistryStoreError("invalid_service_record")
    allowed = {
        "service_id", "host_id", "display_name", "capabilities", "transports",
        "endpoint", "healthy", "max_inline_request_bytes",
    }
    if set(value) - allowed:
        raise RegistryStoreError("unknown_service_record_field")
    required = {"service_id", "host_id", "display_name", "capabilities"}
    if not required.issubset(value):
        raise RegistryStoreError("missing_service_record_field")
    capabilities = value["capabilities"]
    transports = value.get("transports", ["lan"])
    if (not isinstance(capabilities, list) or not isinstance(transports, list)
            or any(not isinstance(x, str) for x in capabilities + transports)):
        raise RegistryStoreError("invalid_service_record")
    try:
        record = ServiceRecord(
            service_id=value["service_id"],
            host_id=value["host_id"],
            display_name=value["display_name"],
            capabilities=tuple(capabilities),
            transports=tuple(transports),
            endpoint=value.get("endpoint"),
            healthy=value.get("healthy", True),
            max_inline_request_bytes=value.get("max_inline_request_bytes", 96),
        )
        record.validate()
    except (RegistryError, TypeError, ValueError) as exc:
        raise RegistryStoreError(str(exc) or "invalid_service_record") from None
    return record


class RegistryStore:
    """Atomic versioned JSON store; POSIX files are owner-readable/writable only."""

    def __init__(self, path: str | os.PathLike[str]) -> None:
        self.path = Path(path)

    def load(self) -> ServiceRegistry:
        registry = ServiceRegistry()
        try:
            if self.path.is_symlink():
                raise RegistryStoreError("registry_store_must_not_be_symlink")
            if not self.path.exists():
                return registry
            size = self.path.stat().st_size
            if size > MAX_STORE_BYTES:
                raise RegistryStoreError("registry_store_too_large")
            raw = self.path.read_bytes()
            payload = json.loads(raw.decode("utf-8"))
        except RegistryStoreError:
            raise
        except (OSError, UnicodeDecodeError, json.JSONDecodeError):
            raise RegistryStoreError("registry_store_unreadable_or_invalid") from None

        if not isinstance(payload, dict) or set(payload) != {"schema_version", "services"}:
            raise RegistryStoreError("invalid_registry_store_schema")
        if payload["schema_version"] != SCHEMA_VERSION or isinstance(payload["schema_version"], bool):
            raise RegistryStoreError("unsupported_registry_store_version")
        records = payload["services"]
        if not isinstance(records, list) or len(records) > MAX_SERVICE_COUNT:
            raise RegistryStoreError("invalid_registry_store_services")
        try:
            for value in records:
                registry.register(record_from_dict(value))
        except (RegistryError, RegistryStoreError):
            raise RegistryStoreError("invalid_registry_store_record") from None
        if len(registry.list(healthy_only=False)) != len(records):
            raise RegistryStoreError("duplicate_service_id")
        return registry

    def save(self, registry: ServiceRegistry) -> None:
        if not isinstance(registry, ServiceRegistry):
            raise RegistryStoreError("invalid_registry")
        payload = {
            "schema_version": SCHEMA_VERSION,
            "services": [record_to_dict(item) for item in registry.list(healthy_only=False)],
        }
        raw = (json.dumps(payload, ensure_ascii=True, separators=(",", ":")) + "\n").encode("utf-8")
        if len(raw) > MAX_STORE_BYTES:
            raise RegistryStoreError("registry_store_too_large")

        parent = self.path.parent
        temporary: str | None = None
        try:
            parent.mkdir(parents=True, exist_ok=True)
            if self.path.is_symlink():
                raise RegistryStoreError("registry_store_must_not_be_symlink")
            fd, temporary = tempfile.mkstemp(prefix=f".{self.path.name}.", suffix=".tmp", dir=parent)
            try:
                if os.name == "posix":
                    os.fchmod(fd, 0o600)
                with os.fdopen(fd, "wb") as stream:
                    stream.write(raw)
                    stream.flush()
                    os.fsync(stream.fileno())
                os.replace(temporary, self.path)
                temporary = None
                if os.name == "posix":
                    os.chmod(self.path, 0o600)
                try:
                    dir_fd = os.open(parent, os.O_RDONLY)
                    try:
                        os.fsync(dir_fd)
                    finally:
                        os.close(dir_fd)
                except OSError:
                    # Some filesystems do not support directory fsync.
                    pass
            except Exception:
                # fdopen owns fd after success; if it failed before ownership,
                # closing an already-closed descriptor is harmlessly guarded.
                try:
                    os.close(fd)
                except OSError:
                    pass
                raise
        except RegistryStoreError:
            raise
        except OSError:
            raise RegistryStoreError("registry_store_write_failed") from None
        finally:
            if temporary is not None:
                try:
                    os.unlink(temporary)
                except OSError:
                    pass
