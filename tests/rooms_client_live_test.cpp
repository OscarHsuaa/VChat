// Run only with disposable VCHAT_TEST_* accounts and an isolated server port.
#include "mainwindow.h"
#include "aimarkdownwidget.h"
#include "mentioninput.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QTcpSocket>
#include <QTextBrowser>
#include <QTimer>
#include <QUuid>
#include <functional>

namespace {
void processFor(int ms = 20)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}
bool waitUntil(const std::function<bool()> &predicate, int ms = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < ms) processFor();
    return predicate();
}
struct Auth { QString name, token; qint64 id = 0; };
struct Peer {
    QTcpSocket socket;
    QByteArray buffer;
    QStringList lines;
    bool connectTo(quint16 port)
    {
        socket.connectToHost(QStringLiteral("127.0.0.1"), port);
        return socket.waitForConnected(3000);
    }
    void send(const QByteArray &line) { socket.write(line + '\n'); socket.flush(); }
    void poll()
    {
        buffer += socket.readAll();
        int end;
        while ((end = buffer.indexOf('\n')) >= 0) {
            lines.append(QString::fromUtf8(buffer.left(end).trimmed()));
            buffer.remove(0, end + 1);
        }
    }
    bool await(const std::function<bool(const QString &)> &match, int ms = 4000)
    {
        QElapsedTimer timer;
        timer.start();
        do {
            poll();
            for (const QString &line : lines) if (match(line)) return true;
            if (!socket.waitForReadyRead(qMax(1, qMin(50, ms - int(timer.elapsed())))) &&
                socket.state() != QAbstractSocket::ConnectedState) break;
        } while (timer.elapsed() < ms);
        return false;
    }
    QJsonObject roomRequest(const QString &action, const QJsonObject &args)
    {
        lines.clear();
        send("ROOM|" + action.toUtf8() + '|' + QJsonDocument(args).toJson(QJsonDocument::Compact).toBase64());
        QJsonObject result;
        await([&](const QString &line) {
            if (!line.startsWith(QStringLiteral("ROOM|result|"))) return false;
            const auto doc = QJsonDocument::fromJson(QByteArray::fromBase64(line.mid(12).toLatin1()));
            if (!doc.isObject() || doc.object().value(QStringLiteral("action")).toString() != action) return false;
            result = doc.object();
            return true;
        });
        return result;
    }
};
Auth authenticate(quint16 port, const QByteArray &user, const QByteArray &password)
{
    Peer peer;
    if (!peer.connectTo(port)) return {};
    peer.send("LOGIN|" + user + '|' + password);
    Auth result;
    peer.await([&](const QString &line) {
        if (line.startsWith(QStringLiteral("LOGIN_META|"))) {
            const QStringList fields = line.split('|');
            if (fields.size() == 3) {
                result.id = fields.at(1).toLongLong();
                result.token = fields.at(2);
            }
        }
        if (line.startsWith(QStringLiteral("LOGIN_OK|"))) result.name = line.mid(9);
        return !result.name.isEmpty() && !result.token.isEmpty();
    });
    return result;
}
bool selectRoom(MainWindow &window, const QString &id)
{
    auto *list = window.findChild<QListWidget *>(QStringLiteral("roomList"));
    if (!list) return false;
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->data(Qt::UserRole).toString() == id) {
            list->itemClicked(list->item(i));
            return waitUntil([&]() { return list->item(i)->isSelected(); });
        }
    return false;
}
bool visibleText(MainWindow &window, const QString &text)
{
    for (QLabel *label : window.findChildren<QLabel *>())
        if (label->text() == text) return true;
    return false;
}
void sendChat(MainWindow &window, const QString &text)
{
    window.findChild<MentionInput *>(QStringLiteral("messageInput"))->setPlainText(text);
    window.findChild<QPushButton *>(QStringLiteral("sendButton"))->click();
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const int port = qEnvironmentVariableIntValue("VCHAT_TEST_PORT");
    const QByteArray userA = qgetenv("VCHAT_TEST_USER_A"), passA = qgetenv("VCHAT_TEST_PASS_A");
    const QByteArray userB = qgetenv("VCHAT_TEST_USER_B"), passB = qgetenv("VCHAT_TEST_PASS_B");
    if (port <= 0 || port >= 65536 || port == 8080 || userA.isEmpty() || passA.isEmpty() ||
        userB.isEmpty() || passB.isEmpty()) return 1;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
        QString::fromLocal8Bit(qgetenv("TEMP")) + QStringLiteral("/VChatRoomIntegration"));
    const Auth aAuth = authenticate(quint16(port), userA, passA);
    const Auth bAuth = authenticate(quint16(port), userB, passB);
    if (aAuth.id <= 0 || bAuth.id <= 0 || aAuth.id == bAuth.id) return 2;
    Peer controlA, controlB;
    if (!controlA.connectTo(quint16(port)) || !controlB.connectTo(quint16(port))) return 3;
    controlA.send("JOIN|" + aAuth.name.toUtf8() + '|' + aAuth.token.toUtf8());
    controlB.send("JOIN|" + bAuth.name.toUtf8() + '|' + bAuth.token.toUtf8());
    if (!controlA.await([](const QString &line) { return line == QStringLiteral("HISTORY_END"); }) ||
        !controlB.await([](const QString &line) { return line == QStringLiteral("HISTORY_END"); })) return 4;
    const QString roomName = QStringLiteral("UI 隔离测试 ") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString roomId = controlA.roomRequest(QStringLiteral("create"),
        {{QStringLiteral("room_name"), roomName}, {QStringLiteral("room_type"), QStringLiteral("public")}})
        .value(QStringLiteral("room")).toObject().value(QStringLiteral("room_id")).toString();
    if (roomId.isEmpty() || !controlB.roomRequest(QStringLiteral("join_public"),
        {{QStringLiteral("room_id"), roomId}}).value(QStringLiteral("ok")).toBool()) return 5;
    controlA.socket.disconnectFromHost();
    controlB.socket.disconnectFromHost();
    const QString address = QStringLiteral("127.0.0.1:%1").arg(port);
    MainWindow a(aAuth.name, address, nullptr, aAuth.id, aAuth.token);
    MainWindow b(bAuth.name, address, nullptr, bAuth.id, bAuth.token);
    a.show(); b.show();
    if (!waitUntil([&]() {
        auto *left = a.findChild<QListWidget *>(QStringLiteral("roomList"));
        auto *right = b.findChild<QListWidget *>(QStringLiteral("roomList"));
        return left && right && left->count() >= 2 && right->count() >= 2;
    }, 8000)) return 6;
    if (!selectRoom(a, roomId) || !selectRoom(b, roomId)) return 7;
    if (!waitUntil([&]() {
        auto *countA = a.findChild<QLabel *>(QStringLiteral("onlineBadge"));
        auto *countB = b.findChild<QLabel *>(QStringLiteral("onlineBadge"));
        return countA && countB && countA->text().contains(QStringLiteral("2 人在线")) &&
            countB->text().contains(QStringLiteral("2 人在线"));
    }, 8000)) return 8;
    const QString first = QStringLiteral("room-live-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    sendChat(a, first);
    if (!waitUntil([&]() { return visibleText(b, first); }, 6000)) return 9;
    if (!selectRoom(a, QStringLiteral("class-room"))) return 10;
    const QString second = QStringLiteral("room-hidden-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    sendChat(b, second);
    if (!waitUntil([&]() { return visibleText(b, second); }, 5000)) return 11;
    processFor(150);
    if (visibleText(a, second)) return 12;
    const int lobbyAiCount = a.findChildren<AiMarkdownWidget *>().size();
    sendChat(b, QStringLiteral("@Vivi 请用一句简短中文回答：多人房间联调是否正常？"));
    if (!waitUntil([&]() {
        for (AiMarkdownWidget *body : b.findChildren<AiMarkdownWidget *>())
            if (!body->plainText().isEmpty() && body->findChild<QTextBrowser *>()) return true;
        return false;
    }, 65000)) return 14;
    if (a.findChildren<AiMarkdownWidget *>().size() != lobbyAiCount) return 17;
    if (!selectRoom(a, roomId) || !waitUntil([&]() { return visibleText(a, second); }, 7000)) return 13;
    if (!waitUntil([&]() {
        for (AiMarkdownWidget *body : a.findChildren<AiMarkdownWidget *>())
            if (!body->plainText().isEmpty() && body->findChild<QTextBrowser *>()) return true;
        return false;
    }, 15000)) return 15;
    MainWindow resumed(aAuth.name, address, nullptr, aAuth.id, aAuth.token);
    resumed.show();
    if (!waitUntil([&]() {
        auto *list = resumed.findChild<QListWidget *>(QStringLiteral("roomList"));
        if (!list) return false;
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->data(Qt::UserRole).toString() == roomId && list->item(i)->isSelected())
                return visibleText(resumed, second);
        return false;
    }, 15000)) return 16;
    std::fputs("PASS: two Qt clients, room navigation, online members, isolated history, Vivi, restore\n", stdout);
    return 0;
}
