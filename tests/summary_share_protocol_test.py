"""Isolated two-client integration test. Requires VCHAT_TEST_DB test schema.

Starts ChatServer on a non-production port and a local SSE stub. No live API
or production user records are used. The database is intentionally retained
until the caller verifies its rows, then may be removed separately.
"""
import base64
import json
import os
import socket
import subprocess
import threading
import time
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SERVER = ROOT / "ChatServer/build/ViviSummaryCheck/ChatServer.exe"
LOG = ROOT / "tests/build/summary_share_server_test.log"
DB = os.environ["VCHAT_TEST_DB"]
PORT = int(os.environ.get("VCHAT_TEST_PORT", "18192"))
API_PORT = PORT + 1
SUMMARY = ("### 讨论概览\n两位成员讨论了 Qt 客户端与 MySQL 数据。\n\n"
           "### 已达成共识\n- 客户端继续使用 Qt Widgets。\n- 历史消息保存在 MySQL。\n\n"
           "### 待解决问题\n- 继续验证重启后的恢复。\n\n"
           "### 下一步行动\n- 测试两名成员的实时同步。")


class Api(BaseHTTPRequestHandler):
    def log_message(self, *_args):
        pass

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        self.rfile.read(length)
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.end_headers()
        middle = len(SUMMARY) // 2
        for part in (SUMMARY[:middle], SUMMARY[middle:]):
            event = {"choices": [{"delta": {"content": part}}]}
            self.wfile.write(b"data: " + json.dumps(event, ensure_ascii=False).encode() + b"\n\n")
            self.wfile.flush()
        self.wfile.write(b"data: [DONE]\n\n")
        self.wfile.flush()


def packet(prefix, obj):
    return prefix + base64.b64encode(json.dumps(obj, ensure_ascii=False).encode()).decode()


def decoded(line, prefix):
    return json.loads(base64.b64decode(line[len(prefix):]))


class Peer:
    def __init__(self):
        self.sock = socket.create_connection(("127.0.0.1", PORT), 4)
        self.sock.settimeout(0.05)
        self.buffer = b""
        self.lines = []

    def send(self, line):
        self.sock.sendall((line + "\n").encode())

    def poll(self):
        try:
            chunk = self.sock.recv(262144)
        except socket.timeout:
            return
        if not chunk:
            return
        self.buffer += chunk
        while b"\n" in self.buffer:
            line, self.buffer = self.buffer.split(b"\n", 1)
            self.lines.append(line.decode().strip())

    def close(self):
        self.sock.close()


def wait_for(peers, predicate, seconds=5):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        for peer in peers:
            peer.poll()
        if predicate():
            return True
    return predicate()


def event(peer, prefix, kind=None):
    for line in peer.lines:
        if not line.startswith(prefix):
            continue
        obj = decoded(line, prefix)
        if kind is None or obj.get("event") == kind:
            return obj
    return None


def register(name, password):
    peer = Peer()
    peer.send(packet("REGISTER|", {"username": name, "password": password}))
    assert wait_for([peer], lambda: any(x.startswith("REGISTER_OK") for x in peer.lines)), peer.lines
    peer.close()


def login(name, password):
    auth = Peer()
    auth.send(f"LOGIN|{name}|{password}")
    assert wait_for([auth], lambda: any(x.startswith("LOGIN_OK|") for x in auth.lines)), auth.lines
    token = next(x.split("|")[2] for x in auth.lines if x.startswith("LOGIN_META|"))
    nick = next(x.split("|")[1] for x in auth.lines if x.startswith("LOGIN_OK|"))
    auth.close()
    peer = Peer()
    peer.send(f"JOIN|{nick}|{token}")
    assert wait_for([peer], lambda: "HISTORY_END" in peer.lines), peer.lines
    return peer


