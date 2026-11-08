"""Run against an isolated ChatServer with VCHAT_TEST_DB and VCHAT_TEST_PORT.

Set VCHAT_TEST_USER_A/PASS_A and VCHAT_TEST_USER_B/PASS_B to disposable
accounts in that database. Set VCHAT_TEST_AI=1 to include real DeepSeek SSE.
No production port, credentials or model output are printed.
"""
import base64
import json
import os
import socket
import time
import uuid

port = int(os.environ.get("VCHAT_TEST_PORT", "0"))
if not 0 < port < 65536 or port == 8080:
    raise SystemExit("Choose a non-production VCHAT_TEST_PORT")
for key in ("USER_A", "PASS_A", "USER_B", "PASS_B"):
    if not os.environ.get("VCHAT_TEST_" + key):
        raise SystemExit("Missing disposable VCHAT_TEST_" + key)


class Peer:
    def __init__(self):
        self.sock = socket.create_connection(("127.0.0.1", port), 4)
        self.sock.setblocking(False)
        self.buffer = b""
        self.lines = []

    def send(self, line):
        self.sock.sendall((line + "\n").encode("utf-8"))

    def poll(self):
        while True:
            try:
                chunk = self.sock.recv(262144)
            except BlockingIOError:
                break
            if not chunk:
                break
            self.buffer += chunk
            while b"\n" in self.buffer:
                line, self.buffer = self.buffer.split(b"\n", 1)
                self.lines.append(line.decode("utf-8").strip())

    def close(self):
        self.sock.close()


def wait(predicate, seconds=5):
    until = time.monotonic() + seconds
    while time.monotonic() < until:
        if predicate():
            return True
        time.sleep(0.02)
    return predicate()


def login(user, password):
    auth = Peer()
    auth.send("LOGIN|" + user + "|" + password)
    assert wait(lambda: (auth.poll() or True) and any(x.startswith("LOGIN_OK|") for x in auth.lines))
    token = next(x.split("|")[2] for x in auth.lines if x.startswith("LOGIN_META|"))
    name = next(x.split("|")[1] for x in auth.lines if x.startswith("LOGIN_OK|"))
    auth.close()
    peer = Peer()
    peer.send("JOIN|" + name + "|" + token)
    assert wait(lambda: (peer.poll() or True) and "HISTORY_END" in peer.lines)
    peer.lines.clear()
    return peer


def request(peer, action, **fields):
    payload = base64.b64encode(json.dumps(fields, ensure_ascii=False).encode()).decode()
    peer.send("ROOM|" + action + "|" + payload)
    result = []

    def received():
        peer.poll()
        for line in peer.lines:
            if not line.startswith("ROOM|result|"):
                continue
            obj = json.loads(base64.b64decode(line.split("|", 2)[2]))
            if obj.get("action") == action:
                result.append(obj)
                peer.lines.remove(line)
                return True
        return False

    assert wait(received), action
    return result[0]


def chat(peer, text):
    message_id = str(uuid.uuid4())
    payload = {"messageId": message_id, "text": text, "mentions": []}
    peer.send("CHAT2|" + base64.b64encode(json.dumps(payload, ensure_ascii=False).encode()).decode())
    return message_id


def received_chat_ids(peer):
    peer.poll()
    return [json.loads(base64.b64decode(line[6:]))["messageId"]
            for line in peer.lines if line.startswith("CHAT2|")]


def ai_events(peer):
    peer.poll()
    return [json.loads(base64.b64decode(line.split("|", 3)[3]))
            for line in peer.lines if line.startswith("AI|stream|")]


a = login(os.environ["VCHAT_TEST_USER_A"], os.environ["VCHAT_TEST_PASS_A"])
b = login(os.environ["VCHAT_TEST_USER_B"], os.environ["VCHAT_TEST_PASS_B"])
try:
    lobby_message = chat(a, "默认大厅回归测试")
    assert wait(lambda: lobby_message in received_chat_ids(b))
    a.lines.clear()
    b.lines.clear()
    private = request(a, "create", room_name="测试邀请房间", room_type="invite_only")["room"]["room_id"]
    public = request(a, "create", room_name="测试公开房间", room_type="public")["room"]["room_id"]
    assert not request(b, "switch", room_id=private)["ok"]
    assert not request(b, "history", room_id=private)["ok"]
    assert not request(b, "join_public", room_id=private)["ok"]
    assert request(b, "join_public", room_id=public)["ok"]
    assert not request(b, "join_public", room_id=public)["ok"]
    assert request(a, "switch", room_id=private)["ok"]
    assert request(b, "switch", room_id=public)["ok"]
    a.lines.clear()
    b.lines.clear()
    a_message = chat(a, "仅属于邀请房间")
    b_message = chat(b, "仅属于公开房间")
    time.sleep(0.2)
    assert b_message not in received_chat_ids(a)
    assert a_message not in received_chat_ids(b)

    if os.environ.get("VCHAT_TEST_AI") == "1":
        a.lines.clear()
        b.lines.clear()
        chat(a, "@Vivi 请回答：邀请房间测试。")
        chat(b, "@Vivi 请回答：公开房间测试。")
        assert wait(lambda: (a.poll() or b.poll() or True) and
                    any(e["event"] == "ai_complete" for e in ai_events(a)) and
                    any(e["event"] == "ai_complete" for e in ai_events(b)), 65)
        assert all(e["room_id"] == private for e in ai_events(a))
        assert all(e["room_id"] == public for e in ai_events(b))
        assert {e["message_id"] for e in ai_events(a) if e["event"] == "ai_complete"}.isdisjoint(
            {e["message_id"] for e in ai_events(b) if e["event"] == "ai_complete"})
        assert {e["request_id"] for e in ai_events(a)}.isdisjoint(
            {e["request_id"] for e in ai_events(b)})

    token = request(a, "invite", room_id=private, expires_minutes=60)["token"]
    assert request(b, "join_invite", token=token)["ok"]
    assert not request(b, "join_invite", token=token)["ok"]
    assert request(b, "members", room_id=private)["ok"]
    print("PASS: room permissions, public/private joins, single-use invite, message isolation"
          + (", real AI stream isolation" if os.environ.get("VCHAT_TEST_AI") == "1" else ""))
finally:
    a.close()
    b.close()
