"""Exercise two rooms on a real local ChatServer and a disposable MySQL schema.

Requires VCHAT_SERVER_EXE, VCHAT_TEST_MYSQL_EXE and VCHAT_TEST_DB_PASSWORD.
Copies definitions only from the local VChat schema. Uses test accounts and a
non-production listener. Does not send requests to the real DeepSeek service.
"""
import base64
import json
import os
from pathlib import Path
import socket
import subprocess
import time
import uuid


def frame(prefix, payload):
    return prefix + base64.b64encode(json.dumps(payload, ensure_ascii=False).encode()).decode()


class Peer:
    def __init__(self, port):
        self.sock = socket.create_connection(("127.0.0.1", port), 4)
        self.sock.settimeout(0.05)
        self.buffer = b""
        self.lines = []

    def send(self, line):
        self.sock.sendall((line + "\n").encode())

    def wait(self, prefix, match=lambda _: True):
        end = time.monotonic() + 5
        while time.monotonic() < end:
            for index, line in enumerate(self.lines):
                if line.startswith(prefix):
                    tail = line[len(prefix):]
                    data = json.loads(base64.b64decode(tail)) if prefix.startswith(
                        ("ROOM|result|", "ROOM|notice|", "NOTIFY|", "CHAT2|")) else tail
                    if match(data):
                        self.lines.pop(index)
                        return data
            try:
                self.buffer += self.sock.recv(65536)
            except TimeoutError:
                continue
            while b"\n" in self.buffer:
                line, self.buffer = self.buffer.split(b"\n", 1)
                self.lines.append(line.decode().strip())
        raise AssertionError(f"Timed out waiting for {prefix}")

    def room(self, action, **kwargs):
        self.send(frame(f"ROOM|{action}|", kwargs))
        return self.wait("ROOM|result|", lambda result: result.get("action") == action)

    def enter(self, room):
        assert self.room("switch", room_id=room)["ok"]
        self.wait("HISTORY_END")

    def sync(self):
        self.send(frame("NOTIFY|sync|", {}))
        return {r["room_id"]: r for r in self.wait("NOTIFY|snapshot|")["rooms"]}

    def chat(self, text, mentions=None):
        message_id = str(uuid.uuid4())
        self.send(frame("CHAT2|", {"messageId": message_id, "text": text, "mentions": mentions or []}))
        return self.wait("CHAT2|", lambda data: data["messageId"] == message_id)

    def message_events(self):
        return [json.loads(base64.b64decode(line.split("|", 2)[2]))
                for line in self.lines if line.startswith("NOTIFY|message|")]


