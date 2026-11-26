"""Verify persisted AI ownership and correlated room history against ChatServer."""
import base64
import json
import os
import subprocess
import threading
import uuid
from http.server import ThreadingHTTPServer

from summary_share_protocol_test import Api, register, login, start_server, wait_for, packet, decoded, API_PORT, LOG


class RecordingApi(Api):
    requests = []

    def do_POST(self):
        self.requests.append(json.loads(self.rfile.read(int(self.headers["Content-Length"]))))
        answer = f"Test reply {len(self.requests)}"
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.end_headers()
        for part in (answer[:5], answer[5:]):
            event = {"choices": [{"delta": {"content": part}}]}
            self.wfile.write(b"data: " + json.dumps(event).encode() + b"\n\n")
            self.wfile.flush()
        self.wfile.write(b"data: [DONE]\n\n")


def stream_events(peer):
    result = []
    for line in peer.lines:
        if not line.startswith("AI|stream|"):
            continue
        parts = line.split("|", 3)
        if len(parts) == 4:
            result.append(json.loads(base64.b64decode(parts[3])))
    return result


def history_ai(peer):
    return [json.loads(base64.b64decode(line[8:])) for line in peer.lines
            if line.startswith("HISTORY|") and
            json.loads(base64.b64decode(line[8:])).get("type") == "ai"]


def stop(server, peers):
    for peer in peers:
        peer.close()
    peers.clear()
    server.terminate()
    try:
        server.wait(timeout=5)
    except subprocess.TimeoutExpired:
        server.kill()
        server.wait(timeout=5)


def test_scoped_room_switch(peer):
    def room_response(action, predicate=lambda _: True):
        responses = [decoded(line, "ROOM|result|") for line in peer.lines if line.startswith("ROOM|result|")]
        return next((item for item in responses if item["action"] == action and predicate(item)), None)

    rooms = []
    for name in ("History B", "History C"):
        peer.send(packet("ROOM|create|", {"room_name": name, "room_type": "invite_only"}))
        assert wait_for([peer], lambda: room_response("create", lambda item: item["ok"] and
                                                      item["room"]["room_name"] == name))
        rooms.append(room_response("create", lambda item: item["ok"] and item["room"]["room_name"] == name)
                     ["room"]["room_id"])
    assert all(rooms)
    rooms.append("class-room")
    requests = [(room, str(uuid.uuid4())) for room in rooms]
    # One TCP write ensures all three requests are handled before a queued
    # worker completion can be delivered on the server network thread.
    peer.sock.sendall(("".join(packet("ROOM|switch|",
        {"room_id": room, "request_id": request, "scoped_history": True}) + "\n"
        for room, request in requests)).encode())
    assert wait_for([peer], lambda: any(line.startswith("HISTORY_DONE|") for line in peer.lines))
    done = [decoded(line, "HISTORY_DONE|") for line in peer.lines if line.startswith("HISTORY_DONE|")]
    assert len(done) == 1 and done[0]["ok"] is True
    assert (done[0]["room_id"], done[0]["request_id"]) == requests[-1]
    for room, request in requests:
        result = room_response("switch", lambda item: item.get("request_id") == request)
        assert result and result["ok"] and result["room_id"] == room and result["scoped_history"]
    tagged = [decoded(line, "HISTORY|") for line in peer.lines if line.startswith("HISTORY|")
              and "request_id" in decoded(line, "HISTORY|")]
    assert tagged and all((item["room_id"], item["request_id"]) == requests[-1] for item in tagged)


