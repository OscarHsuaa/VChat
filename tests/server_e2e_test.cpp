#include <QCoreApplication>
#include <QEventLoop>
#include <QElapsedTimer>
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

static bool loginJoin(Client &client, const QByteArray &user, const QByteArray &password, quint16 port)
{
    QTcpSocket auth;
    auth.connectToHost(QHostAddress::LocalHost, port);
    if (!auth.waitForConnected(3000)) return false;
    auth.write("LOGIN|" + user + '|' + password + '\n');
    auth.flush();
    QByteArray received;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 3000 && (!received.contains("LOGIN_META|") || !received.contains("LOGIN_OK|"))) {
        if (auth.waitForReadyRead(3000 - int(timer.elapsed()))) received += auth.readAll();
    }
    QByteArray token, nickname;
    for (const QByteArray &line : received.split('\n')) {
        if (line.startsWith("LOGIN_META|")) token = line.trimmed().split('|').value(2);
        if (line.startsWith("LOGIN_OK|")) nickname = line.trimmed().split('|').value(1);
    }
    auth.disconnectFromHost();
    if (token.isEmpty() || nickname.isEmpty()) return false;
    client.socket.connectToHost(QHostAddress::LocalHost, port);
    if (!client.socket.waitForConnected(3000)) return false;
    client.socket.write("JOIN|" + nickname + '|' + token + '\n');
    return client.socket.waitForBytesWritten(1000);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    Client a, b, rejoined;
    for (Client *client : {&a, &b, &rejoined}) observe(*client);
    const QByteArray portText = qgetenv("VCHAT_TEST_PORT");
    const int port = portText.toInt();
    const QByteArray userA = qgetenv("VCHAT_TEST_USER_A"), passwordA = qgetenv("VCHAT_TEST_PASS_A");
    const QByteArray userB = qgetenv("VCHAT_TEST_USER_B"), passwordB = qgetenv("VCHAT_TEST_PASS_B");
    if (port <= 0 || port >= 65536 || port == 8080 || userA.isEmpty() || passwordA.isEmpty() ||
        userB.isEmpty() || passwordB.isEmpty()) return 1;
    if (!loginJoin(a, userA, passwordA, quint16(port)) ||
        !loginJoin(b, userB, passwordB, quint16(port))) return 1;
    until([]() { return false; }, 200);
    a.socket.write("@Vivi 请用一句简短中文确认流式连接。\n");
    if (!until([&]() { return !finalEvent(a).isEmpty() && !finalEvent(b).isEmpty(); }, 65000)) return 3;
    const QJsonObject answerA = finalEvent(a), answerB = finalEvent(b);
    int chunksA = 0, chunksB = 0;
    for (const QJsonObject &event : a.events) if (event.value("event").toString() == QStringLiteral("ai_chunk")) ++chunksA;
    for (const QJsonObject &event : b.events) if (event.value("event").toString() == QStringLiteral("ai_chunk")) ++chunksB;
    if (chunksA == 0 || chunksA != chunksB ||
        answerA.value("message_id").toString() != answerB.value("message_id").toString() ||
        answerA.value("full_text").toString().isEmpty() ||
        answerA.value("full_text") != answerB.value("full_text")) return 4;
    if (!loginJoin(rejoined, userA, passwordA, quint16(port))) return 5;
    if (!until([&]() { return !finalEvent(rejoined).isEmpty(); }, 3000) ||
        finalEvent(rejoined).value("message_id") != answerA.value("message_id")) return 6;
    std::fprintf(stdout, "SERVER_E2E clients=3 chunks_each=%d same_message=true snapshot=true\n", chunksA);
    return 0;
}