def unused_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def main():
    exe = os.environ["VCHAT_SERVER_EXE"]
    mysql = os.environ["VCHAT_TEST_MYSQL_EXE"]
    mysql_env = dict(os.environ, MYSQL_PWD=os.environ["VCHAT_TEST_DB_PASSWORD"])
    db = "vchat_notification_verify_" + uuid.uuid4().hex[:12]
    port = unused_port()
    assert port != 8080
    peers = []
    server = None
    migration = (Path(__file__).resolve().parents[1] / "ChatServer/migrations/009_message_notifications.sql").read_bytes()

    def sql(query, schema=None):
        command = [mysql, "--no-defaults", "--protocol=TCP", "--host=127.0.0.1", "--port=3306",
                   "--user=root", "--batch", "--skip-column-names", "--default-character-set=utf8mb4"]
        if schema:
            command.append(schema)
        return subprocess.run(command, input=query if isinstance(query, bytes) else query.encode(),
                              stdout=subprocess.PIPE, env=mysql_env, check=True).stdout.decode().strip()

    def stop():
        nonlocal server
        for peer in peers:
            peer.sock.close()
        peers.clear()
        if server is not None:
            server.terminate()
            server.wait(timeout=5)
            server = None

    def start():
        nonlocal server
        environment = dict(os.environ, VCHAT_TEST_DB=db, VCHAT_TEST_PORT=str(port),
                           DEEPSEEK_API_KEY="test-only", VCHAT_TEST_AI_ENDPOINT=f"http://127.0.0.1:{unused_port()}")
        environment.pop("VCHAT_TEST_DB_PASSWORD", None)
        server = subprocess.Popen([exe], env=environment, stdout=subprocess.DEVNULL,
                                  stderr=subprocess.DEVNULL, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        for _ in range(150):
            assert server.poll() is None, "ChatServer stopped during startup"
            try:
                with socket.create_connection(("127.0.0.1", port), 0.1):
                    return
            except OSError:
                time.sleep(0.04)
        raise AssertionError("ChatServer did not open its test port")

    def login(name, register=False):
        auth = Peer(port)
        try:
            password = "notifytest134"
            if register:
                auth.send(frame("REGISTER|", {"username": name, "password": password}))
                auth.wait("REGISTER_OK")
            auth.send(f"LOGIN|{name}|{password}")
            account, token = auth.wait("LOGIN_META|").split("|")
            nickname = auth.wait("LOGIN_OK|")
        finally:
            auth.sock.close()
        peer = Peer(port)
        peers.append(peer)
        peer.send(f"JOIN|{nickname}|{token}")
        peer.wait("HISTORY_END")
        return peer, account, nickname

    sql(f"CREATE DATABASE `{db}` CHARACTER SET utf8mb4")
    try:
        for table in ("users", "messages", "rooms", "room_members", "room_invites", "room_direct_invites", "chat_history"):
            sql(f"CREATE TABLE `{db}`.`{table}` LIKE `vchat_demo`.`{table}`")
        sql("INSERT INTO rooms(room_id,room_name,description,room_type) VALUES('class-room','Lobby','','public')", db)
        start()
        a, _, _ = login("notify_a", True)
        b, _, _ = login("notify_b", True)
        login("notify_c", True)
        assert not a.room("list_mine").get("notifications_supported", False)
        assert not a.room("list_mine").get("mention_all_supported", False)
        room = a.room("create", room_name="C++", room_type="public")["room"]["room_id"]
        other = a.room("create", room_name="Project", room_type="public")["room"]["room_id"]
        assert b.room("join_public", room_id=room)["ok"]
        assert b.room("join_public", room_id=other)["ok"]
        a.enter(room)
        a.chat("Before migration")
        stop()
        sql(migration, db)
        start()

        a, a_id, _ = login("notify_a")
        b, b_id, b_name = login("notify_b")
        c, _, _ = login("notify_c")
        assert b.room("list_mine")["notifications_supported"]
        assert b.sync()[room]["unread"] == 0
        a.sync()
        assert room not in c.sync()
        a.enter(room)
        first = a.chat("first")
        second = a.chat("@" + b_name + " check", [{"type": "user", "id": b_id, "name": b_name,
                                                       "start": 0, "length": len(b_name) + 1}])
        third = a.chat("plain @" + b_name)
        states = b.sync()
        assert states[room]["unread"] == 3 and states[room]["mentions"] == 1
        events = b.message_events()
        assert [x["message_id"] for x in events] == [item["messageId"] for item in (first, second, third)]
        assert [x["mentioned"] for x in events] == [False, True, False]
        assert all(x["room_id"] == room and x["sender_id"] == a_id and x["type"] == "user" and
                   x["time"].endswith("Z") and "text" not in x for x in events)
        assert not any(line.startswith("CHAT2|") for line in b.lines)
        assert room not in c.sync() and not c.message_events()
        assert a.sync()[room]["unread"] == 0 and not a.message_events()
        assert states[other]["unread"] == 0
        a.enter(other)
        a.chat("other room")
        assert b.sync()[other]["unread"] == 1

        stop()
        sql(migration, db)  # Reapplying the script preserves established positions.
        start()
        b, _, _ = login("notify_b")
        states = b.sync()
        assert states[room]["unread"] == 3 and states[other]["unread"] == 1 and states[room]["mentions"] == 1
        b.send(frame("NOTIFY|read|", {"room_id": room, "sequence_id": third["sequence_id"]}))
        assert b.sync()[room]["unread"] == 3  # Switching elsewhere cannot acknowledge this room.
        b.enter(room)
        assert b.sync()[room]["unread"] == 3  # Entering alone never marks read.
        b.send(frame("NOTIFY|read|", {"room_id": room, "sequence_id": first["sequence_id"]}))
        state = b.wait("NOTIFY|room|")
        assert state["unread"] == 2 and state["mentions"] == 1
        b.send(frame("NOTIFY|read|", {"room_id": room, "sequence_id": second["sequence_id"]}))
        state = b.wait("NOTIFY|room|")
        assert state["unread"] == 1 and state["mentions"] == 0
        b.send(frame("NOTIFY|read|", {"room_id": room, "sequence_id": third["sequence_id"]}))
        assert b.wait("NOTIFY|room|")["unread"] == 0
        b.chat("own message")
        assert b.sync()[room]["unread"] == 0
        a, _, _ = login("notify_a")
        c, c_id, c_name = login("notify_c")
        assert c.room("join_public", room_id=room)["ok"]
        a.enter(room)
        a.chat("@" + c_name, [{"type": "user", "id": c_id, "name": c_name,
                                "start": 0, "length": len(c_name) + 1}])
        a.chat("@Vivi test", [{"type": "ai", "id": "vivi", "name": "Vivi", "start": 0, "length": 5}])
        state = b.sync()[room]
        assert state["unread"] == 2 and state["mentions"] == 0
        assert all(not event["mentioned"] for event in b.message_events())
        assert c.sync()[room]["mentions"] == 1 and not c.message_events()  # No unsolicited frames before sync.
        a.sync()
        for left, right in ((other, room), (room, other), (other, room)):
            a.send(frame("ROOM|switch|", {"room_id": left}))
            b.send(frame("ROOM|switch|", {"room_id": right}))
            for peer, expected in ((a, left), (b, right)):
                result = peer.wait("ROOM|result|", lambda data: data.get("action") == "switch")
                assert result["ok"] and result["room_id"] == expected
                peer.wait("HISTORY_END")
            sent = a.chat("paired room switch")
            notice = b.wait("NOTIFY|message|", lambda data: data["message_id"] == sent["messageId"])
            assert notice["room_id"] == left and not notice["mentioned"]
            assert not any(json.loads(base64.b64decode(line[6:]))["messageId"] == sent["messageId"]
                           for line in b.lines if line.startswith("CHAT2|"))
        b.lines = [line for line in b.lines if not line.startswith("NOTIFY|message|")]
        assert b.room("leave", room_id=room)["ok"]
        a.enter(room)
        a.chat("after B left")
        assert room not in b.sync() and not b.message_events()

        # A and C are the remaining two human members. Vivi does not count.
        token = "@全体成员"
        group = {"type": "all", "id": "all", "name": token[1:], "start": 0, "length": len(token)}
        a.send(frame("CHAT2|", {"text": token, "mentions": [group]}))
        a.wait("[System] ", lambda text: "暂不可提及全体成员" in text)
        assert a.room("list_mine").get("mention_all_supported") is True
        a.lines = [line for line in a.lines if not line.startswith("ROOM|notice|")]
        assert b.room("join_public", room_id=room)["ok"]
        a.wait("ROOM|notice|", lambda data: data.get("event") == "members_changed" and data.get("room_id") == room)
        b.enter(other)
        b.sync()
        outsider, _, _ = login("notify_d", True)
        assert outsider.room("join_public", room_id=other)["ok"]
        outsider.sync()
        offline_baseline = c.sync()[room]
        sender_baseline = a.sync()[room]
        c.sock.close()

        a.send(frame("CHAT2|", {"text": token + "示例", "mentions": [group]}))
        a.wait("[System] ", lambda text: "请重新选择" in text)
        plain = a.chat(token + " 纯文本以及 ＠全体成员")
        assert not b.wait("NOTIFY|message|", lambda data: data["message_id"] == plain["messageId"])["mentioned"]
        text = f"{token} {token} @{b_name} 检查"
        sent = a.chat(text, [group, dict(group, start=len(token) + 1),
                            {"type": "user", "id": b_id, "name": b_name,
                             "start": 2 * (len(token) + 1), "length": len(b_name) + 1}])
        notice = b.wait("NOTIFY|message|", lambda data: data["message_id"] == sent["messageId"])
        assert notice["mentioned"] and notice["room_id"] == room and "text" not in notice
        assert sent["text"] == text and sent["mentions"][0] == group
        assert b.sync()[room]["mentions"] == 1 and not b.message_events()
        assert room not in outsider.sync() and not outsider.message_events()
        assert a.sync()[room] == sender_baseline and not a.message_events()
        assert sql(f"SELECT user_id FROM chat_message_mentions WHERE sequence_id={int(sent['sequence_id'])} ORDER BY user_id", db).splitlines() == sorted([b_id, c_id], key=int)
        assert sql(f"SELECT COUNT(*) FROM chat_history WHERE reply_to_message_id='{sent['messageId']}' AND message_type='ai'", db) == "0"

        stop()
        start()
        c, _, _ = login("notify_c")
        restored = c.sync()[room]
        assert restored["mentions"] == offline_baseline["mentions"] + 1
        assert restored["unread"] == offline_baseline["unread"] + 2
        c.enter(room)
        c.send(frame("NOTIFY|read|", {"room_id": room, "sequence_id": sent["sequence_id"]}))
        assert c.wait("NOTIFY|room|")["mentions"] == 0
        print("All-member mention: threshold, literal text, deduplication, room isolation, no Vivi reply and offline restart passed")
        print("Message notifications: migration, cross-room delivery, mentions, restart, cursor and membership passed")
    finally:
        stop()
        sql(f"DROP DATABASE `{db}`")


if __name__ == "__main__":
    main()
