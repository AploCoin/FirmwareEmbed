#!/usr/bin/env python3
"""Exercise the board-independent client without a Serial device."""

from collections import deque
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from configure import Connection, ProtocolError, apply, wire_value


class Port:
    def __init__(self, replies=(), automatic=False):
        self.replies = deque(replies)
        self.frames = []
        self.automatic = automatic

    def write(self, frame):
        self.frames.append(frame)
        if self.automatic:
            version, id, command, *_ = frame.decode().split()
            suffix = " restart" if command == "COMMIT" else ""
            self.replies.append(f"{version} {id} OK{suffix}\n".encode())
        return len(frame)

    def read_until(self, suffix, limit):
        return self.replies.popleft() if self.replies else b""


class ConfiguratorTests(unittest.TestCase):
    def test_chunks_and_commit_order(self):
        port = Port(automatic=True)
        pem = b"A" * 4096
        apply(Connection(port, 1), [("ssid", wire_value("Demo"))], [("fallback", pem)])
        frames = [frame.decode().split()[2:] for frame in port.frames]
        self.assertEqual(frames[:3], [["BEGIN"], ["SET", "ssid", "44656d6f"],
                                      ["CA_BEGIN", "fallback", "4096"]])
        self.assertEqual(frames[-2:], [["CA_END"], ["COMMIT"]])
        self.assertEqual([int(frame[1]) for frame in frames[3:-2]], list(range(0, 4096, 192)))
        self.assertEqual(b"".join(bytes.fromhex(frame[2]) for frame in frames[3:-2]), pem)
        self.assertLessEqual(max(map(len, port.frames)), 513)

    def test_split_diagnostics_and_stale_replies(self):
        port = Port([
            b"ROM boot\r\n", b"APLO1 17 OK garbage\n", b"APLO1 1 OK diag reset_",
            b"reason=0 uptime_ms=20 cpu_mhz=160 flash_bytes=4194304 psram_bytes=0 "
            b"heap_free=32000 heap_largest=16000\r\n",
        ])
        self.assertIn("heap_free=32000", Connection(port, 1).request("DIAG"))

    def test_device_error_cannot_echo_a_secret(self):
        port = Port([b"APLO1 1 ERR privatefixture\n"])
        with self.assertRaisesRegex(ProtocolError, r"^AUTH: unexpected error$"):
            Connection(port, 1).request("AUTH", "fixture", expected="authenticated")

    def test_field_limits(self):
        self.assertEqual(wire_value(True), "31")
        self.assertEqual(wire_value(""), "-")
        for value in ("a" * 225, "\0", []):
            with self.assertRaises(ProtocolError):
                wire_value(value)


if __name__ == "__main__":
    unittest.main()
