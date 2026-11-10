"""Run against an isolated ChatServer with VCHAT_TEST_DB and VCHAT_TEST_PORT.

Set VCHAT_TEST_USER_A/PASS_A and VCHAT_TEST_USER_B/PASS_B to disposable
accounts in that database. Set VCHAT_TEST_AI=1 to include real DeepSeek SSE.
No production port, credentials or model output are printed.
"""
import base64
import json
import os
import socket
import threading
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
    directed = request(a, "create", room_name="定向邀请测试", room_type="invite_only")["room"]["room_id"]
    a_members = request(a, "members", room_id=directed)["members"]
    assert len(a_members) == 1
    search = request(a, "search_users", room_id=directed, query=os.environ["VCHAT_TEST_USER_B"])
    assert search["ok"]
    recipient_id = next(x["user_id"] for x in search["users"]
                        if x["username"] == os.environ["VCHAT_TEST_USER_B"])
    assert not request(b, "search_users", room_id=directed, query="")["ok"]
    assert not request(b, "send_user_invites", room_id=directed, user_ids=[recipient_id])["ok"]
    sent = request(a, "send_user_invites", room_id=directed, user_ids=[recipient_id])
    assert sent["ok"] and len(sent["sent"]) == 1
    invite_id = sent["sent"][0]["invite_id"]
    assert not request(a, "send_user_invites", room_id=directed, user_ids=[recipient_id])["ok"]
    assert not request(a, "respond_invite", invite_id=invite_id, decision="accept")["ok"]
    b.close()  # The pending invitation must survive a disconnected recipient.
    b = login(os.environ["VCHAT_TEST_USER_B"], os.environ["VCHAT_TEST_PASS_B"])
    inbox = request(b, "pending_invites")
    assert inbox["ok"] and any(x["invite_id"] == invite_id for x in inbox["invites"])
    assert request(b, "respond_invite", invite_id=invite_id, decision="decline")["ok"]
    assert not request(b, "respond_invite", invite_id=invite_id, decision="accept")["ok"]
    assert all(x["invite_id"] != invite_id for x in request(b, "pending_invites")["invites"])
    new_invite = request(a, "send_user_invites", room_id=directed, user_ids=[recipient_id])
    assert new_invite["ok"] and len(new_invite["sent"]) == 1
    new_id = new_invite["sent"][0]["invite_id"]
    assert new_id != invite_id
    assert request(b, "respond_invite", invite_id=new_id, decision="accept")["ok"]
    assert not request(b, "respond_invite", invite_id=new_id, decision="accept")["ok"]
    assert not request(a, "send_user_invites", room_id=directed, user_ids=[recipient_id])["ok"]
    assert any(x["room_id"] == directed for x in request(b, "list_mine")["rooms"])
    members = request(b, "members", room_id=directed)["members"]
    assert len(members) == 2 and any(x["user_id"] == recipient_id for x in members)
    if os.environ.get("VCHAT_TEST_USER_C") and os.environ.get("VCHAT_TEST_PASS_C"):
        batch_room = request(a, "create", room_name="批量邀请测试", room_type="invite_only")["room"]["room_id"]
        third = request(a, "search_users", room_id=batch_room,
                        query=os.environ["VCHAT_TEST_USER_C"])
        third_id = next(x["user_id"] for x in third["users"]
                        if x["username"] == os.environ["VCHAT_TEST_USER_C"])
        batch = request(a, "send_user_invites", room_id=batch_room,
                        user_ids=[recipient_id, third_id])
        assert batch["ok"] and len(batch["sent"]) == 2
        third_peer = login(os.environ["VCHAT_TEST_USER_C"], os.environ["VCHAT_TEST_PASS_C"])
        try:
            third_inbox = request(third_peer, "pending_invites")
            third_invite = next(x["invite_id"] for x in third_inbox["invites"]
                                if x["room_id"] == batch_room)
            assert request(third_peer, "respond_invite", invite_id=third_invite,
                           decision="accept")["ok"]
            second_invite = next(x["invite_id"] for x in request(b, "pending_invites")["invites"]
                                 if x["room_id"] == batch_room)
            assert request(b, "respond_invite", invite_id=second_invite,
                           decision="decline")["ok"]
            assert len(request(a, "members", room_id=batch_room)["members"]) == 2
        finally:
            third_peer.close()
        race_room = request(a, "create", room_name="并发接受测试", room_type="invite_only")["room"]["room_id"]
        race_invite = request(a, "send_user_invites", room_id=race_room,
                              user_ids=[recipient_id])["sent"][0]["invite_id"]
        second_session = login(os.environ["VCHAT_TEST_USER_B"], os.environ["VCHAT_TEST_PASS_B"])
        try:
            gate = threading.Barrier(2)
            outcomes = []
            def accept_from(peer):
                gate.wait()
                outcomes.append(request(peer, "respond_invite", invite_id=race_invite,
                                        decision="accept")["ok"])
            first = threading.Thread(target=accept_from, args=(b,))
            second = threading.Thread(target=accept_from, args=(second_session,))
            first.start(); second.start()
            first.join(timeout=8); second.join(timeout=8)
            assert not first.is_alive() and not second.is_alive()
            assert sorted(outcomes) == [False, True]
            assert len(request(a, "members", room_id=race_room)["members"]) == 2
        finally:
            second_session.close()
    # Room management always uses the authenticated account, not a client
    # supplied owner ID. These rooms and accounts exist only in the test DB.
    details = request(a, "details", room_id=public)
    assert details["ok"] and details["room"]["room_type"] == "public"
    assert details["room"]["member_count"] == 2
    base_permissions = request(a, "permissions", room_id=public)
    assert base_permissions["ok"] and not base_permissions["permissions"]["allow_member_edit"]
    assert request(b, "permissions", room_id=public)["ok"]
    settings = {"room_type": "public", "join_policy": "direct",
                "allow_member_invite": True, "allow_member_edit": True,
                "allow_member_kick": False,
                "allow_member_manage_permissions": False}
    assert not request(b, "update_permissions", room_id=public,
                       permissions=settings, user_id=details["room"]["owner_id"])["ok"]
    assert request(a, "update_permissions", room_id=public, permissions=settings)["ok"]
    assert request(b, "update_details", room_id=public,
                   room_name="成员授权编辑", description="授权测试")["ok"]
    assert request(b, "invite", room_id=public, expires_minutes=1)["ok"]
    assert not request(b, "remove_member", room_id=public,
                       user_id=details["room"]["owner_id"])["ok"]
    approval = dict(settings, join_policy="approval")
    assert not request(a, "update_permissions", room_id=public,
                       permissions=approval)["ok"]
    private_settings = dict(settings, room_type="invite_only",
                            allow_member_invite=False, allow_member_edit=False)
    assert request(a, "update_permissions", room_id=public,
                   permissions=private_settings)["ok"]
    assert not request(b, "update_details", room_id=public,
                       room_name="撤销后应拒绝", description="test")["ok"]
    assert not request(b, "invite", room_id=public, expires_minutes=1)["ok"]
    assert request(a, "update_permissions", room_id=public,
                   permissions=dict(private_settings, room_type="public"))["ok"]
    assert request(a, "permissions", room_id=public)["permissions"]["room_type"] == "public"
    assert not request(b, "update_details", room_id=public,
                       room_name="伪造房主", description="无权限")["ok"]
    assert not request(b, "remove_member", room_id=public,
                       user_id=details["room"]["owner_id"])["ok"]
    updated = request(a, "update_details", room_id=public,
                      room_name="公开房间已更新", description="新的简介")
    assert updated["ok"] and updated["room"]["room_name"] == "公开房间已更新"
    assert request(b, "details", room_id=public)["room"]["description"] == "新的简介"
    assert not request(a, "leave", room_id=public)["ok"]
    assert request(a, "remove_member", room_id=public, user_id=recipient_id)["ok"]
    assert not request(a, "remove_member", room_id=public, user_id=recipient_id)["ok"]
    assert not request(b, "history", room_id=public)["ok"]
    assert not request(b, "details", room_id=public)["ok"]
    assert request(b, "join_public", room_id=public)["ok"]
    assert request(b, "leave", room_id=public)["ok"]
    assert all(x["room_id"] != public for x in request(b, "list_mine")["rooms"])
    assert request(b, "join_public", room_id=public)["ok"]
    assert not request(b, "dissolve", room_id=public)["ok"]
    assert request(a, "dissolve", room_id=public)["ok"]
    assert not request(a, "dissolve", room_id=public)["ok"]
    assert not request(a, "history", room_id=public)["ok"]
    assert not request(b, "history", room_id=public)["ok"]
    assert not request(b, "join_public", room_id=public)["ok"]
    assert all(x["room_id"] != public for x in request(a, "list_mine")["rooms"])
    assert all(x["room_id"] != public for x in request(b, "list_mine")["rooms"])
    print("PASS: room permissions, public/private joins, single-use and directed invites,"
          " offline recovery, decline/accept, batch send, concurrent accept,"
          " details/edit/remove/leave/dissolve, message isolation"
          + (", real AI stream isolation" if os.environ.get("VCHAT_TEST_AI") == "1" else ""))
finally:
    a.close()
    b.close()
