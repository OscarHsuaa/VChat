"""Two authenticated clients, persisted AI ownership, and forged regeneration."""
import base64
import json
import os
import uuid
from http.server import ThreadingHTTPServer

from summary_share_protocol_test import Api, register, login, start_server, wait_for, packet, API_PORT, LOG


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


def main():
    assert os.environ["VCHAT_TEST_DB"].startswith("vchat_test_")
    api = ThreadingHTTPServer(("127.0.0.1", API_PORT), Api)
    api_thread = __import__("threading").Thread(target=api.serve_forever, daemon=True)
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
                a.send(packet("CHAT2|", {"messageId": str(uuid.uuid4()),
                    "text": "@Vivi 请总结 Qt 方案", "mentions": []}))
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
                a.close(); b.close()
                peers = []
                a, b = login(a_name, password), login(b_name, password)
                peers = [a, b]
                assert wait_for(peers, lambda: any(x.get("message_id") == message_id
                    for x in history_ai(a)) and any(x.get("message_id") == message_id
                    for x in history_ai(b)))
                owner = next(x for x in history_ai(a) if x["message_id"] == message_id)
                other = next(x for x in history_ai(b) if x["message_id"] == message_id)
                assert owner["can_regenerate"] is True and other["can_regenerate"] is False
                print("AI_REGEN_E2E clients=2 forged_rejected=true owner_allowed=true history_owner=true")
            finally:
                for peer in peers:
                    peer.close()
                server.terminate()
                try:
                    server.wait(timeout=5)
                except Exception:
                    server.kill()
    finally:
        api.shutdown()
        api.server_close()


if __name__ == "__main__":
    main()
