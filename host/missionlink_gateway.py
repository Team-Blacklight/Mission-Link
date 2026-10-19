"""Loopback-only, token-authenticated HTTP API for the MissionLink host registry.

This is a local control-plane API, not a mesh transport. It never opens RF,
proxies requests to registered endpoints, or accepts remote bind addresses.
"""
from __future__ import annotations

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import hmac
import json
import secrets
import threading
from pathlib import Path
from typing import Any
from urllib.parse import parse_qs, urlsplit

from host.missionlink_services import RegistryError, ServiceRecord, ServiceRegistry
from host.missionlink_store import RegistryStore, RegistryStoreError

API_VERSION = 1
MAX_BODY_BYTES = 16 * 1024


def _record_dict(record: ServiceRecord) -> dict[str, Any]:
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


def _record_from_dict(value: Any) -> ServiceRecord:
    if not isinstance(value, dict):
        raise RegistryError("invalid_service_record")
    allowed = {
        "service_id", "host_id", "display_name", "capabilities", "transports",
        "endpoint", "healthy", "max_inline_request_bytes",
    }
    if set(value) - allowed:
        raise RegistryError("unknown_service_record_field")
    required = {"service_id", "host_id", "display_name", "capabilities"}
    if not required.issubset(value):
        raise RegistryError("missing_service_record_field")
    capabilities = value["capabilities"]
    transports = value.get("transports", ["lan"])
    if not isinstance(capabilities, list) or not isinstance(transports, list):
        raise RegistryError("invalid_service_record")
    if any(not isinstance(item, str) for item in capabilities + transports):
        raise RegistryError("invalid_service_record")
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
    return record


def make_handler(registry: ServiceRegistry, bearer_token: str, store: RegistryStore | None = None):
    """Create an isolated handler; callers must keep the token secret."""
    if not isinstance(bearer_token, str) or len(bearer_token) < 24:
        raise ValueError("bearer_token_must_be_at_least_24_characters")

    mutation_lock = threading.RLock()

    class GatewayHandler(BaseHTTPRequestHandler):
        server_version = "MissionLinkGateway/1"
        sys_version = ""

        def log_message(self, fmt: str, *args: object) -> None:
            # Avoid logging request headers, tokens, and user-supplied values.
            return

        def _send(self, status: int, payload: dict[str, Any]) -> None:
            body = json.dumps(payload, separators=(",", ":"), ensure_ascii=True).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.end_headers()
            self.wfile.write(body)

        def _authorized(self) -> bool:
            supplied = self.headers.get("Authorization", "")
            expected = "Bearer " + bearer_token
            if not hmac.compare_digest(supplied.encode("utf-8"), expected.encode("utf-8")):
                self._send(401, {"ok": False, "error": "unauthorized", "api_version": API_VERSION})
                return False
            return True

        def _read_json(self) -> Any:
            content_type = self.headers.get("Content-Type", "").split(";", 1)[0].strip().lower()
            if content_type != "application/json":
                raise RegistryError("content_type_must_be_application_json")
            raw_length = self.headers.get("Content-Length")
            if raw_length is None or not raw_length.isdigit():
                raise RegistryError("invalid_content_length")
            length = int(raw_length)
            if length > MAX_BODY_BYTES:
                raise OverflowError("request_body_too_large")
            if length == 0:
                raise RegistryError("empty_request_body")
            try:
                return json.loads(self.rfile.read(length).decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError):
                raise RegistryError("invalid_json") from None

        def do_GET(self) -> None:
            parsed = urlsplit(self.path)
            if parsed.path == "/v1/health":
                self._send(200, {"ok": True, "api_version": API_VERSION})
                return
            if not self._authorized():
                return
            if parsed.path == "/v1/services":
                query = parse_qs(parsed.query, strict_parsing=False)
                if set(query) - {"capability", "healthy"}:
                    self._send(400, {"ok": False, "error": "unknown_query_parameter"})
                    return
                healthy_values = query.get("healthy", ["true"])
                if len(healthy_values) != 1 or healthy_values[0] not in ("true", "false"):
                    self._send(400, {"ok": False, "error": "invalid_health_filter"})
                    return
                capability_values = query.get("capability", [None])
                if len(capability_values) != 1:
                    self._send(400, {"ok": False, "error": "invalid_capability_filter"})
                    return
                try:
                    records = registry.list(
                        capability=capability_values[0],
                        healthy_only=healthy_values[0] == "true",
                    )
                except RegistryError as exc:
                    self._send(400, {"ok": False, "error": str(exc)})
                    return
                self._send(200, {"ok": True, "api_version": API_VERSION,
                                 "services": [_record_dict(record) for record in records]})
                return
            if parsed.path.startswith("/v1/services/"):
                service_id = parsed.path.removeprefix("/v1/services/")
                if not service_id or "/" in service_id:
                    self._send(404, {"ok": False, "error": "not_found"})
                    return
                record = registry.get(service_id)
                if record is None:
                    self._send(404, {"ok": False, "error": "service_not_found"})
                else:
                    self._send(200, {"ok": True, "api_version": API_VERSION,
                                     "service": _record_dict(record)})
                return
            self._send(404, {"ok": False, "error": "not_found", "api_version": API_VERSION})

        def do_POST(self) -> None:
            if not self._authorized():
                return
            if urlsplit(self.path).path != "/v1/services":
                self._send(404, {"ok": False, "error": "not_found", "api_version": API_VERSION})
                return
            try:
                record = _record_from_dict(self._read_json())
            except OverflowError:
                self._send(413, {"ok": False, "error": "request_body_too_large"})
                return
            except RegistryError as exc:
                self._send(400, {"ok": False, "error": str(exc)})
                return
            except (TypeError, ValueError):
                self._send(400, {"ok": False, "error": "invalid_service_record"})
                return
            with mutation_lock:
                previous = registry.get(record.service_id)
                try:
                    registry.register(record)
                    if store is not None:
                        store.save(registry)
                except (RegistryError, RegistryStoreError, OSError):
                    if previous is None:
                        registry.unregister(record.service_id)
                    else:
                        registry.register(previous)
                    self._send(500, {"ok": False, "error": "registry_persistence_failed"})
                    return
            self._send(201, {"ok": True, "api_version": API_VERSION,
                             "service": _record_dict(record)})

        def do_DELETE(self) -> None:
            if not self._authorized():
                return
            path = urlsplit(self.path).path
            prefix = "/v1/services/"
            if not path.startswith(prefix) or not path[len(prefix):] or "/" in path[len(prefix):]:
                self._send(404, {"ok": False, "error": "not_found", "api_version": API_VERSION})
                return
            service_id = path[len(prefix):]
            with mutation_lock:
                previous = registry.get(service_id)
                if previous is None:
                    self._send(404, {"ok": False, "error": "service_not_found", "api_version": API_VERSION})
                    return
                registry.unregister(service_id)
                try:
                    if store is not None:
                        store.save(registry)
                except (RegistryStoreError, OSError):
                    registry.register(previous)
                    self._send(500, {"ok": False, "error": "registry_persistence_failed"})
                    return
            self._send(200, {"ok": True, "api_version": API_VERSION, "removed": service_id})

    return GatewayHandler


