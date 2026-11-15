#include "airoom.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <cstdio>

static bool waitFor(const std::function<bool()> &condition)
{
    QEventLoop loop;
    QTimer poll;
    poll.setInterval(10);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] { if (condition()) loop.quit(); });
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    poll.start();
    loop.exec();
    return condition();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    qputenv("DEEPSEEK_API_KEY", "local-test-only");
    QTcpServer http;
    if (!http.listen(QHostAddress::LocalHost, 0)) return 1;
    QList<QJsonArray> requests;
    QObject::connect(&http, &QTcpServer::newConnection, &app, [&] {
        QTcpSocket *peer = http.nextPendingConnection();
        auto *buffer = new QByteArray;
        QObject::connect(peer, &QObject::destroyed, &app, [buffer] { delete buffer; });
        QObject::connect(peer, &QTcpSocket::readyRead, peer, [&, peer, buffer] {
            buffer->append(peer->readAll());
            const int end = buffer->indexOf("\r\n\r\n");
            if (end < 0) return;
            int length = -1;
            for (const QByteArray &line : buffer->left(end).split('\n'))
                if (line.trimmed().toLower().startsWith("content-length:"))
                    length = line.mid(line.indexOf(':') + 1).trimmed().toInt();
            if (length < 0 || buffer->size() < end + 4 + length) return;
            requests.append(QJsonDocument::fromJson(buffer->mid(end + 4, length))
                                .object().value(QStringLiteral("messages")).toArray());
            const QByteArray payload = "data: {\"choices\":[{\"delta\":{\"content\":\"ok\"}}]}\n\n"
                                       "data: [DONE]\n\n";
            peer->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: " +
                        QByteArray::number(payload.size()) + "\r\nConnection: close\r\n\r\n" + payload);
            peer->disconnectFromHost();
        });
    });
    AiService service(nullptr, QUrl(QStringLiteral("http://127.0.0.1:%1/chat/completions")
                                  .arg(http.serverPort())));
    auto message = [](qint64 sequence, const QString &id, qint64 user,
                      const QString &name, const QString &body) {
        return AiRoom::HistoryMessage{sequence, id, QStringLiteral("user"),
            AiRoom::memberContent(user, name, body, QStringLiteral("2026-01-01T12:00:00"), sequence)};
    };
    // A fresh AiRoom simulates a server restart. The loader supplies persisted rows.
    AiRoom roomA(&service, [](const QString &, quint64, const QString &) {});
    std::function<void(AiRoom::HistoryResult)> finishA;
    roomA.setHistoryLoader([&](qint64 cutoff, int limit,
                               std::function<void(AiRoom::HistoryResult)> done) {
        if (cutoff != 4 || limit != 21) qFatal("wrong history boundary");
        finishA = std::move(done);
    });
    roomA.addUserMessage(11, QStringLiteral("甲"), QStringLiteral("@Vivi 总结"), true,
                         QStringLiteral("a4"), 4, QStringLiteral("2026-01-01T12:04:00"));
    roomA.addUserMessage(22, QStringLiteral("乙"), QStringLiteral("后续新消息"), false,
                         QStringLiteral("a5"), 5, QStringLiteral("2026-01-01T12:05:00"));
    if (!finishA || !requests.isEmpty()) return 2;
    finishA({{message(1, QStringLiteral("a1"), 11, QStringLiteral("甲"), QStringLiteral("方案甲")),
              message(2, QStringLiteral("a2"), 22, QStringLiteral("乙"), QStringLiteral("方案乙")),
              {3, QStringLiteral("a3"), QStringLiteral("assistant"), QStringLiteral("Vivi 旧回复")},
              message(4, QStringLiteral("a4"), 11, QStringLiteral("甲"), QStringLiteral("@Vivi 总结")),
              message(5, QStringLiteral("a5"), 22, QStringLiteral("乙"), QStringLiteral("后续新消息"))}, {}});
    if (!waitFor([&] { return requests.size() == 1 && roomA.snapshots().first()
        .value(QStringLiteral("event")) == QStringLiteral("ai_complete"); })) {
        std::fprintf(stderr, "context test first request: requests=%lld state=%s\n",
            static_cast<long long>(requests.size()),
            QJsonDocument(roomA.snapshots().first()).toJson(QJsonDocument::Compact).constData());
        return 3;
    }
    const QByteArray first = QJsonDocument(requests.first()).toJson(QJsonDocument::Compact);
    if (!first.contains("方案甲") || !first.contains("方案乙") ||
        !first.contains("Vivi 旧回复") || first.contains("后续新消息")) return 4;
    int questionCount = 0;
    for (const QJsonValue &value : requests.first())
        questionCount += value.toObject().value(QStringLiteral("content")).toString()
                             .contains(QStringLiteral("@Vivi 总结"));
    if (questionCount != 1) return 5;
    if (!roomA.regenerate(1, 11) || !waitFor([&] { return requests.size() == 2; }) ||
        requests.at(1) != requests.at(0)) return 6;

    AiRoom roomB(&service, [](const QString &, quint64, const QString &) {});
    roomB.setHistoryLoader([&](qint64, int, std::function<void(AiRoom::HistoryResult)> done) {
        done({{message(1, QStringLiteral("b1"), 33, QStringLiteral("丙"), QStringLiteral("房间B秘密"))}, {}});
    });
    roomB.addUserMessage(33, QStringLiteral("丙"), QStringLiteral("@Vivi 回答"), true,
                         QStringLiteral("b2"), 2, QStringLiteral("2026-01-01T13:00:00"));
    if (!waitFor([&] { return requests.size() == 3; })) return 7;
    const QByteArray second = QJsonDocument(requests.at(2)).toJson(QJsonDocument::Compact);
    if (!second.contains("房间B秘密") || second.contains("方案甲")) return 8;

    AiRoom failing(&service, [](const QString &, quint64, const QString &) {});
    QString failure;
    failing.setHistoryLoader([](qint64, int, std::function<void(AiRoom::HistoryResult)> done) {
        done({{}, QStringLiteral("database offline")});
    });
    // The query error must not silently send an incomplete prompt to DeepSeek.
    failing.addUserMessage(44, QStringLiteral("丁"), QStringLiteral("@Vivi 回答"), true,
                           QStringLiteral("c1"), 1);
    failure = failing.snapshots().first().value(QStringLiteral("error")).toString();
    if (failure.isEmpty() || requests.size() != 3) return 9;

    AiRoom many(&service, [](const QString &, quint64, const QString &) {});
    many.setHistoryLoader([&](qint64, int, std::function<void(AiRoom::HistoryResult)> done) {
        AiRoom::HistoryResult rows;
        for (int i = 1; i <= 25; ++i)
            rows.messages.append(message(i, QStringLiteral("m%1").arg(i), i,
                                         QStringLiteral("成员"), QStringLiteral("内容%1").arg(i)));
        done(rows);
    });
    many.addUserMessage(99, QStringLiteral("提问者"), QStringLiteral("@Vivi 总结"), true,
                        QStringLiteral("m26"), 26);
    if (!waitFor([&] { return requests.size() == 4; })) return 10;
    if (requests.at(3).size() != 22 ||
        QJsonDocument(requests.at(3)).toJson().contains("内容1\"") ||
        !QJsonDocument(requests.at(3)).toJson().contains("内容25")) return 11;
    return 0;
}
