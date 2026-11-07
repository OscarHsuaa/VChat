#include "airoom.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <functional>

static bool waitFor(const std::function<bool()> &condition, int timeout = 3000)
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (!condition() && elapsed.elapsed() < timeout) {
        QEventLoop loop;
        QTimer::singleShot(10, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return condition();
}

static QByteArray eventData(const QString &text)
{
    const QJsonObject event{{"choices", QJsonArray{QJsonObject{{"delta", QJsonObject{{"content", text}}}}}}};
    return "data: " + QJsonDocument(event).toJson(QJsonDocument::Compact) + "\n\n";
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    qputenv("DEEPSEEK_API_KEY", "local-test-only");
    QTcpServer http;
    if (!http.listen(QHostAddress::LocalHost, 0)) return 1;
    QList<QTcpSocket *> peers;
    QSet<QTcpSocket *> headers;
    QObject::connect(&http, &QTcpServer::newConnection, &app, [&]() {
        while (http.hasPendingConnections()) {
            QTcpSocket *peer = http.nextPendingConnection();
            auto *input = new QByteArray;
            QObject::connect(peer, &QObject::destroyed, &app, [input]() { delete input; });
            QObject::connect(peer, &QTcpSocket::readyRead, peer, [&, peer, input]() {
                input->append(peer->readAll());
                const int end = input->indexOf("\r\n\r\n");
                if (end < 0) return;
                int length = -1;
                for (const QByteArray &line : input->left(end).split('\n'))
                    if (line.trimmed().toLower().startsWith("content-length:"))
                        length = line.mid(line.indexOf(':') + 1).trimmed().toInt();
                if (length < 0 || input->size() < end + 4 + length || peers.contains(peer)) return;
                peers.append(peer);
            });
        }
    });
    const auto chunk = [&](QTcpSocket *peer, const QByteArray &data) {
        if (!headers.contains(peer)) {
            headers.insert(peer);
            peer->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n"
                        "Transfer-Encoding: chunked\r\n\r\n");
        }
        peer->write(QByteArray::number(data.size(), 16) + "\r\n" + data + "\r\n");
        peer->flush();
    };
    const auto finish = [&](QTcpSocket *peer, const QString &text) {
        chunk(peer, eventData(text));
        chunk(peer, "data: [DONE]\n\n");
        peer->write("0\r\n\r\n");
        peer->disconnectFromHost();
    };
    AiService service(nullptr, QUrl(QStringLiteral("http://127.0.0.1:%1/chat/completions").arg(http.serverPort())), 5000);
    QList<QJsonObject> events;
    AiRoom *roomPtr = nullptr;
    AiRoom room(&service, [&](const QString &state, quint64 id, const QString &text) {
        if (!state.startsWith(QStringLiteral("stream_"))) return;
        const QString kind = state == QStringLiteral("stream_start") ? QStringLiteral("ai_start")
            : state == QStringLiteral("stream_chunk") ? QStringLiteral("ai_chunk")
            : state == QStringLiteral("stream_complete") ? QStringLiteral("ai_complete")
            : QStringLiteral("ai_error");
        events.append(roomPtr->streamEvent(id, kind, text));
    }, nullptr, 2, 1, 700);
    roomPtr = &room;
    const auto ask = [&](int id) {
        return room.addUserMessage(id, QStringLiteral("成员%1").arg(id),
            QStringLiteral("@Vivi 问题%1").arg(id), true, QStringLiteral("question-%1").arg(id));
    };
    const auto state = [&](int id, const QString &kind) {
        for (const QJsonObject &event : events)
            if (event.value("request_id") == QString::number(id) && event.value("event") == kind) return event;
        return QJsonObject{};
    };
    if (!ask(1) || !ask(2) || !waitFor([&]() { return peers.size() == 2; })) return 2;
    chunk(peers[0], eventData(QStringLiteral("甲")));
    chunk(peers[1], eventData(QStringLiteral("乙")));
    finish(peers[1], QStringLiteral("二")); // B finishes before A.
    finish(peers[0], QStringLiteral("一"));
    if (!waitFor([&]() { return !state(1, "ai_complete").isEmpty() && !state(2, "ai_complete").isEmpty(); }) ||
        state(1, "ai_complete").value("full_text") != QStringLiteral("甲一") ||
        state(2, "ai_complete").value("full_text") != QStringLiteral("乙二") ||
        state(1, "ai_complete").value("message_id") == state(2, "ai_complete").value("message_id")) return 3;
    if (!ask(3) || !ask(4) || !waitFor([&]() { return peers.size() == 4; })) return 4;
    finish(peers[2], QStringLiteral("先完成")); // A finishes before B.
    finish(peers[3], QStringLiteral("后完成"));
    if (!waitFor([&]() { return !state(3, "ai_complete").isEmpty() && !state(4, "ai_complete").isEmpty(); })) return 5;
    if (!ask(5) || !ask(6) || !waitFor([&]() { return peers.size() == 6; })) return 6;
    peers[4]->write("HTTP/1.1 429 Too Many Requests\r\nContent-Length: 0\r\n\r\n");
    peers[4]->disconnectFromHost();
    finish(peers[5], QStringLiteral("仍可完成"));
    if (!waitFor([&]() { return !state(5, "ai_error").isEmpty() && !state(6, "ai_complete").isEmpty(); }) ||
        state(5, "ai_error").value("request_state") != QStringLiteral("failed")) return 7;
    if (!ask(7) || !ask(8) || !waitFor([&]() { return peers.size() == 8; })) return 8;
    if (!room.cancel(7, 7)) return 9;
    finish(peers[7], QStringLiteral("取消不影响我"));
    if (!waitFor([&]() { return !state(7, "ai_error").isEmpty() && !state(8, "ai_complete").isEmpty(); }) ||
        state(7, "ai_error").value("request_state") != QStringLiteral("cancelled")) return 10;
    if (!room.regenerate(1, 1) || !ask(9) || !waitFor([&]() { return peers.size() == 10; })) return 11;
    finish(peers[9], QStringLiteral("新提问"));
    finish(peers[8], QStringLiteral("重生成"));
    if (!waitFor([&]() { return !state(9, "ai_complete").isEmpty() && !state(10, "ai_complete").isEmpty(); }) ||
        state(9, "ai_complete").value("message_id") != state(1, "ai_complete").value("message_id")) return 12;
    if (!ask(11) || !ask(12) || !waitFor([&]() { return peers.size() == 12; }) ||
        !ask(13) || ask(14)) return 13; // Two active, one queued, fourth rejected.
    finish(peers[10], QStringLiteral("释放容量"));
    if (!waitFor([&]() { return peers.size() == 13; })) return 14;
    finish(peers[11], QStringLiteral("并行结束"));
    finish(peers[12], QStringLiteral("队列结束"));
    if (!waitFor([&]() { return !state(11, "ai_complete").isEmpty() &&
                              !state(12, "ai_complete").isEmpty() && !state(13, "ai_complete").isEmpty(); })) return 15;
    if (!ask(15) || !waitFor([&]() { return peers.size() == 14; }) ||
        !waitFor([&]() { return !state(14, "ai_error").isEmpty(); }, 3000) ||
        state(14, "ai_error").value("request_state") != QStringLiteral("timed_out")) return 16;
    return 0;
}
