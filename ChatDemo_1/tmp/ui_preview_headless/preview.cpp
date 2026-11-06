#include "mainwindow.h"
#include "aimarkdownwidget.h"
#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QListWidget>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QToolButton>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QTime>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
    QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyhbd.ttc"));
    if (argc < 2) return 2;
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) return 3;
    bool gotRegenerate = false;
    QObject::connect(&server, &QTcpServer::newConnection, &app, [&server, &gotRegenerate]() {
        QTcpSocket *client = server.nextPendingConnection();
        QObject::connect(client, &QTcpSocket::readyRead, client, [client, &gotRegenerate]() {
            const QByteArray data = client->readAll();
            if (data.startsWith("JOIN|")) {
                client->write("USERLIST|Preview User\nAI|status|0|b25saW5l\n");
            } else if (data.contains("@Vivi")) {
                client->write("AI|thinking|42|" + QStringLiteral("Vivi 正在思考…").toUtf8().toBase64() + "\n");
                QTimer::singleShot(230, client, [client]() {
                    client->write("AI|reply|42|" + QStringLiteral("## 示例\n这是 **Vivi** 的 C++ 示例：\n```cpp\n#include <iostream>\nint main() { return 0; }\n```\n完成。")
                        .toUtf8().toBase64() + "\nAI|status|0|b25saW5l\n");
                });
            } else if (data.contains("AI_REGENERATE|42")) {
                gotRegenerate = true;
            }
        });
    });
    MainWindow window(QStringLiteral("Preview User"),
                      QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));
    window.resize(880, 600);
    window.show();
    QTimer::singleShot(200, &window, [&window]() {
        auto *input = window.findChild<QLineEdit *>(QStringLiteral("messageInput"));
        input->setText(QStringLiteral("@Vivi 你好"));
        QMetaObject::invokeMethod(&window, "on_sendButton_clicked", Qt::DirectConnection);
    });
    if (argc >= 3) QTimer::singleShot(300, &app, [&]() { window.grab().save(QString::fromLocal8Bit(argv[2])); });
    QTimer::singleShot(800, &app, [&]() {
        const auto *users = window.findChild<QListWidget *>(QStringLiteral("userList"));
        const auto *badge = window.findChild<QLabel *>(QStringLiteral("onlineBadge"));
        const auto labels = window.findChildren<QLabel *>();
        bool thinking = false, online = false;
        for (const auto *label : labels) {
            thinking |= label->text().startsWith(QStringLiteral("Vivi 正在思考"));
            online |= label->text() == QStringLiteral("在线");
        }
        auto *markdown = window.findChild<AiMarkdownWidget *>();
        auto *code = markdown ? markdown->findChild<QPlainTextEdit *>() : nullptr;
        auto *prose = markdown ? markdown->findChild<QTextBrowser *>() : nullptr;
        QPushButton *copyCode = nullptr;
        for (auto *button : window.findChildren<QPushButton *>())
            if (button->text() == QStringLiteral("复制代码")) copyCode = button;
        if (copyCode) copyCode->click();
        QToolButton *retry = nullptr;
        for (auto *button : window.findChildren<QToolButton *>())
            if (button->text() == QStringLiteral("重新生成") && button->isEnabled()) retry = button;
        AiMarkdownWidget unsafe(QStringLiteral("<b>raw HTML</b>"));
        const auto *unsafeText = unsafe.findChild<QTextBrowser *>();
        const bool safeHtml = unsafeText && unsafeText->document()->toPlainText().contains(QStringLiteral("<b>"));
        const bool stateOk = markdown && prose && code && copyCode && retry && online && !thinking &&
                             prose->document()->toPlainText().contains(QStringLiteral("Vivi")) &&
                             code->toPlainText().contains(QStringLiteral("#include <iostream>")) &&
                             QGuiApplication::clipboard()->text().contains(QStringLiteral("int main()")) &&
                             safeHtml && users && users->count() == 1 &&
                             badge && badge->text() == QStringLiteral("1 人在线") &&
                             window.findChildren<QFrame *>(QStringLiteral("viviCard")).size() == 1;
        const bool saved = window.grab().save(QString::fromLocal8Bit(argv[1]));
        if (retry) retry->click();
        QTimer::singleShot(100, &app, [&, stateOk, saved]() {
            app.exit(!stateOk ? 5 : !saved ? 4 : !gotRegenerate ? 6 : 0);
        });
    });
    return app.exec();
}
