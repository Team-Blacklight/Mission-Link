import json
import tempfile
import threading
import unittest
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, urlopen

from host.missionlink_gateway import LoopbackGatewayServer
from host.missionlink_services import ServiceRegistry
from host.missionlink_store import RegistryStore


TOKEN = "persistent-gateway-test-token-long-enough"


class PersistentGatewayTests(unittest.TestCase):
    def test_mutations_survive_reload(self):
        with tempfile.TemporaryDirectory() as temp:
            store = RegistryStore(Path(temp) / "services.json")
            registry = ServiceRegistry()
            store.save(registry)
            server = LoopbackGatewayServer(("127.0.0.1", 0), registry, TOKEN, store)
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            base = f"http://127.0.0.1:{server.server_port}"
            try:
                payload = {
                    "service_id": "files.main",
                    "host_id": "nas.home",
                    "display_name": "Home NAS",
                    "capabilities": ["file.index"],
                }
                request = Request(base + "/v1/services", data=json.dumps(payload).encode(), headers={
                    "Authorization": f"Bearer {TOKEN}",
                    "Content-Type": "application/json",
                }, method="POST")
                with urlopen(request, timeout=2) as response:
                    self.assertEqual(response.status, 201)
                self.assertEqual(store.load().get("files.main").display_name, "Home NAS")

                delete = Request(base + "/v1/services/files.main", headers={
                    "Authorization": f"Bearer {TOKEN}",
                }, method="DELETE")
                with urlopen(delete, timeout=2) as response:
                    self.assertEqual(response.status, 200)
                self.assertIsNone(store.load().get("files.main"))
            finally:
                server.shutdown()
                server.server_close()
                thread.join(timeout=2)

    def test_failed_persistence_rolls_back_registry_mutation(self):
        class BrokenStore:
            def save(self, _registry):
                raise OSError("simulated disk failure")

        registry = ServiceRegistry()
        server = LoopbackGatewayServer(("127.0.0.1", 0), registry, TOKEN, BrokenStore())
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base = f"http://127.0.0.1:{server.server_port}"
        try:
            payload = {
                "service_id": "ai.local",
                "host_id": "workstation",
                "display_name": "Local AI",
                "capabilities": ["ai.inference"],
            }
            request = Request(base + "/v1/services", data=json.dumps(payload).encode(), headers={
                "Authorization": f"Bearer {TOKEN}",
                "Content-Type": "application/json",
            }, method="POST")
            with self.assertRaises(HTTPError) as caught:
                urlopen(request, timeout=2)
            self.assertEqual(caught.exception.code, 500)
            self.assertIsNone(registry.get("ai.local"))
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)


if __name__ == "__main__":
    unittest.main()
