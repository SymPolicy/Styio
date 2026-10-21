#!/usr/bin/env python3
"""Verify styio_lspd emits byte-exact LSP stdio framing."""

from __future__ import annotations

import argparse
import json
import os
import queue
import shutil
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


def task_resource_parity(binary: Path, evidence_dir: Path | None) -> int:
    """Exercise the selected server, including its request-driven semantic drain."""
    fixture = Path(__file__).resolve().parent / "features/state_resources/t13_task_history_capture.styio"
    if evidence_dir is not None:
        evidence_dir = evidence_dir.resolve()
        evidence_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="styio-lsp-task-") as temporary:
        workspace = evidence_dir / "workspace" if evidence_dir else Path(temporary)
        workspace.mkdir(parents=True, exist_ok=True)
        document = workspace / fixture.name
        shutil.copyfile(fixture, document)
        good = document.read_text(encoding="utf-8")
        assert "[-1]" in good
        malformed = good.replace("[-1]", "[)]", 1)
        uri = file_uri(document)
        transcript = []
        environment = os.environ.copy()
        environment["XDG_CACHE_HOME"] = str(workspace / "cache")
        stderr_path = (evidence_dir or Path(temporary)) / "server.stderr.log"
        with stderr_path.open("wb") as stderr:
            process = subprocess.Popen(
                [str(binary)], cwd=workspace, env=environment,
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=stderr,
            )
            assert process.stdin is not None and process.stdout is not None

            def send(method, params, request_id=None):
                message = {"jsonrpc": "2.0", "method": method, "params": params}
                if request_id is not None:
                    message["id"] = request_id
                transcript.append({"direction": "send", "message": message})
                body = json.dumps(message, separators=(",", ":")).encode("utf-8")
                process.stdin.write(b"Content-Length: " + str(len(body)).encode("ascii") + STANDARD_BOUNDARY + body)
                process.stdin.flush()

            def receive():
                # EOF is a failure; elapsed time is not a reason to kill a viable server.
                header = process.stdout.readline()
                assert header.startswith(b"Content-Length: ") and header.endswith(b"\r\n"), header
                assert process.stdout.readline() == b"\r\n"
                body = process.stdout.read(int(header.split(b":", 1)[1]))
                message = json.loads(body)
                transcript.append({"direction": "receive", "message": message})
                return message

            def through_response(request_id):
                publications = []
                while True:
                    message = receive()
                    if message.get("id") == request_id:
                        assert "result" in message, message
                        return publications
                    assert message.get("method") == "textDocument/publishDiagnostics", message
                    publications.append(message["params"])

            def barrier(request_id):
                send("textDocument/documentSymbol", {"textDocument": {"uri": uri}}, request_id)
                return through_response(request_id)

            stages = []
            try:
                send("initialize", {"rootUri": file_uri(workspace), "capabilities": {}}, 1)
                assert through_response(1) == []
                send("initialized", {})
                for version, source in ((1, good), (2, malformed), (3, good)):
                    if version == 1:
                        send("textDocument/didOpen", {"textDocument": {
                            "uri": uri, "languageId": "styio", "version": version, "text": source}})
                    else:
                        send("textDocument/didChange", {"textDocument": {"uri": uri, "version": version},
                            "contentChanges": [{"text": source}]})
                    publications = barrier(version * 10)
                    document.write_text(source, encoding="utf-8")
                    # didSave has no version field: it refers to the latest synchronized version.
                    transcript.append({"saved_document_version": version})
                    send("textDocument/didSave", {"textDocument": {"uri": uri}})
                    after_save = barrier(version * 10 + 1)
                    stages.append((version, publications, after_save))

                # Queue an invalid edit and its repair together; no old publication may
                # follow a newer one, and the final current publication must be clear.
                for version, source in ((4, malformed), (5, good)):
                    send("textDocument/didChange", {"textDocument": {"uri": uri, "version": version},
                        "contentChanges": [{"text": source}]})
                rapid = barrier(50)
                send("textDocument/didClose", {"textDocument": {"uri": uri}})
            finally:
                process.stdin.close()
                trailing_stdout = process.stdout.read()
                exit_code = process.wait()
                if evidence_dir:
                    (evidence_dir / "protocol.json").write_text(json.dumps({
                        "binary": str(binary), "server_exit": exit_code, "transcript": transcript,
                        "trailing_stdout": trailing_stdout.decode("utf-8", errors="replace"),
                    }, indent=2) + "\n", encoding="utf-8")
                    (evidence_dir / "server.exit").write_text(str(exit_code) + "\n")

        assert exit_code == 0, exit_code
        assert trailing_stdout == b"", trailing_stdout
        for version, publications, after_save in stages:
            # The first publication is syntax; the second is the merged compiler bridge result.
            assert len(publications) == 2, (version, publications)
            assert after_save == [], (version, after_save)  # save itself is currently a no-op
            for publication in publications:
                assert publication["uri"] == uri and publication["version"] == version
                diagnostics = publication["diagnostics"]
                if version == 2:
                    assert any(d.get("code") == "STYIO_SERVICE_EDITOR_SYNTAX" for d in diagnostics)
                else:
                    assert diagnostics == [], (version, diagnostics)
        assert [p["version"] for p in rapid] == [4, 4, 5, 5], rapid
        assert all(p["uri"] == uri for p in rapid)
        assert all(p["diagnostics"] for p in rapid if p["version"] == 4)
        assert all(p["diagnostics"] == [] for p in rapid if p["version"] == 5)
        print("task-resource LSP syntax/merged diagnostics and saved versions agree; restored and current diagnostics clear")
        return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True, help="Path to the built styio_lspd executable")
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument("--task-resource-parity", action="store_true")
    parser.add_argument("--evidence-dir", type=Path)
    args = parser.parse_args()

    binary = Path(args.binary)
    if not binary.exists():
        fail(f"styio_lspd binary not found: {binary}")

    if args.task_resource_parity:
        return task_resource_parity(binary.resolve(), args.evidence_dir)

    with tempfile.TemporaryDirectory(prefix="styio-lspd-framing-") as workspace:
        workspace_path = Path(workspace)
        unavailable_cache_parent = workspace_path / "cache-parent"
        unavailable_cache_parent.write_text("occupied", encoding="utf-8")
        environment = os.environ.copy()
        environment["XDG_CACHE_HOME"] = str(unavailable_cache_parent)
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
            env=environment,
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
