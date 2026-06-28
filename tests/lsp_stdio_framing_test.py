#!/usr/bin/env python3
"""Verify styio_lspd emits byte-exact LSP stdio framing."""

from __future__ import annotations

import argparse
import json
import queue
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path


STANDARD_BOUNDARY = b"\r\n\r\n"
WINDOWS_TEXT_MODE_BOUNDARY = b"\r\r\n\r\r\n"
LF_ONLY_BOUNDARY = b"\n\n"


class PipeReader(threading.Thread):
    def __init__(self, pipe) -> None:
        super().__init__(daemon=True)
        self._pipe = pipe
        self.chunks: "queue.Queue[bytes | None]" = queue.Queue()

    def run(self) -> None:
        try:
            while True:
                chunk = self._pipe.read(1)
                if not chunk:
                    break
                self.chunks.put(chunk)
        finally:
            self.chunks.put(None)


def file_uri(path: Path) -> str:
    return path.resolve().as_uri()


def fail(message: str) -> None:
    raise AssertionError(message)


def read_until_header(reader: PipeReader, timeout_seconds: float) -> bytes:
    deadline = time.monotonic() + timeout_seconds
    buffer = bytearray()

    while time.monotonic() < deadline:
        remaining = max(0.01, deadline - time.monotonic())
        try:
            chunk = reader.chunks.get(timeout=remaining)
        except queue.Empty:
            break

        if chunk is None:
            break
        buffer.extend(chunk)

        current = bytes(buffer)
        if WINDOWS_TEXT_MODE_BOUNDARY in current:
            fail("styio_lspd emitted Windows text-mode LSP boundary '\\r\\r\\n\\r\\r\\n'")
        if LF_ONLY_BOUNDARY in current and STANDARD_BOUNDARY not in current:
            fail("styio_lspd emitted non-standard LSP boundary '\\n\\n'")
        if STANDARD_BOUNDARY in current:
            return current

    fail(f"timed out waiting for LSP header boundary; received {bytes(buffer)!r}")


def read_body(
    reader: PipeReader, initial: bytes, body_start: int, length: int, timeout_seconds: float
) -> bytes:
    deadline = time.monotonic() + timeout_seconds
    buffer = bytearray(initial)
    body_end = body_start + length

    while len(buffer) < body_end and time.monotonic() < deadline:
        remaining = max(0.01, deadline - time.monotonic())
        try:
            chunk = reader.chunks.get(timeout=remaining)
        except queue.Empty:
            break
        if chunk is None:
            break
        buffer.extend(chunk)

    if len(buffer) < body_end:
        fail(f"timed out waiting for LSP body; expected {length} bytes")
    return bytes(buffer[body_start:body_end])


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True, help="Path to the built styio_lspd executable")
    parser.add_argument("--timeout", type=float, default=10.0)
    args = parser.parse_args()

    binary = Path(args.binary)
    if not binary.exists():
        fail(f"styio_lspd binary not found: {binary}")

    with tempfile.TemporaryDirectory(prefix="styio-lspd-framing-") as workspace:
        workspace_path = Path(workspace)
        request = {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {
                "processId": None,
                "rootUri": file_uri(workspace_path),
                "capabilities": {},
                "trace": "off",
            },
        }
        body = json.dumps(request, separators=(",", ":")).encode("utf-8")
        wire = b"Content-Length: " + str(len(body)).encode("ascii") + STANDARD_BOUNDARY + body

        process = subprocess.Popen(
            [str(binary)],
            cwd=str(workspace_path),
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        assert process.stdin is not None
        assert process.stdout is not None
        stdout_reader = PipeReader(process.stdout)
        stderr_reader = PipeReader(process.stderr)
        stdout_reader.start()
        stderr_reader.start()

        try:
            process.stdin.write(wire)
            process.stdin.flush()

            header_buffer = read_until_header(stdout_reader, args.timeout)
            boundary_index = header_buffer.index(STANDARD_BOUNDARY)
            header = header_buffer[:boundary_index].decode("ascii", errors="replace")
            prefix = "Content-Length:"
            if not header.lower().startswith(prefix.lower()):
                fail(f"missing Content-Length header: {header!r}")
            try:
                content_length = int(header.split(":", 1)[1].strip())
            except ValueError as error:
                fail(f"invalid Content-Length header {header!r}: {error}")

            body_start = boundary_index + len(STANDARD_BOUNDARY)
            response_body = read_body(
                stdout_reader, header_buffer, body_start, content_length, args.timeout
            )
            response = json.loads(response_body.decode("utf-8"))
            if response.get("id") != 1 or "result" not in response:
                fail(f"initialize response missing id/result: {response!r}")

            print("styio_lspd stdio framing is byte-exact")
            return 0
        finally:
            process.kill()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.terminate()


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"lsp_stdio_framing_test failed: {error}", file=sys.stderr)
        raise SystemExit(1)
