#include "mainwindow.h"
#include <QApplication>
#include <QEventLoop>
#include <QHash>
#include <QListWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) return 1;
    QHash<QString, QTcpSocket *> peers;
    QObject::connect(&server, &QTcpServer::newConnection, &app, [&]() {
        while (server.hasPendingConnections()) {
            auto *peer = server.nextPendingConnection();
            QObject::connect(peer, &QTcpSocket::readyRead, &app, [&, peer]() {
                const QByteArray data = peer->readAll();
                if (data.startsWith("JOIN|"))
                    peers.insert(QString::fromUtf8(data.mid(5)).trimmed(), peer);
            });
        }
    });
    const QString address = QStringLiteral("127.0.0.1:%1").arg(server.serverPort());
    auto wait = [](int ms = 120) {
        QEventLoop loop;
        QTimer::singleShot(ms, &loop, &QEventLoop::quit);
        loop.exec();
    };
    auto broadcast = [&](const QString &list) {
        const QByteArray line = (QStringLiteral("USERLIST|") + list + QLatin1Char('\n')).toUtf8();
        for (auto *peer : peers)
            peer->write(line);
        wait();
    };
    auto check = [](MainWindow *window, const QStringList &order) {
        auto *list = window->findChild<QListWidget *>(QStringLiteral("userList"));
        auto *badge = window->findChild<QLabel *>(QStringLiteral("onlineBadge"));
        if (!list || !badge || list->count() != order.size() ||
            badge->text() != QStringLiteral("%1 人在线").arg(order.size()) ||
            window->findChildren<QFrame *>(QStringLiteral("viviCard")).size() != 1)
            return false;
        for (int i = 0; i < order.size(); ++i) {
            QWidget *item = list->itemWidget(list->item(i));
            if (!item) return false;
            QStringList labels;
            for (QLabel *label : item->findChildren<QLabel *>()) labels.append(label->text());
            const QString expected = order.at(i) + (i == 0 ? QStringLiteral("（我）") : QString());
            if (!labels.contains(expected)) return false;
        }
        return true;
    };

    auto *a = new MainWindow(QStringLiteral("A"), address);
    a->show();
    wait();
    if (peers.size() != 1) return 2;
    broadcast(QStringLiteral("A"));
    if (!check(a, {QStringLiteral("A")})) return 3;

    auto *b = new MainWindow(QStringLiteral("B"), address);
    b->show();
    wait();
    if (peers.size() != 2) return 4;
    broadcast(QStringLiteral("B,A"));
    if (!check(a, {QStringLiteral("A"), QStringLiteral("B")}) ||
        !check(b, {QStringLiteral("B"), QStringLiteral("A")})) return 5;

    auto *c = new MainWindow(QStringLiteral("C"), address);
    c->show();
    wait();
    if (peers.size() != 3) return 6;
    broadcast(QStringLiteral("C,A,B"));
    if (!check(a, {QStringLiteral("A"), QStringLiteral("C"), QStringLiteral("B")}) ||
        !check(b, {QStringLiteral("B"), QStringLiteral("C"), QStringLiteral("A")}) ||
        !check(c, {QStringLiteral("C"), QStringLiteral("A"), QStringLiteral("B")})) return 7;

    peers.take(QStringLiteral("B"))->disconnectFromHost();
    wait();
    broadcast(QStringLiteral("C,A"));
    if (!check(a, {QStringLiteral("A"), QStringLiteral("C")}) ||
        !check(c, {QStringLiteral("C"), QStringLiteral("A")})) return 8;
    QWidget *first = a->findChild<QListWidget *>(QStringLiteral("userList"))->itemWidget(
        a->findChild<QListWidget *>(QStringLiteral("userList"))->item(0));
    broadcast(QStringLiteral("C,A"));
    if (first != a->findChild<QListWidget *>(QStringLiteral("userList"))->itemWidget(
                     a->findChild<QListWidget *>(QStringLiteral("userList"))->item(0))) return 9;

    delete a;
    delete b;
    delete c;
    return 0;
}