class LoopbackGatewayServer(ThreadingHTTPServer):
    """HTTP server that refuses non-loopback bind addresses."""

    daemon_threads = True
    allow_reuse_address = False

    def __init__(self, address: tuple[str, int], registry: ServiceRegistry, bearer_token: str, store: RegistryStore | None = None):
        import ipaddress

        host, _port = address
        try:
            loopback = ipaddress.ip_address(host).is_loopback
        except ValueError:
            loopback = host.lower().rstrip(".") == "localhost"
        if not loopback:
            raise ValueError("gateway_must_bind_to_loopback")
        super().__init__(address, make_handler(registry, bearer_token, store))


def serve(registry: ServiceRegistry | None = None, host: str = "127.0.0.1",
          port: int = 8765, bearer_token: str | None = None,
          store_path: str | Path = "~/.missionlink/services.json") -> None:
    """Run the local gateway. A random token is generated when none is supplied."""
    token = bearer_token or secrets.token_urlsafe(32)
    store = RegistryStore(Path(store_path).expanduser())
    loaded_registry = registry if registry is not None else store.load()
    # Persist the initial validated snapshot so a newly created store exists.
    store.save(loaded_registry)
    server = LoopbackGatewayServer((host, port), loaded_registry, token, store)
    print(f"MissionLink local gateway: http://{host}:{server.server_port}")
    print(f"Bearer token (keep private): {token}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Run the loopback-only MissionLink host registry API")
    parser.add_argument("--host", default="127.0.0.1", help="loopback address only")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--store", default="~/.missionlink/services.json", help="persistent registry JSON path")
    args = parser.parse_args()
    serve(host=args.host, port=args.port, store_path=args.store)
