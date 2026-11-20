#!/usr/bin/env python3
"""Provision any FirmwareEmbed board over the APLO1 USB Serial protocol."""

import argparse
import getpass
import json
from pathlib import Path
import re
import sys
import time
import warnings

FIELDS = {
    "mode", "ssid", "primary_url", "fallback_url", "ntp_primary", "ntp_fallback",
    "owner", "signer", "chain_id", "gaplo_budget", "expiry", "hashes_per_step",
    "interval_ms", "enabled",
}
ERRORS = {
    "framing", "unavailable", "enrollment", "auth", "memory", "transaction",
    "value", "chunk", "certificate", "upload", "commit", "reset", "command",
}
PUBLIC_REPLIES = {
    "STATUS": re.compile(
        r"status locked=[01] enrolled=[01] ready=[01] pending=[01] restart=[01]"
        r" mode=(?:session|direct|unset) state=[a-z_]+ journal=(?:NONE|0x[0-9a-fA-F]{64})"
    ),
    "DIAG": re.compile(
        r"diag reset_reason=\d+ uptime_ms=\d+ cpu_mhz=\d+ flash_bytes=\d+"
        r" psram_bytes=\d+ heap_free=\d+ heap_largest=\d+"
    ),
}
HEX32 = re.compile(r"[0-9a-fA-F]{64}")


class ProtocolError(Exception):
    pass


class Connection:
    def __init__(self, serial_port, timeout):
        self.serial_port = serial_port
        self.timeout = timeout
        self.request_id = 0

    def request(self, command, *arguments, expected=""):
        self.request_id += 1
        prefix = f"APLO1 {self.request_id} "
        frame = (" ".join([prefix + command, *arguments]) + "\n").encode("ascii")
        if len(frame) > 513:
            raise ProtocolError(f"{command}: request too long")
        if self.serial_port.write(frame) != len(frame):
            raise ProtocolError(f"{command}: incomplete Serial write")
        deadline = time.monotonic() + self.timeout
        buffer = b""
        discard = False
        while time.monotonic() < deadline:
            chunk = self.serial_port.read_until(b"\n", 513)
            ended = chunk.endswith(b"\n")
            if discard or len(buffer) + len(chunk) > 513:
                discard = not ended
                buffer = b""
                continue
            buffer += chunk
            if not ended:
                continue
            reply = buffer.decode("ascii", errors="replace").rstrip("\r\n")
            buffer = b""
            if not reply.startswith(prefix):
                continue
            body = reply[len(prefix):]
            if body.startswith("ERR "):
                code = body[4:]
                message = code if code in ERRORS else "unexpected error"
                raise ProtocolError(f"{command}: {message}")
            if command in PUBLIC_REPLIES and body.startswith("OK "):
                value = body[3:]
                if PUBLIC_REPLIES[command].fullmatch(value):
                    return value
            if command not in PUBLIC_REPLIES and body == ("OK " + expected if expected else "OK"):
                return None
            raise ProtocolError(f"{command}: unexpected reply")
        raise ProtocolError(f"{command}: timed out. Use STATUS to check the device before retrying")


def prompt_secret(prompt):
    with warnings.catch_warnings():
        warnings.simplefilter("error", getpass.GetPassWarning)
        try:
            return getpass.getpass(prompt)
        except getpass.GetPassWarning as error:
            raise ProtocolError("a terminal with hidden input is required") from error


def secret(prompt):
    value = prompt_secret(prompt)
    if not HEX32.fullmatch(value):
        raise ProtocolError("expected exactly 64 hexadecimal characters")
    return value


def wire_value(value):
    if isinstance(value, bool):
        value = int(value)
    if not isinstance(value, (int, str)):
        raise ProtocolError("expected a text or integer configuration value")
    data = str(value).encode("utf-8")
    if len(data) > 224 or any(byte < 32 or byte == 127 for byte in data):
        raise ProtocolError("configuration value exceeds the APLO1 field limits")
    return data.hex() if data else "-"


