#!/usr/bin/env python3

"""Length-prefixed JSON mock server for VTKQtCore socket testing.

The current C++ socket client expects each TCP message to use a 4-byte
big-endian length prefix followed by one UTF-8 JSON payload.

Examples:

    python tests/mock_socket_server.py --port 9000

Interactive commands:

    clients
    state planning {"status":"demo"}
    command switch_module {"targetModule":"planning"}
    control planning {"command":"replan"}
    resync manual-test
    send {"module":"navigation","type":"state","value":{"status":"tracking"}}
    scenario tests/sample_messages.jsonl
    quit

Scenario file format is JSON Lines. Each line should look like:

    {"delay_ms": 0, "message": {"module": "planning", "type": "state", "value": {"status": "ready"}}}

Optional line fields:
    delay_ms: delay before sending this message
    target: "all" or an integer client id
"""

from __future__ import annotations

import argparse
import json
import socket
import struct
import sys
import threading
import time
import uuid
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, Optional, Tuple


def encode_frame(payload: str) -> bytes:
    data = payload.encode("utf-8")
    return struct.pack(">I", len(data)) + data


def recv_exact(sock: socket.socket, size: int) -> Optional[bytes]:
    chunks = bytearray()
    while len(chunks) < size:
        try:
            chunk = sock.recv(size - len(chunks))
        except OSError:
            return None
        if not chunk:
            return None
        chunks.extend(chunk)
    return bytes(chunks)


def recv_frame(sock: socket.socket) -> Optional[str]:
    header = recv_exact(sock, 4)
    if header is None:
        return None
    length = struct.unpack(">I", header)[0]
    payload = recv_exact(sock, length)
    if payload is None:
        return None
    return payload.decode("utf-8", errors="replace")