def main():
    assert os.environ["VCHAT_TEST_DB"].startswith("vchat_test_")
    api = ThreadingHTTPServer(("127.0.0.1", API_PORT), RecordingApi)
    api_thread = threading.Thread(target=api.serve_forever, daemon=True)
    api_thread.start()
    peers = []
    LOG.parent.mkdir(exist_ok=True, parents=True)
    try:
        with LOG.open("wb") as log:
            server = start_server(log)
            try:
                suffix = uuid.uuid4().hex[:8]
                a_name, b_name = "regen_a_" + suffix, "regen_b_" + suffix
                password = "regen_test_123"
                register(a_name, password)
                register(b_name, password)
                a, b = login(a_name, password), login(b_name, password)
                peers = [a, b]
                question = "@Vivi 请总结 Qt 方案"
                a.send(packet("CHAT2|", {"messageId": str(uuid.uuid4()), "text": question, "mentions": []}))
                assert wait_for(peers, lambda: any(e.get("event") == "ai_complete"
                    for e in stream_events(a)), 12), a.lines[-10:]
                complete = next(e for e in stream_events(a) if e.get("event") == "ai_complete")
                original = complete["original_request_id"]
                message_id = complete["message_id"]
                assert wait_for(peers, lambda: any(e.get("event") == "ai_complete"
                    for e in stream_events(b))), b.lines[-10:]
                b.send("AI_REGENERATE|" + original)
                assert wait_for(peers, lambda: any(x.startswith("AI|error|0|")
                    for x in b.lines)), b.lines[-10:]
                assert not any(e.get("regeneration") for e in stream_events(a))
                a.send("AI_REGENERATE|" + original)
                assert wait_for(peers, lambda: any(e.get("event") == "ai_complete" and
                    e.get("regeneration") for e in stream_events(a)), 12), a.lines[-10:]
                a.send(packet("CHAT2|", {"messageId": str(uuid.uuid4()),
                    "text": "after-question-context-marker", "mentions": []}))
                assert wait_for(peers, lambda: any(x.startswith("CHAT2|") and
                    decoded(x, "CHAT2|").get("text") == "after-question-context-marker" for x in a.lines))
                stop(server, peers)
                server = start_server(log)
                a, b = login(a_name, password), login(b_name, password)
                peers = [a, b]
                assert wait_for(peers, lambda: any(x.get("message_id") == message_id
                    for x in history_ai(a)) and any(x.get("message_id") == message_id
                    for x in history_ai(b)))
                owner = next(x for x in history_ai(a) if x["message_id"] == message_id)
                other = next(x for x in history_ai(b) if x["message_id"] == message_id)
                assert owner["can_regenerate_by_message"] is True and other["can_regenerate_by_message"] is False
                assert owner["can_regenerate"] is False  # No runtime ID survived the restart.
                original_body = owner["text"]
                b.send("AI_REGENERATE|" + message_id)
                assert wait_for(peers, lambda: any(x.startswith("AI|error|0|") for x in b.lines))
                assert len(RecordingApi.requests) == 2

                # Reuse the old numeric request ID for a different message in the new process.
                a.send(packet("CHAT2|", {"messageId": str(uuid.uuid4()),
                    "text": "@Vivi 新进程提问", "mentions": []}))
                assert wait_for(peers, lambda: any(e.get("event") == "ai_complete" and
                    e.get("original_request_id") == original and e["message_id"] != message_id
                    for e in stream_events(a)), 12)
                a.send("AI_REGENERATE|" + message_id)
                complete_after_restart = lambda: next((e for e in stream_events(a)
                    if e.get("event") == "ai_complete" and e.get("regeneration") and e["message_id"] == message_id), None)
                assert wait_for(peers, lambda: complete_after_restart() is not None, 12), a.lines[-10:]
                after = complete_after_restart()
                assert after["full_text"] != original_body and after["user_id"] == owner["sender_id"]
                payload = json.dumps(RecordingApi.requests[-1]["messages"], ensure_ascii=False)
                assert payload.count(question) == 1 and "after-question-context-marker" not in payload
                assert "新进程提问" not in payload

                # A third server process must recover the replacement in the original row.
                stop(server, peers)
                server = start_server(log)
                a = login(a_name, password)
                peers = [a]
                persisted = next(x for x in history_ai(a) if x["message_id"] == message_id)
                assert persisted["text"] == after["full_text"]
                assert all(persisted[key] == owner[key] for key in ("sequence_id", "original_request_id", "time"))
                test_scoped_room_switch(a)
                print("AI_REGEN_E2E restart_owner=true other_rejected=true stable_id=true original_context=true")
                print("ROOM_HISTORY_E2E three_requests=true only_latest_result=true")
            finally:
                stop(server, peers)
    finally:
        api.shutdown()
        api.server_close()


if __name__ == "__main__":
    main()
