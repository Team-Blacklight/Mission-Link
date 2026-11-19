import json
import os
from pathlib import Path
import tempfile
import unittest

from host.missionlink_services import ServiceRecord, ServiceRegistry
from host.missionlink_store import RegistryStore, RegistryStoreError


def sample_registry():
    registry = ServiceRegistry()
    registry.register(ServiceRecord(
        service_id="files.home",
        host_id="nas.home",
        display_name="Home NAS",
        capabilities=("file.index", "file.read"),
        transports=("lan",),
        endpoint="https://nas.example.invalid/missionlink",
    ))
    registry.register(ServiceRecord(
        service_id="ai.local",
        host_id="workstation",
        display_name="Local AI",
        capabilities=("ai.inference",),
        transports=("lan", "usb-serial"),
        healthy=False,
    ))
    return registry


class RegistryStoreTests(unittest.TestCase):
    def test_round_trip_preserves_records_and_unhealthy_services(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "nested" / "services.json"
            store = RegistryStore(path)
            original = sample_registry()
            store.save(original)
            loaded = store.load()
            self.assertEqual(
                [x.service_id for x in loaded.list(healthy_only=False)],
                ["ai.local", "files.home"],
            )
            self.assertEqual(loaded.get("files.home"), original.get("files.home"))
            self.assertFalse(loaded.get("ai.local").healthy)

    def test_missing_store_loads_empty_registry(self):
        with tempfile.TemporaryDirectory() as temp:
            self.assertEqual(RegistryStore(Path(temp) / "missing.json").load().list(), ())

    def test_invalid_schema_and_record_are_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "services.json"
            store = RegistryStore(path)
            for payload in (
                [],
                {"schema_version": 99, "services": []},
                {"schema_version": 1, "services": [{"service_id": "../bad"}]},
                {"schema_version": 1, "services": [], "unexpected": True},
            ):
                with self.subTest(payload=payload):
                    path.write_text(json.dumps(payload), encoding="utf-8")
                    with self.assertRaises(RegistryStoreError):
                        store.load()

    def test_duplicate_ids_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "services.json"
            record = {
                "service_id": "duplicate",
                "host_id": "host",
                "display_name": "Service",
                "capabilities": ["test.read"],
            }
            path.write_text(json.dumps({"schema_version": 1, "services": [record, record]}), encoding="utf-8")
            with self.assertRaises(RegistryStoreError):
                RegistryStore(path).load()

    def test_too_large_file_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "services.json"
            path.write_bytes(b" " * (256 * 1024 + 1))
            with self.assertRaisesRegex(RegistryStoreError, "too_large"):
                RegistryStore(path).load()

    def test_symlink_store_rejected(self):
        if not hasattr(os, "symlink"):
            self.skipTest("symlinks are unavailable")
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / "target.json"
            target.write_text('{"schema_version":1,"services":[]}', encoding="utf-8")
            link = Path(temp) / "link.json"
            try:
                link.symlink_to(target)
            except OSError:
                self.skipTest("symlink creation is not permitted")
            with self.assertRaisesRegex(RegistryStoreError, "symlink"):
                RegistryStore(link).load()
            with self.assertRaisesRegex(RegistryStoreError, "symlink"):
                RegistryStore(link).save(ServiceRegistry())

    @unittest.skipUnless(os.name == "posix", "POSIX permissions only")
    def test_saved_file_is_owner_only(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "services.json"
            RegistryStore(path).save(sample_registry())
            self.assertEqual(path.stat().st_mode & 0o777, 0o600)


if __name__ == "__main__":
    unittest.main()