def apply_inputs(args):
    try:
        values = json.loads(args.config.read_text(encoding="utf-8")) if args.config else {}
    except (ValueError, OSError) as error:
        raise ProtocolError("unable to read the configuration JSON") from error
    if not isinstance(values, dict) or not values.keys() <= FIELDS:
        raise ProtocolError("expected a JSON object with supported public field names")
    if args.ask_wifi_password:
        values["password"] = prompt_secret("Wi-Fi password (empty for open network): ")
    if args.ask_key:
        values["key"] = secret("Mining key: ")
    fields = [(name, wire_value(value)) for name, value in values.items()]
    certificates = []
    for endpoint, path in (("primary", args.primary_ca), ("fallback", args.fallback_ca)):
        if path is None:
            continue
        try:
            with path.open("rb") as source:
                pem = source.read(4097)
        except OSError as error:
            raise ProtocolError(f"unable to read {endpoint} CA") from error
        if len(pem) > 4096 or b"\0" in pem:
            raise ProtocolError(f"{endpoint} CA exceeds the APLO1 PEM limits")
        certificates.append((endpoint, pem))
    if not fields and not certificates:
        raise ProtocolError("no configuration changes supplied")
    return fields, certificates


def apply(connection, fields, certificates):
    connection.request("BEGIN")
    for name, value in fields:
        connection.request("SET", name, value)
    for endpoint, pem in certificates:
        connection.request("CA_BEGIN", endpoint, str(len(pem)))
        for offset in range(0, len(pem), 192):
            connection.request("CA_CHUNK", str(offset), pem[offset:offset + 192].hex())
        connection.request("CA_END")
    connection.request("COMMIT", expected="restart")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="USB Serial device")
    parser.add_argument("--timeout", type=float, default=120, help="seconds per reply (default: 120)")
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("status", "diag", "enroll"):
        sub.add_parser(name)
    edit = sub.add_parser("apply", help="stage and commit configuration")
    edit.add_argument("--config", type=Path, help="JSON with public configuration fields")
    edit.add_argument("--ask-wifi-password", action="store_true")
    edit.add_argument("--ask-key", action="store_true")
    edit.add_argument("--primary-ca", type=Path)
    edit.add_argument("--fallback-ca", type=Path)
    reset = sub.add_parser("reset", help="reset after reconciling any pending transaction")
    reset.add_argument("--journal", required=True)
    args = parser.parse_args()
    if not 0 < args.timeout <= 600:
        parser.error("timeout must be in (0, 600] seconds")
    try:
        import serial
    except ImportError:
        parser.exit(1, "Install pyserial with python3 -m pip install pyserial\n")

    try:
        fields, certificates = apply_inputs(args) if args.command == "apply" else ([], [])
        if args.command == "reset" and not re.fullmatch(r"NONE|0x[0-9a-fA-F]{64}", args.journal):
            raise ProtocolError("journal must be NONE or the exact public transaction hash")
        token = secret("Operator token: ") if args.command in {"apply", "enroll", "reset"} else None
        with serial.Serial(baudrate=115200, timeout=0.25, write_timeout=2) as port:
            port.dtr = port.rts = False
            port.port = args.port
            port.open()
            port.reset_input_buffer()
            connection = Connection(port, args.timeout)
            if args.command in {"status", "diag"}:
                print(connection.request(args.command.upper()))
                return
            if args.command == "enroll":
                connection.request("ENROLL", token, "I_HAVE_PHYSICAL_USB_CONTROL", expected="restart")
            else:
                connection.request("AUTH", token, expected="authenticated")
                if args.command == "apply":
                    apply(connection, fields, certificates)
                else:
                    connection.request(
                        "RESET", args.journal, "ERASE_LOCAL_ONLY_I_HAVE_RECONCILED_CHAIN",
                        expected="restart",
                    )
            print("Saved. Device is restarting.")
    except (ProtocolError, serial.SerialException, UnicodeError, EOFError) as error:
        print(error if isinstance(error, ProtocolError) else "Serial connection or input failed", file=sys.stderr)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
