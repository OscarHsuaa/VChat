#include <QCoreApplication>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTimer>
#include <cstdio>

struct Client {
    QTcpSocket socket;
    QByteArray buffer;
    QList<QJsonObject> events;
};

static bool until(const std::function<bool()> &condition, int timeoutMs)
{
    QEventLoop loop;
    QTimer timer;
    timer.setInterval(15);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() { if (condition()) loop.quit(); });
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    timer.start();
    loop.exec();
    return condition();
}

static void observe(Client &client)
{
    QObject::connect(&client.socket, &QTcpSocket::readyRead, &client.socket, [&client]() {
        client.buffer += client.socket.readAll();
        int end = -1;
        while ((end = client.buffer.indexOf('\n')) >= 0) {
            const QByteArray line = client.buffer.left(end).trimmed();
            client.buffer.remove(0, end + 1);
            if (!line.startsWith("AI|stream|")) continue;
            const QList<QByteArray> parts = line.split('|');
            if (parts.size() != 4) continue;
            const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromBase64(parts.at(3)));
            if (document.isObject()) client.events.append(document.object());
        }
    });
}

static QJsonObject finalEvent(const Client &client)
{
    for (const QJsonObject &event : client.events)
        if (event.value("event").toString() == QStringLiteral("ai_complete")) return event;
    return {};
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    Client a, b, rejoined;
    for (Client *client : {&a, &b, &rejoined}) observe(*client);
    a.socket.connectToHost(QHostAddress::LocalHost, 8080);
    b.socket.connectToHost(QHostAddress::LocalHost, 8080);
    if (!a.socket.waitForConnected(3000) || !b.socket.waitForConnected(3000)) return 1;
    a.socket.write("JOIN|VChatStreamTestA\n");
    b.socket.write("JOIN|VChatStreamTestB\n");
    a.socket.waitForBytesWritten(1000);
    b.socket.waitForBytesWritten(1000);
    until([]() { return false; }, 200);
    a.socket.write("VChatStreamTestA: @Vivi 请用一句简短中文确认流式连接。\n");
    if (!until([&]() { return !finalEvent(a).isEmpty() && !finalEvent(b).isEmpty(); }, 65000)) return 3;
    const QJsonObject answerA = finalEvent(a), answerB = finalEvent(b);
    int chunksA = 0, chunksB = 0;
    for (const QJsonObject &event : a.events) if (event.value("event").toString() == QStringLiteral("ai_chunk")) ++chunksA;
    for (const QJsonObject &event : b.events) if (event.value("event").toString() == QStringLiteral("ai_chunk")) ++chunksB;
    if (chunksA == 0 || chunksA != chunksB ||
        answerA.value("message_id").toString() != answerB.value("message_id").toString() ||
        answerA.value("full_text").toString().isEmpty() ||
        answerA.value("full_text") != answerB.value("full_text")) return 4;
    rejoined.socket.connectToHost(QHostAddress::LocalHost, 8080);
    if (!rejoined.socket.waitForConnected(3000)) return 5;
    rejoined.socket.write("JOIN|VChatStreamTestC\n");
    if (!until([&]() { return !finalEvent(rejoined).isEmpty(); }, 3000) ||
        finalEvent(rejoined).value("message_id") != answerA.value("message_id")) return 6;
    std::fprintf(stdout, "SERVER_E2E clients=3 chunks_each=%d same_message=true snapshot=true\n", chunksA);
    return 0;
}
