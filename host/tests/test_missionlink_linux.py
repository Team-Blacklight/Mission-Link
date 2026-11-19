import unittest

from host.missionlink_linux import MissionLinkError, parse_response, request


class FakeSerial:
    def __init__(self, lines):
        self.lines = list(lines)
        self.written = b""
        self.reset = False
        self.flushed = False

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        return False

    def reset_input_buffer(self):
        self.reset = True

    def write(self, data):
        self.written += data
        return len(data)

    def flush(self):
        self.flushed = True

    def readline(self):
        return self.lines.pop(0) if self.lines else b""


class LinuxClientTests(unittest.TestCase):
    def test_parse_status_json(self):
        result = parse_response(b'{"type":"status","rfEnabled":false}\r\n')
        self.assertEqual(result["type"], "status")
        self.assertIs(result["rfEnabled"], False)

    def test_rejects_malformed_or_oversized_responses(self):
        for line in (b"", b"not json", b"[]" , b'{"ok":true}', b"x" * 4097):
            with self.subTest(line=line[:20]), self.assertRaises(MissionLinkError):
                parse_response(line)

    def test_firmware_error_is_not_hidden(self):
        with self.assertRaisesRegex(MissionLinkError, "not_ready"):
            parse_response('{"type":"error","code":"not_ready","message":"unavailable"}')

    def test_request_sends_command_and_parses_reply(self):
        device = FakeSerial([b'boot chatter\n',
                             b'{"type":"status","rfEnabled":false}\n'])
        result = request("/dev/fake", "status", timeout=0.2,
                         serial_factory=lambda _port, _timeout: device)
        self.assertEqual(result["type"], "status")
        self.assertEqual(device.written, b"status\n")
        self.assertTrue(device.reset)
        self.assertTrue(device.flushed)

    def test_only_known_commands_are_allowed(self):
        for command in ("send 123 hello", "preset CliPeatWay", "anything"):
            with self.subTest(command=command), self.assertRaisesRegex(
                MissionLinkError, "unsupported_or_unavailable_command"
            ):
                request("/dev/fake", command, serial_factory=lambda *_: None)

    def test_firmware_rejection_propagates(self):
        device = FakeSerial([b'{"type":"error","code":"not_ready","message":"TX blocked"}\n'])
        with self.assertRaisesRegex(MissionLinkError, "not_ready"):
            request("/dev/fake", "status", timeout=0.2,
                    serial_factory=lambda *_: device)


if __name__ == "__main__":
    unittest.main()
