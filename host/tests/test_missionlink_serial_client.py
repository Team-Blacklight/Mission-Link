import json
import unittest

from host.missionlink_serial_client import (
    MissionLinkSerialClient,
    SerialClientError,
    SerialProtocolError,
    SerialTimeout,
)


def event(value):
    return json.dumps(value, separators=(",", ":")).encode() + b"\n"


class FakeSerial:
    def __init__(self, lines=(), *, write_count=None):
        self.lines = list(lines)
        self.writes = []
        self.write_count = write_count
        self.closed = False

    def write(self, data):
        self.writes.append(data)
        return len(data) if self.write_count is None else self.write_count

    def readline(self):
        if self.lines:
            return self.lines.pop(0)
        return b""

    def close(self):
        self.closed = True


class SerialClientTests(unittest.TestCase):
    def test_status_skips_unsolicited_events_and_returns_matching_json(self):
        port = FakeSerial([
            event({"type": "boot", "firmware": "0.2.1"}),
            event({"type": "status", "rfEnabled": False, "radioReady": False}),
        ])
        client = MissionLinkSerialClient(port)
        self.assertEqual(client.status()["type"], "status")
        self.assertEqual(port.writes, [b"status\n"])

    def test_partial_serial_reads_are_reassembled(self):
        port = FakeSerial([
            b'{"type":"sta',
            b'tus","rfEnabled":false}',
            b"\n",
        ])
        result = MissionLinkSerialClient(port).status()
        self.assertFalse(result["rfEnabled"])

    def test_nodes_returns_not_ready_diagnostic_without_claiming_discovery(self):
        port = FakeSerial([event({
            "type": "nodes", "items": [], "ready": False,
            "reason": "discovery_not_implemented",
        })])
        result = MissionLinkSerialClient(port).nodes()
        self.assertFalse(result["ready"])
        self.assertEqual(result["reason"], "discovery_not_implemented")

    def test_preset_is_validated_and_response_matched(self):
        port = FakeSerial([event({"type": "preset", "active": "repeater"})])
        self.assertEqual(MissionLinkSerialClient(port).set_preset("repeater")["active"], "repeater")
        with self.assertRaisesRegex(ValueError, "invalid_preset"):
            MissionLinkSerialClient(FakeSerial()).set_preset("unknown")

    def test_firmware_error_is_returned_to_caller(self):
        port = FakeSerial([event({"type": "error", "code": "not_ready", "message": "RF disabled"})])
        result = MissionLinkSerialClient(port).request("status")
        self.assertEqual(result["code"], "not_ready")

    def test_rejects_unsupported_commands_without_writing(self):
        port = FakeSerial()
        with self.assertRaisesRegex(ValueError, "unsupported_command"):
            MissionLinkSerialClient(port).request("send node hello")
        self.assertEqual(port.writes, [])

    def test_rejects_embedded_newline_and_oversized_command(self):
        port = FakeSerial()
        client = MissionLinkSerialClient(port)
        with self.assertRaisesRegex(ValueError, "invalid_command_length_or_framing"):
            client.request("status\nhelp")
        with self.assertRaisesRegex(ValueError, "invalid_command_length_or_framing"):
            client.request("preset " + "x" * 256)
        self.assertEqual(port.writes, [])

    def test_rejects_invalid_json_and_invalid_event_shape(self):
        for line in (b"{not-json}\n", b"[]\n", b'{"value":1}\n'):
            with self.subTest(line=line):
                with self.assertRaises(SerialProtocolError):
                    MissionLinkSerialClient(FakeSerial([line])).status()

    def test_rejects_oversized_event(self):
        line = b'{"type":"boot","x":"' + b"x" * 5000 + b'"}\n'
        with self.assertRaisesRegex(SerialProtocolError, "event_too_large"):
            MissionLinkSerialClient(FakeSerial([line])).status()

    def test_rejects_short_write(self):
        port = FakeSerial(write_count=1)
        with self.assertRaisesRegex(SerialClientError, "incomplete_serial_write"):
            MissionLinkSerialClient(port).status()

    def test_close_closes_port(self):
        port = FakeSerial()
        MissionLinkSerialClient(port).close()
        self.assertTrue(port.closed)

    def test_timeout_when_no_matching_response_arrives(self):
        client = MissionLinkSerialClient(FakeSerial(), timeout_seconds=0.001)
        with self.assertRaises(SerialTimeout):
            client.status()


if __name__ == "__main__":
    unittest.main()
