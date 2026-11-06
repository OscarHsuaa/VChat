#include "airoom.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

static QStringList capturedWarnings;
static void captureWarning(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    if (type == QtWarningMsg) capturedWarnings.append(message);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTcpServer mock;
    if (!mock.listen(QHostAddress::LocalHost, 0)) return 1;
    int requests = 0;
    QList<QJsonObject> bodies;
    QObject::connect(&mock, &QTcpServer::newConnection, &app, [&]() {
        auto *peer = mock.nextPendingConnection();
        auto *buffer = new QByteArray;
        QObject::connect(peer, &QObject::destroyed, &app, [buffer]() { delete buffer; });
        QObject::connect(peer, &QTcpSocket::readyRead, peer, [&, peer, buffer]() {
            buffer->append(peer->readAll());
            const int headerEnd = buffer->indexOf("\r\n\r\n");
            if (headerEnd < 0) return;
            const QByteArray header = buffer->left(headerEnd);
            int length = -1;
            for (const QByteArray &line : header.split('\n')) {
                if (line.trimmed().toLower().startsWith("content-length:"))
                    length = line.mid(line.indexOf(':') + 1).trimmed().toInt();
            }
            if (length < 0 || buffer->size() < headerEnd + 4 + length) return;
            bodies.append(QJsonDocument::fromJson(buffer->mid(headerEnd + 4, length)).object());
            ++requests;
            const QString answer = requests == 1 ? QStringLiteral("[A] 回答1")
                                                 : QStringLiteral("回答%1").arg(requests);
            const QByteArray json = QJsonDocument(QJsonObject{
                {QStringLiteral("choices"), QJsonArray{QJsonObject{
                    {QStringLiteral("message"), QJsonObject{{QStringLiteral("content"), answer}}}}}}}).toJson();
            peer->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                        + QByteArray::number(json.size()) + "\r\nConnection: close\r\n\r\n" + json);
            peer->disconnectFromHost();
        });
    });
    qputenv("DEEPSEEK_API_KEY", "local-test-only");
    AiService service(nullptr, QUrl(QStringLiteral("http://127.0.0.1:%1/chat/completions").arg(mock.serverPort())));
    QStringList events;
    QStringList targetTrace;
    AiRoom *roomPtr = nullptr;
    AiRoom room(&service, [&](const QString &state, quint64 id, const QString &text) {
        events << state + QStringLiteral(":") + text;
        if (state == QStringLiteral("reply")) {
            const auto target = roomPtr->replyTarget(id);
            targetTrace << QString::number(target.accountId) + QStringLiteral(":") + target.nickname;
        }
    });
    roomPtr = &room;
    room.addUserMessage(11, QStringLiteral("A"), QStringLiteral("普通聊天"), false);
    room.addUserMessage(11, QStringLiteral("A"), QStringLiteral("@Vivi 你好"), true);
    room.addUserMessage(22, QStringLiteral("B"), QStringLiteral("@Vivi 接着说"), true);
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&]() {
        if (events.filter(QStringLiteral("reply:")).size() == 2) loop.quit();
    });
    poll.start(20);
    loop.exec();
    if (requests != 2 || bodies.size() != 2) return 2;
    if (bodies.at(0).value(QStringLiteral("model")).toString() != QStringLiteral("deepseek-flash") ||
        bodies.at(0).value(QStringLiteral("thinking")).toObject().value(QStringLiteral("type")).toString() != QStringLiteral("disabled") ||
        bodies.at(0).value(QStringLiteral("stream")).toBool() ||
        bodies.at(0).value(QStringLiteral("max_tokens")).toInt() != 1024) return 3;
    const QString second = QString::fromUtf8(QJsonDocument(bodies.at(1).value(QStringLiteral("messages")).toArray()).toJson());
    if (!second.contains(QStringLiteral("[A] 普通聊天")) || !second.contains(QStringLiteral("回答1")) ||
        !second.contains(QStringLiteral("[B] @Vivi 接着说"))) return 4;
    if (events.filter(QStringLiteral("thinking:")).size() != 2 ||
        events.filter(QStringLiteral("reply:")).size() != 2) return 5;
    if (targetTrace != QStringList{QStringLiteral("11:A"), QStringLiteral("22:B")} ||
        !events.contains(QStringLiteral("reply:回答1"))) return 15;
    if (!room.regenerate(1) || room.regenerate(99999)) return 12;
    QEventLoop regenerateLoop;
    QTimer::singleShot(3000, &regenerateLoop, &QEventLoop::quit);
    QTimer regeneratePoll;
    QObject::connect(&regeneratePoll, &QTimer::timeout, &regenerateLoop, [&]() {
        if (events.filter(QStringLiteral("reply:")).size() == 3) regenerateLoop.quit();
    });
    regeneratePoll.start(20);
    regenerateLoop.exec();
    if (requests != 3 || bodies.size() != 3) return 13;
    if (targetTrace.last() != QStringLiteral("11:A")) return 16;
    if (bodies.at(2).value(QStringLiteral("messages")).toArray() !=
        bodies.at(0).value(QStringLiteral("messages")).toArray()) return 14;
    qunsetenv("DEEPSEEK_API_KEY");
    AiRoom missing(&service, [&](const QString &state, quint64, const QString &text) {
        events << state + QStringLiteral(":") + text;
    });
    missing.addUserMessage(11, QStringLiteral("A"), QStringLiteral("@Vivi test"), true);
    QEventLoop missingLoop;
    QTimer::singleShot(100, &missingLoop, &QEventLoop::quit);
    missingLoop.exec();
    if (requests != 3 || events.filter(QStringLiteral("error:")).size() != 1) return 6;
    qputenv("DEEPSEEK_API_KEY", "local-test-only");
    QString networkError;
    AiService unavailable(nullptr, QUrl(QStringLiteral("http://127.0.0.1:1/chat/completions")), 100);
    unavailable.complete(QJsonArray{}, [&](const QString &, const QString &error) { networkError = error; });
    QEventLoop errorLoop;
    QTimer::singleShot(300, &errorLoop, &QEventLoop::quit);
    errorLoop.exec();
    if (networkError.isEmpty()) return 7;
    QTcpServer stalled;
    if (!stalled.listen(QHostAddress::LocalHost, 0)) return 8;
    QObject::connect(&stalled, &QTcpServer::newConnection, &app, [&]() {
        stalled.nextPendingConnection(); // Keep the socket open without responding.
    });
    QString timeoutError;
    AiService timeout(nullptr, QUrl(QStringLiteral("http://127.0.0.1:%1/chat/completions").arg(stalled.serverPort())), 100);
    timeout.complete(QJsonArray{}, [&](const QString &, const QString &error) { timeoutError = error; });
    QEventLoop timeoutLoop;
    QTimer::singleShot(600, &timeoutLoop, &QEventLoop::quit);
    timeoutLoop.exec();
    if (!timeoutError.contains(QStringLiteral("超时"))) return 9;
    QTcpServer rejected;
    if (!rejected.listen(QHostAddress::LocalHost, 0)) return 10;
    QObject::connect(&rejected, &QTcpServer::newConnection, &app, [&]() {
        auto *peer = rejected.nextPendingConnection();
        QObject::connect(peer, &QTcpSocket::readyRead, peer, [peer]() {
            peer->readAll();
            const QByteArray body = R"({"error":{"message":"SECRET_SENTINEL"}})";
            peer->write("HTTP/1.1 429 Too Many Requests\r\nContent-Type: application/json\r\nContent-Length: "
                        + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            peer->disconnectFromHost();
        });
    });
    QString rejectedError;
    const QtMessageHandler previous = qInstallMessageHandler(captureWarning);
    AiService limited(nullptr, QUrl(QStringLiteral("http://127.0.0.1:%1/chat/completions").arg(rejected.serverPort())), 1000);
    limited.complete(QJsonArray{}, [&](const QString &, const QString &error) { rejectedError = error; });
    QEventLoop rejectedLoop;
    QTimer::singleShot(500, &rejectedLoop, &QEventLoop::quit);
    rejectedLoop.exec();
    qInstallMessageHandler(previous);
    const QString log = capturedWarnings.join(QLatin1Char('\n'));
    if (rejectedError.isEmpty() || !log.contains(QStringLiteral("HTTP 429")) ||
        !log.contains(QStringLiteral("rate_limit")) ||
        log.contains(QStringLiteral("SECRET_SENTINEL")) ||
        log.contains(QStringLiteral("local-test-only"))) return 11;
    return 0;
}