def start_server(log):
    env = os.environ.copy()
    env["VCHAT_TEST_DB"] = DB
    env["VCHAT_TEST_PORT"] = str(PORT)
    env["VCHAT_TEST_AI_ENDPOINT"] = f"http://127.0.0.1:{API_PORT}/chat/completions"
    env["DEEPSEEK_API_KEY"] = "local-test-only"
    process = subprocess.Popen([str(SERVER)], cwd=str(SERVER.parent), env=env,
                               stdout=log, stderr=subprocess.STDOUT,
                               creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise AssertionError(f"ChatServer exited {process.returncode}")
        try:
            probe = socket.create_connection(("127.0.0.1", PORT), 0.2)
            probe.close()
            return process
        except OSError:
            time.sleep(0.05)
    raise AssertionError("ChatServer failed to listen")


def main():
    assert DB.startswith("vchat_test_") and PORT != 8080
    api = ThreadingHTTPServer(("127.0.0.1", API_PORT), Api)
    thread = threading.Thread(target=api.serve_forever, daemon=True)
    thread.start()
    LOG.parent.mkdir(exist_ok=True, parents=True)
    with LOG.open("wb") as log:
        server = start_server(log)
        peers = []
        try:
            suffix = uuid.uuid4().hex[:8]
            user_a, user_b = "sum_a_" + suffix, "sum_b_" + suffix
            password = "summary_test_123"
            register(user_a, password)
            register(user_b, password)
            a, b = login(user_a, password), login(user_b, password)
            peers = [a, b]
            a.send(packet("CHAT2|", {"messageId": str(uuid.uuid4()),
                "text": "客户端使用 Qt Widgets。", "mentions": []}))
            b.send(packet("CHAT2|", {"messageId": str(uuid.uuid4()),
                "text": "聊天历史保存到 MySQL。", "mentions": []}))
            assert wait_for(peers, lambda: any(x.startswith("CHAT2|") for x in a.lines)
                            and any(x.startswith("CHAT2|") for x in b.lines))
            summary_id = str(uuid.uuid4())
            a.send(packet("AI_SUMMARY|", {"action": "start", "request_id": summary_id,
                "room_id": "class-room"}))
            def summary_event(kind):
                return next((decoded(x, "AI_SUMMARY|") for x in a.lines
                    if x.startswith("AI_SUMMARY|") and
                    decoded(x, "AI_SUMMARY|").get("event") == kind and
                    decoded(x, "AI_SUMMARY|").get("request_id") == summary_id), None)
            assert wait_for(peers, lambda: summary_event("complete") is not None, 12), a.lines
            assert summary_event("complete")["text"] == SUMMARY
            assert summary_event("meta")["message_count"] >= 2
            share_id = str(uuid.uuid4())
            share = {"request_id": share_id, "summary_request_id": summary_id,
                     "room_id": "class-room"}
            a.send(packet("SUMMARY_SHARE|", share))
            assert wait_for(peers, lambda: event(a, "SUMMARY_SHARE|", "message")
                            and event(b, "SUMMARY_SHARE|", "message")), (a.lines, b.lines)
            first = event(a, "SUMMARY_SHARE|", "message")
            second = event(b, "SUMMARY_SHARE|", "message")
            assert first["message_id"] == second["message_id"]
            assert first["markdown"] == SUMMARY
            assert first["shared_by_name"] == user_a
            assert first["room_id"] == "class-room"
            a.lines.clear()
            b.lines.clear()
            a.send(packet("SUMMARY_SHARE|", share))
            assert wait_for(peers, lambda: event(a, "SUMMARY_SHARE|", "result")
                            and event(a, "SUMMARY_SHARE|", "message"))
            assert event(a, "SUMMARY_SHARE|", "result")["message_id"] == first["message_id"]
            assert event(a, "SUMMARY_SHARE|", "message")["message_id"] == first["message_id"]
            invalid = {"request_id": str(uuid.uuid4()), "summary_request_id": summary_id,
                       "room_id": "forged-room"}
            b.send(packet("SUMMARY_SHARE|", invalid))
            assert wait_for(peers, lambda: event(b, "SUMMARY_SHARE|", "result"))
            assert event(b, "SUMMARY_SHARE|", "result")["ok"] is False
            for peer in peers:
                peer.close()
            peers.clear()
            server.terminate()
            server.wait(timeout=6)
            server = start_server(log)
            rejoined = login(user_b, password)
            peers.append(rejoined)
            history = [decoded(x, "HISTORY|") for x in rejoined.lines
                       if x.startswith("HISTORY|")]
            summaries = [x for x in history if x.get("type") == "discussion_summary"]
            matching = [x for x in summaries if x["message_id"] == first["message_id"]]
            assert len(matching) == 1, history
            assert json.loads(matching[0]["text"])["markdown"] == SUMMARY
            a2 = login(user_a, password)
            peers.append(a2)
            a2.send(packet("SUMMARY_SHARE|", share))
            assert wait_for(peers, lambda: event(a2, "SUMMARY_SHARE|", "result"))
            assert event(a2, "SUMMARY_SHARE|", "result")["message_id"] == first["message_id"]
            print("SUMMARY_SHARE_E2E clients=2 persisted=true restart=true idempotent=true")
        finally:
            for peer in peers:
                peer.close()
            server.terminate()
            server.wait(timeout=6)
            api.shutdown()


if __name__ == "__main__":
    main()