def compact_json(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"))


def build_legacy_envelope(module: str, msg_type: str, value: Dict[str, Any]) -> Dict[str, Any]:
    return {
        "module": module,
        "type": msg_type,
        "value": value,
    }


@dataclass
class ClientSession:
    client_id: int
    sock: socket.socket
    address: Tuple[str, int]
    send_lock: threading.Lock = field(default_factory=threading.Lock)

    def send_json(self, payload: Dict[str, Any]) -> None:
        self.send_text(compact_json(payload))

    def send_text(self, payload: str) -> None:
        frame = encode_frame(payload)
        with self.send_lock:
            self.sock.sendall(frame)

    def close(self) -> None:
        try:
            self.sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        try:
            self.sock.close()
        except OSError:
            pass


class MockSocketServer:
    def __init__(self, host: str, port: int, auto_ack: bool = False) -> None:
        self.host = host
        self.port = port
        self.auto_ack = auto_ack
        self._server_socket: Optional[socket.socket] = None
        self._stop_event = threading.Event()
        self._client_ready = threading.Event()
        self._accept_thread: Optional[threading.Thread] = None
        self._client_threads: list[threading.Thread] = []
        self._clients: Dict[int, ClientSession] = {}
        self._clients_lock = threading.Lock()
        self._next_client_id = 1

    def start(self) -> None:
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_socket.bind((self.host, self.port))
        server_socket.listen()
        server_socket.settimeout(0.5)
        self._server_socket = server_socket
        self.port = server_socket.getsockname()[1]

        self._accept_thread = threading.Thread(target=self._accept_loop, name="mock-accept", daemon=True)
        self._accept_thread.start()

    def stop(self) -> None:
        self._stop_event.set()
        if self._server_socket is not None:
            try:
                self._server_socket.close()
            except OSError:
                pass
            self._server_socket = None

        with self._clients_lock:
            clients = list(self._clients.values())
            self._clients.clear()

        for client in clients:
            client.close()

        if self._accept_thread is not None:
            self._accept_thread.join(timeout=1.0)

        for thread in self._client_threads:
            thread.join(timeout=1.0)

    def wait_for_client(self, timeout_seconds: float) -> bool:
        return self._client_ready.wait(timeout_seconds)

    def list_clients(self) -> list[ClientSession]:
        with self._clients_lock:
            return list(self._clients.values())

    def broadcast_json(self, payload: Dict[str, Any]) -> int:
        clients = self.list_clients()
        sent = 0
        for client in clients:
            try:
                client.send_json(payload)
                sent += 1
            except OSError:
                self._remove_client(client.client_id)
        return sent

    def send_json(self, client_id: int, payload: Dict[str, Any]) -> bool:
        with self._clients_lock:
            client = self._clients.get(client_id)
        if client is None:
            return False
        try:
            client.send_json(payload)
            return True
        except OSError:
            self._remove_client(client_id)
            return False

    def run_scenario(self, path: Path) -> None:
        entries = []
        with path.open("r", encoding="utf-8") as handle:
            for line_number, raw_line in enumerate(handle, start=1):
                line = raw_line.strip()
                if not line or line.startswith("#"):
                    continue
                try:
                    entries.append(json.loads(line))
                except json.JSONDecodeError as exc:
                    raise ValueError(f"Invalid JSON on line {line_number}: {exc}") from exc

        if entries and not self.list_clients():
            print("[scenario] waiting for first client before sending messages...", flush=True)
            if not self.wait_for_client(15.0):
                raise TimeoutError("No client connected before scenario timeout")

        for index, entry in enumerate(entries, start=1):
            delay_ms = int(entry.get("delay_ms", 0))
            if delay_ms > 0:
                time.sleep(delay_ms / 1000.0)

            message = entry.get("message")
            if not isinstance(message, dict):
                raise ValueError(f"Scenario entry {index} is missing object field 'message'")

            target = entry.get("target", "all")
            if target == "all":
                count = self.broadcast_json(message)
                print(f"[scenario] sent entry {index} to {count} client(s)", flush=True)
                continue

            if not isinstance(target, int):
                raise ValueError(f"Scenario entry {index} has invalid target: {target!r}")
            if not self.send_json(target, message):
                raise ValueError(f"Scenario entry {index} failed: client {target} not connected")
            print(f"[scenario] sent entry {index} to client {target}", flush=True)

    def _accept_loop(self) -> None:
        assert self._server_socket is not None
        while not self._stop_event.is_set():
            try:
                sock, address = self._server_socket.accept()
            except socket.timeout:
                continue
            except OSError:
                break

            client_id = self._next_client_id
            self._next_client_id += 1
            session = ClientSession(client_id=client_id, sock=sock, address=address)
            with self._clients_lock:
                self._clients[client_id] = session
            self._client_ready.set()
            print(f"[connect] client={client_id} addr={address[0]}:{address[1]}", flush=True)

            thread = threading.Thread(
                target=self._client_loop,
                args=(session,),
                name=f"mock-client-{client_id}",
                daemon=True,
            )
            self._client_threads.append(thread)
            thread.start()

    def _client_loop(self, client: ClientSession) -> None:
        try:
            while not self._stop_event.is_set():
                payload = recv_frame(client.sock)
                if payload is None:
                    break
                self._handle_incoming(client, payload)
        finally:
            self._remove_client(client.client_id)
            print(f"[disconnect] client={client.client_id}", flush=True)

    def _remove_client(self, client_id: int) -> None:
        with self._clients_lock:
            client = self._clients.pop(client_id, None)
        if client is not None:
            client.close()

    def _handle_incoming(self, client: ClientSession, payload: str) -> None:
        try:
            message = json.loads(payload)
        except json.JSONDecodeError:
            print(f"[recv] client={client.client_id} non-json={payload}", flush=True)
            return

        print(f"[recv] client={client.client_id} {compact_json(message)}", flush=True)

        if self.auto_ack:
            ack = self._build_ack(message)
            if ack is not None:
                try:
                    client.send_json(ack)
                    print(f"[ack] client={client.client_id} {compact_json(ack)}", flush=True)
                except OSError:
                    self._remove_client(client.client_id)

    @staticmethod
    def _build_ack(message: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        msg_type = str(message.get("type", "")).strip().lower()
        category = str(message.get("category", "")).strip()
        if msg_type not in {"action", "action_request", "ui_action", "command", "server_command", "resync_request"} and category not in {"ActionRequest", "ServerCommand", "ResyncRequest"}:
            return None

        value = message.get("value") if isinstance(message.get("value"), dict) else {}
        request_msg_id = message.get("msgId") or value.get("msgId") or value.get("actionId")
        return {
            "category": "Ack",
            "msgId": uuid.uuid4().hex,
            "module": str(message.get("module", "global")) or "global",
            "requestMsgId": request_msg_id,
            "accepted": True,
            "detail": "accepted by mock_socket_server",
            "timestampMs": int(time.time() * 1000),
        }


def print_repl_help() -> None:
    print(
        "Commands:\n"
        "  help\n"
        "  clients\n"
        "  send <json>\n"
        "  sendto <client_id> <json>\n"
        "  state <module> <json-payload>\n"
        "  control <module> <json-payload>\n"
        "  command <command_type> <json-payload>\n"
        "  heartbeat [json-payload]\n"
        "  resync <reason>\n"
        "  scenario <path-to-jsonl>\n"
        "  quit\n",
        flush=True,
    )


def parse_json_object(raw: str, default: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    text = raw.strip()
    if not text:
        return {} if default is None else dict(default)
    value = json.loads(text)
    if not isinstance(value, dict):
        raise ValueError("JSON payload must be an object")
    return value


def run_repl(server: MockSocketServer) -> None:
    print_repl_help()
    while True:
        try:
            line = input("mock-server> ").strip()
        except EOFError:
            print()
            return
        except KeyboardInterrupt:
            print()
            return

        if not line:
            continue
        if line in {"quit", "exit"}:
            return
        if line == "help":
            print_repl_help()
            continue
        if line == "clients":
            clients = server.list_clients()
            if not clients:
                print("[clients] none", flush=True)
                continue
            for client in clients:
                print(f"[clients] id={client.client_id} addr={client.address[0]}:{client.address[1]}", flush=True)
            continue
        if line.startswith("scenario "):
            path = Path(line.split(" ", 1)[1].strip())
            server.run_scenario(path)
            continue
        if line.startswith("sendto "):
            rest = line.split(" ", 1)[1].strip()
            client_token, _, json_text = rest.partition(" ")
            client_id = int(client_token)
            payload = parse_json_object(json_text)
            ok = server.send_json(client_id, payload)
            print(f"[sendto] client={client_id} sent={ok}", flush=True)
            continue
        if line.startswith("send "):
            payload = parse_json_object(line.split(" ", 1)[1])
            count = server.broadcast_json(payload)
            print(f"[send] broadcast-to={count}", flush=True)
            continue
        if line.startswith("state "):
            rest = line.split(" ", 1)[1].strip()
            module, _, json_text = rest.partition(" ")
            payload = build_legacy_envelope(module, "state", parse_json_object(json_text))
            count = server.broadcast_json(payload)
            print(f"[state] module={module} broadcast-to={count}", flush=True)
            continue
        if line.startswith("control "):
            rest = line.split(" ", 1)[1].strip()
            module, _, json_text = rest.partition(" ")
            payload = build_legacy_envelope(module, "action_request", parse_json_object(json_text))
            count = server.broadcast_json(payload)
            print(f"[control] module={module} broadcast-to={count}", flush=True)
            continue
        if line.startswith("command "):
            rest = line.split(" ", 1)[1].strip()
            command_type, _, json_text = rest.partition(" ")
            value = parse_json_object(json_text)
            value.setdefault("commandType", command_type)
            payload = build_legacy_envelope("shell", "server_command", value)
            count = server.broadcast_json(payload)
            print(f"[command] type={command_type} broadcast-to={count}", flush=True)
            continue
        if line.startswith("heartbeat"):
            _, _, json_text = line.partition(" ")
            payload = build_legacy_envelope("global", "heartbeat", parse_json_object(json_text))
            count = server.broadcast_json(payload)
            print(f"[heartbeat] broadcast-to={count}", flush=True)
            continue
        if line.startswith("resync "):
            reason = line.split(" ", 1)[1].strip()
            payload = build_legacy_envelope("global", "resync_request", {"reason": reason})
            count = server.broadcast_json(payload)
            print(f"[resync] reason={reason} broadcast-to={count}", flush=True)
            continue

        print(f"Unknown command: {line}", flush=True)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Mock length-prefixed JSON socket server for VTKQtCore")
    parser.add_argument("--host", default="127.0.0.1", help="bind host")
    parser.add_argument("--port", type=int, default=9000, help="bind port")
    parser.add_argument("--auto-ack", action="store_true", help="send Ack category messages for inbound control/command traffic")
    parser.add_argument("--scenario", type=Path, help="optional JSONL scenario file to send after first client connects")
    parser.add_argument("--no-repl", action="store_true", help="run without the interactive console")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    server = MockSocketServer(args.host, args.port, auto_ack=args.auto_ack)
    server.start()
    print(f"[ready] mock socket server listening on {args.host}:{server.port}", flush=True)

    scenario_thread: Optional[threading.Thread] = None
    scenario_error: list[BaseException] = []

    if args.scenario is not None:
        def run_scenario_file() -> None:
            try:
                server.run_scenario(args.scenario)
            except BaseException as exc:  # noqa: BLE001
                scenario_error.append(exc)
                print(f"[scenario-error] {exc}", file=sys.stderr, flush=True)

        scenario_thread = threading.Thread(target=run_scenario_file, name="mock-scenario", daemon=True)
        scenario_thread.start()

    try:
        if args.no_repl:
            while not scenario_error:
                time.sleep(0.25)
        else:
            run_repl(server)
    except KeyboardInterrupt:
        print()
    finally:
        server.stop()
        if scenario_thread is not None:
            scenario_thread.join(timeout=1.0)

    if scenario_error:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())