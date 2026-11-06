#include "mainwindow.h"
#include <QApplication>
#include <QTcpServer>
#include <QTcpSocket>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 18080)) return 2;
    QObject::connect(&server, &QTcpServer::newConnection, &app, [&server]() {
        QTcpSocket *client = server.nextPendingConnection();
        client->write("[System] Connected to chat server\n");
        QObject::connect(client, &QTcpSocket::readyRead, client, [client]() {
            const QByteArray data = client->readAll();
            if (data.startsWith("JOIN|")) {
                client->write("USERLIST|Preview User\n");
            }
        });
    });
    MainWindow window(QStringLiteral("Preview User"), QStringLiteral("127.0.0.1:18080"));
    window.show();
    return app.exec();
}
