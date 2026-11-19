import unittest

from host.missionlink_services import (
    DEFAULT_MESH_INLINE_MAX,
    RegistryError,
    ServiceRecord,
    ServiceRegistry,
    mesh_inline_allowed,
)


def service(**overrides):
    values = dict(
        service_id="files.main",
        host_id="nas.home",
        display_name="Home NAS",
        capabilities=("file.index", "file.read"),
        transports=("lan",),
        endpoint="https://nas.example.invalid/missionlink",
        healthy=True,
        max_inline_request_bytes=DEFAULT_MESH_INLINE_MAX,
    )
    values.update(overrides)
    return ServiceRecord(**values)


class ServiceRegistryTests(unittest.TestCase):
    def test_register_and_find_by_capability(self):
        registry = ServiceRegistry()
        registry.register(service())
        registry.register(service(
            service_id="ai.local",
            host_id="workstation",
            display_name="Local AI",
            capabilities=("ai.inference",),
        ))
        self.assertEqual([x.service_id for x in registry.list("ai.inference")], ["ai.local"])
        self.assertEqual(registry.get("files.main").host_id, "nas.home")

    def test_invalid_ids_and_untrusted_remote_endpoint_rejected(self):
        with self.assertRaises(RegistryError):
            service(service_id="../secret").validate()
        with self.assertRaises(RegistryError):
            service(endpoint="http://192.168.1.20:8000").validate()

    def test_endpoint_parser_rejects_lookalikes_and_bad_ports(self):
        for endpoint in (
            "http://localhost.evil:8000",
            "http://127.0.0.1.evil:8000",
            "http://localhost:99999",
            "http://localhost:notaport",
            "https:///missing-host",
            "ftp://example.com/file",
            "https://nas.example.invalid/path?token=secret",
            "https://nas.example.invalid/path#fragment",
            "https://nas.example.invalid\\\\path",
            "https://nas.example.invalid/path with-space",
            "https://nas.example.invalid/path\\nheader: value",
        ):
            with self.subTest(endpoint=endpoint), self.assertRaises(RegistryError):
                service(endpoint=endpoint).validate()

    def test_loopback_http_and_remote_https_are_allowed(self):
        for endpoint in (
            "http://localhost:8000/api",
            "http://127.0.0.1:8000",
            "http://[::1]:8000",
            "https://nas.example.invalid/missionlink",
        ):
            with self.subTest(endpoint=endpoint):
                service(endpoint=endpoint).validate()

    def test_runtime_types_are_checked(self):
        for record in (
            service(service_id=None),
            service(host_id=12),
            service(display_name=" "),
            service(capabilities=("file.read", 3)),
            service(transports=("lan", "lan")),
            service(max_inline_request_bytes=True),
            service(healthy="yes"),
            service(endpoint=42),
        ):
            with self.subTest(record=record), self.assertRaises(RegistryError):
                record.validate()
        with self.assertRaises(RegistryError):
            ServiceRegistry().register(object())
        with self.assertRaises(ValueError):
            ServiceRegistry(max_services=True)

    def test_bounds_and_duplicate_capabilities(self):
        with self.assertRaises(RegistryError):
            service(capabilities=("file.read", "file.read")).validate()
        with self.assertRaises(RegistryError):
            service(max_inline_request_bytes=181).validate()
        registry = ServiceRegistry(max_services=1)
        registry.register(service())
        with self.assertRaises(RegistryError):
            registry.register(service(service_id="another"))

    def test_health_filter_and_unregister(self):
        registry = ServiceRegistry()
        registry.register(service(healthy=False))
        self.assertEqual(registry.list(), ())
        self.assertEqual(len(registry.list(healthy_only=False)), 1)
        self.assertTrue(registry.unregister("files.main"))
        self.assertFalse(registry.unregister("files.main"))

    def test_mesh_inline_policy_is_conservative(self):
        item = service()
        self.assertTrue(mesh_inline_allowed(64, item))
        self.assertFalse(mesh_inline_allowed(97, item))
        self.assertFalse(mesh_inline_allowed(181, item))
        self.assertFalse(mesh_inline_allowed(True, item))
        self.assertFalse(mesh_inline_allowed(-1, item))
        self.assertFalse(mesh_inline_allowed(4, object()))


if __name__ == "__main__":
    unittest.main()
