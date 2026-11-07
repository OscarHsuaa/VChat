#include "airoom.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <cstdio>

static QByteArray sse(const QString &text)
{
    const QJsonObject delta{{"choices", QJsonArray{QJsonObject{{"delta", QJsonObject{{"content", text}}}}}}};
    return "data: " + QJsonDocument(delta).toJson(QJsonDocument::Compact) + "\r\n\r\n";
}

static bool until(const std::function<bool()> &condition, int timeoutMs = 5000)
{
    QEventLoop loop;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() { if (condition()) loop.quit(); });
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    timer.start();
    loop.exec();
    return condition();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--live-room")) {
        AiService live;
        if (!live.isConfigured()) return 10;
        QList<QJsonObject> liveEvents;
        AiRoom *roomPtr = nullptr;
        AiRoom room(&live, [&](const QString &state, quint64 id, const QString &text) {
            if (!state.startsWith(QStringLiteral("stream_"))) return;
            const QString kind = state == QStringLiteral("stream_start") ? QStringLiteral("ai_start")
                : state == QStringLiteral("stream_chunk") ? QStringLiteral("ai_chunk")
                : state == QStringLiteral("stream_complete") ? QStringLiteral("ai_complete")
                : QStringLiteral("ai_error");
            liveEvents.append(roomPtr->streamEvent(id, kind, text));
        });
        roomPtr = &room;
        room.addUserMessage(11, QStringLiteral("测试者"), QStringLiteral("@Vivi 请用一句话介绍 VChat。"),
                            true, QStringLiteral("live-question"));
        auto completed = [&](const QString &id) {
            for (const QJsonObject &item : liveEvents)
                if (item.value("request_id") == id &&
                    (item.value("event") == QStringLiteral("ai_complete") ||
                     item.value("event") == QStringLiteral("ai_error"))) return true;
            return false;
        };
        if (!until([&]() { return completed(QStringLiteral("1")); }, 65000)) return 11;
        const QJsonObject initial = room.snapshots().first();
        if (initial.value("event") != QStringLiteral("ai_complete") ||
            initial.value("full_text").toString().isEmpty() || !room.regenerate(1, 11)) return 12;
        if (!until([&]() { return completed(QStringLiteral("2")); }, 65000)) return 13;
        const QJsonObject revised = room.snapshots().first();
        const bool okay = revised.value("event") == QStringLiteral("ai_complete") &&
            revised.value("message_id") == initial.value("message_id") &&
            revised.value("generation_id") != initial.value("generation_id") &&
            !revised.value("full_text").toString().isEmpty() && room.snapshots().size() == 1;
        int chunks = 0;
        for (const QJsonObject &item : liveEvents)
            if (item.value("event") == QStringLiteral("ai_chunk")) ++chunks;
        std::fprintf(stdout, "LIVE_REGENERATE completed=%s requests=2 chunks=%d stable_message=%s\n",
                     okay ? "true" : "false", chunks,
                     revised.value("message_id") == initial.value("message_id") ? "true" : "false");
        return okay && chunks > 0 ? 0 : 14;
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--live")) {
        AiService live;
        if (!live.isConfigured()) {
            std::fprintf(stdout, "LIVE_STREAM configured=false\n");
            return 10;
        }
        int chunks = 0;
        QElapsedTimer clock;
        clock.start();
        qint64 firstChunkMs = -1, lastChunkMs = -1, completedMs = -1;
        QString answer, error;
        bool finished = false;
        const QJsonArray prompt{QJsonObject{{"role", "user"},
            {"content", QStringLiteral("请用三段、总计约一百二十字，简要介绍 VChat 聊天室中 AI 助手可以怎样帮助成员交流。")}}};
        live.stream(prompt, [&](const QString &) {
            ++chunks;
            if (firstChunkMs < 0) firstChunkMs = clock.elapsed();
            lastChunkMs = clock.elapsed();
        },
            [&](const QString &text, const QString &failure) {
                answer = text; error = failure; finished = true; completedMs = clock.elapsed();
            });
        until([&]() { return finished; }, 65000);
        std::fprintf(stdout, "LIVE_STREAM configured=true chunks=%d first_ms=%lld last_ms=%lld complete_ms=%lld completed=%s error=%s\n",
                     chunks, static_cast<long long>(firstChunkMs), static_cast<long long>(lastChunkMs),
                     static_cast<long long>(completedMs),
                     finished && error.isEmpty() && !answer.isEmpty() ? "true" : "false",
                     error.isEmpty() ? "none" : error.toUtf8().constData());
        return finished && error.isEmpty() && chunks > 0 && !answer.isEmpty() ? 0 : 11;
    }
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    qputenv("DEEPSEEK_API_KEY", "local-test-only");
    QTcpServer http;
    if (!http.listen(QHostAddress::LocalHost, 0)) return 1;
    int requests = 0;
    bool regenerationPhase = false;
    int regenerationCalls = 0;
    QList<QJsonObject> requestBodies;
    QObject::connect(&http, &QTcpServer::newConnection, &app, [&]() {
        QTcpSocket *peer = http.nextPendingConnection();
        auto *buffer = new QByteArray;
        QObject::connect(peer, &QObject::destroyed, &app, [buffer]() { delete buffer; });
        QObject::connect(peer, &QTcpSocket::readyRead, peer, [&, peer, buffer]() {
            buffer->append(peer->readAll());
            const int headerEnd = buffer->indexOf("\r\n\r\n");
            if (headerEnd < 0) return;
            int length = -1;
            for (const QByteArray &line : buffer->left(headerEnd).split('\n'))
                if (line.trimmed().toLower().startsWith("content-length:"))
                    length = line.mid(line.indexOf(':') + 1).trimmed().toInt();
            if (length < 0 || buffer->size() < headerEnd + 4 + length) return;
            requestBodies.append(QJsonDocument::fromJson(buffer->mid(headerEnd + 4, length)).object());
            buffer->clear();
            ++requests;
            if (regenerationPhase) {
                ++regenerationCalls;
                if (regenerationCalls == 3) {
                    peer->write("HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                } else if (regenerationCalls == 4) {
                    return; // The client cancels this in-flight regeneration.
                } else {
                    const QString answer = regenerationCalls == 1 ? QStringLiteral("新版本一")
                        : QStringLiteral("```cpp\nint answer = 2;\n```");
                    const QByteArray payload = sse(answer) + "data: [DONE]\r\n\r\n";
                    peer->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: " +
                        QByteArray::number(payload.size()) + "\r\nConnection: close\r\n\r\n" + payload);
                }
                peer->disconnectFromHost();
                return;
            }
            if (requests == 3) {
                const QByteArray body = R"({"error":{"message":"SECRET_SENTINEL"}})";
                peer->write("HTTP/1.1 429 Too Many Requests\r\nContent-Length: " +
                    QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                peer->disconnectFromHost();
                return;
            }
            if (requests == 4) {
                const QByteArray first = sse(QStringLiteral("部分"));
                peer->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n"
                            "Transfer-Encoding: chunked\r\nConnection: keep-alive\r\n\r\n");
                const QByteArray chunk = QByteArray::number(first.size(), 16) + "\r\n" + first + "\r\n";
                peer->write(chunk);
                return; // The client aborts after its first delta.
            }
            if (requests == 5) return; // Inactivity timeout.
            if (requests == 6) {
                const QByteArray bad = "data: {broken-json}\n\n";
                peer->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: " +
                            QByteArray::number(bad.size()) + "\r\nConnection: close\r\n\r\n" + bad);
                peer->disconnectFromHost();
                return;
            }
            if (requests == 7) {
                const QByteArray partial = sse(QStringLiteral("未完成"));
                peer->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: " +
                            QByteArray::number(partial.size()) + "\r\nConnection: close\r\n\r\n" + partial);
                peer->disconnectFromHost(); // Missing [DONE].
                return;
            }
            const QByteArray first = sse(requests == 1 ? QStringLiteral("你") : QStringLiteral("```cpp\n"));
            const QByteArray second = sse(requests == 1 ? QStringLiteral("好") : QStringLiteral("int x = 1;\n```"));
            const QByteArray rest = second + "data: [DONE]\r\n\r\n";
            const QByteArray all = first + rest;
            peer->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: " +
                        QByteArray::number(all.size()) + "\r\nConnection: close\r\n\r\n");
            const int split = first.indexOf(QStringLiteral("你").toUtf8()) + 1;
            const int cut = requests == 1 && split > 0 ? split : first.size() / 2;
            peer->write(first.left(cut));
            QTimer::singleShot(15, peer, [peer, first, rest, cut]() {
                peer->write(first.mid(cut) + rest); // Partial UTF-8 and merged SSE events.
                peer->disconnectFromHost();
            });
        });
    });
    AiService service(nullptr, QUrl(QStringLiteral("http://127.0.0.1:%1/chat/completions").arg(http.serverPort())));
    QList<QJsonObject> events;
    AiRoom *roomPtr = nullptr;
    AiRoom room(&service, [&](const QString &state, quint64 id, const QString &text) {
        if (!state.startsWith(QStringLiteral("stream_"))) return;
        const QString type = state == QStringLiteral("stream_start") ? QStringLiteral("ai_start")
            : state == QStringLiteral("stream_chunk") ? QStringLiteral("ai_chunk")
            : state == QStringLiteral("stream_complete") ? QStringLiteral("ai_complete")
            : QStringLiteral("ai_error");
        events.append(roomPtr->streamEvent(id, type, text));
    });
    roomPtr = &room;
    room.addUserMessage(11, QStringLiteral("甲"), QStringLiteral("@Vivi 你好"), true, QStringLiteral("question-1"));
    room.addUserMessage(22, QStringLiteral("乙"), QStringLiteral("@Vivi 代码"), true, QStringLiteral("question-2"));
    if (!until([&]() {
        int complete = 0;
        for (const QJsonObject &event : events)
            if (event.value("event").toString() == QStringLiteral("ai_complete")) ++complete;
        return complete == 2;
    })) return 2;
    if (requests != 2 || requestBodies.size() != 2 ||
        !requestBodies.at(0).value("stream").toBool() ||
        requestBodies.at(0).value("max_tokens").toInt() != 1024) return 3;
    QHash<QString, QString> answers;
    QHash<QString, QString> messageIds;
    int chunks = 0;
    for (const QJsonObject &event : events) {
        const QString type = event.value("event").toString();
        const QString id = event.value("request_id").toString();
        if (type == QStringLiteral("ai_chunk")) ++chunks;
        if (type == QStringLiteral("ai_complete")) {
            answers[id] = event.value("full_text").toString();
            messageIds[id] = event.value("message_id").toString();
            if (event.value("user_id").toString() != (id == "1" ? "11" : "22") ||
                event.value("reply_to_message_id").toString() != (id == "1" ? "question-1" : "question-2") ||
                event.value("generation_id").toString().isEmpty()) return 4;
        }
    }
    if (chunks != 4 || answers.value("1") != QStringLiteral("你好") ||
        answers.value("2") != QStringLiteral("```cpp\nint x = 1;\n```") ||
        messageIds.value("1").isEmpty() || messageIds.value("1") == messageIds.value("2") ||
        room.snapshots().size() != 2) return 5;
    QString httpError;
    service.stream({}, [](const QString &) {}, [&](const QString &, const QString &error) { httpError = error; });
    if (!until([&]() { return !httpError.isEmpty(); }) || requests != 3 ||
        httpError.contains(QStringLiteral("SECRET_SENTINEL"))) return 6;
    QString canceled;
    QNetworkReply *cancelReply = nullptr;
    cancelReply = service.stream({}, [&](const QString &) { if (cancelReply) cancelReply->abort(); },
        [&](const QString &, const QString &error) { canceled = error; });
    if (!until([&]() { return !canceled.isEmpty(); }) || requests != 4 ||
        !canceled.contains(QStringLiteral("取消"))) return 7;
    qunsetenv("DEEPSEEK_API_KEY");
    QString missing;
    service.stream({}, [](const QString &) {}, [&](const QString &, const QString &error) { missing = error; });
    if (!until([&]() { return !missing.isEmpty(); }) || requests != 4) return 8;
    QString canceledRoomError;
    AiRoom noKeyRoom(&service, [&](const QString &state, quint64, const QString &text) {
        if (state == QStringLiteral("stream_error")) canceledRoomError = text;
    });
    noKeyRoom.addUserMessage(99, QStringLiteral("测试者"), QStringLiteral("@Vivi 你好"), true,
                             QStringLiteral("question-cancel"));
    if (noKeyRoom.cancel(1, 100) || !noKeyRoom.cancel(1, 99) ||
        !until([&]() { return !canceledRoomError.isEmpty(); }) ||
        !canceledRoomError.contains(QStringLiteral("取消"))) return 9;
    qputenv("DEEPSEEK_API_KEY", "local-test-only");
    const QUrl endpoint(QStringLiteral("http://127.0.0.1:%1/chat/completions").arg(http.serverPort()));
    AiService quick(nullptr, endpoint, 150);
    QString timeout;
    quick.stream({}, [](const QString &) {}, [&](const QString &, const QString &error) { timeout = error; });
    if (!until([&]() { return !timeout.isEmpty(); }) || requests != 5 ||
        !timeout.contains(QStringLiteral("超时"))) return 10;
    QString malformed;
    service.stream({}, [](const QString &) {}, [&](const QString &, const QString &error) { malformed = error; });
    if (!until([&]() { return !malformed.isEmpty(); }) || requests != 6 ||
        !malformed.contains(QStringLiteral("格式"))) return 11;
    QString interrupted;
    service.stream({}, [](const QString &) {}, [&](const QString &, const QString &error) { interrupted = error; });
    if (!until([&]() { return !interrupted.isEmpty(); }) || requests != 7 ||
        !interrupted.contains(QStringLiteral("中断"))) return 12;
    regenerationPhase = true;
    room.addUserMessage(33, QStringLiteral("丙"), QStringLiteral("后来的普通消息"), false);
    if (room.regenerate(1, 22) || !room.regenerate(1, 11) || room.regenerate(1, 11)) return 13;
    if (!until([&]() {
        for (const QJsonObject &item : events)
            if (item.value("event") == QStringLiteral("ai_complete") &&
                item.value("request_id") == QStringLiteral("3")) return true;
        return false;
    })) return 14;
    if (requestBodies.size() != 8 || requestBodies.at(7).value("messages") !=
        requestBodies.at(0).value("messages")) return 15;
    const QJsonObject firstRegenerated = room.snapshots().first();
    if (room.snapshots().size() != 2 || firstRegenerated.value("message_id") != messageIds.value("1") ||
        firstRegenerated.value("full_text") != QStringLiteral("新版本一") ||
        firstRegenerated.value("request_id") != QStringLiteral("3") ||
        firstRegenerated.value("generation_id") == events.first().value("generation_id")) return 16;
    if (!room.regenerate(1, 11)) return 17;
    if (!until([&]() {
        for (const QJsonObject &item : events)
            if (item.value("event") == QStringLiteral("ai_complete") &&
                item.value("request_id") == QStringLiteral("4")) return true;
        return false;
    })) return 18;
    const QJsonObject secondRegenerated = room.snapshots().first();
    if (secondRegenerated.value("full_text") != QStringLiteral("```cpp\nint answer = 2;\n```") ||
        secondRegenerated.value("message_id") != messageIds.value("1") ||
        secondRegenerated.value("generation_id") == firstRegenerated.value("generation_id")) return 19;
    if (!room.regenerate(1, 11)) return 20;
    if (!until([&]() {
        for (const QJsonObject &item : events)
            if (item.value("event") == QStringLiteral("ai_error") &&
                item.value("request_id") == QStringLiteral("5")) return true;
        return false;
    })) return 21;
    if (room.snapshots().first() != secondRegenerated || requestBodies.size() != 10 ||
        requestBodies.at(9).value("messages") != requestBodies.at(0).value("messages")) return 22;
    if (!room.regenerate(1, 11) || !room.cancel(6, 11)) return 23;
    if (!until([&]() {
        for (const QJsonObject &item : events)
            if (item.value("event") == QStringLiteral("ai_error") &&
                item.value("request_id") == QStringLiteral("6")) return true;
        return false;
    }) || room.snapshots().first() != secondRegenerated) return 24;
    return 0;
}
