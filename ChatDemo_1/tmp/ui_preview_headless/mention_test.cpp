#include "mainwindow.h"
#include "mentioninput.h"
#include "aimarkdownwidget.h"

#include <QApplication>
#include <QClipboard>
#include <QEventLoop>
#include <QFontDatabase>
#include <QInputMethodEvent>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QLayout>
#include <QScrollArea>
#include <QTextBrowser>
#include <QTextDocument>
#include <QtMath>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>
#include <QToolButton>

static void waitEvents(int ms = 80)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
    QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyhbd.ttc"));
    QTcpServer mock;
    if (!mock.listen(QHostAddress::LocalHost, 0)) return 1;
    QList<QJsonObject> sent;
    QStringList aiCommands;
    QTcpSocket *peer = nullptr;
    QObject::connect(&mock, &QTcpServer::newConnection, &app, [&]() {
        peer = mock.nextPendingConnection();
        auto *buffer = new QByteArray;
        QObject::connect(peer, &QObject::destroyed, &app, [buffer]() { delete buffer; });
        QObject::connect(peer, &QTcpSocket::readyRead, peer, [&, buffer]() {
            buffer->append(peer->readAll());
            int end = -1;
            while ((end = buffer->indexOf('\n')) >= 0) {
                const QByteArray line = buffer->left(end);
                buffer->remove(0, end + 1);
                if (line.startsWith("JOIN|")) {
                    QJsonArray users;
                    for (const auto &pair : {qMakePair("1", "A"), qMakePair("2", "张三"),
                                             qMakePair("3", "张三"), qMakePair("4", "李四")})
                        users.append(QJsonObject{{"id", pair.first}, {"name", QString::fromUtf8(pair.second)}});
                    peer->write("USERLIST|A,张三,张三,李四\n");
                    peer->write("USERLIST2|" + QJsonDocument(users).toJson(QJsonDocument::Compact).toBase64() + "\n");
                    peer->write("AI|status|0|b25saW5l\n");
                } else if (line.startsWith("CHAT2|")) {
                    sent.append(QJsonDocument::fromJson(QByteArray::fromBase64(line.mid(6))).object());
                } else if (line.startsWith("AI_REGENERATE|")) {
                    aiCommands.append(QString::fromUtf8(line));
                }
            }
        });
    });
    MainWindow window(QStringLiteral("A"), QStringLiteral("127.0.0.1:%1").arg(mock.serverPort()),
                      nullptr, 1, QStringLiteral("test-session"));
    window.resize(900, 620);
    window.show();
    waitEvents(160);
    if (argc > 4) window.grab().save(QString::fromLocal8Bit(argv[4]));
    auto *input = window.findChild<MentionInput *>(QStringLiteral("messageInput"));
    auto *popup = window.findChild<QFrame *>(QStringLiteral("mentionPopup"));
    auto *list = window.findChild<QListWidget *>(QStringLiteral("mentionList"));
    if (!input || !popup || !list) return 2;
    auto *composer = window.findChild<QFrame *>(QStringLiteral("composer"));
    auto *scroll = window.findChild<QScrollArea *>(QStringLiteral("messageScroll"));
    qInfo() << "GEOMETRY" << "composer" << composer->geometry() << "scroll" << scroll->geometry()
            << "input" << input->geometry() << "placeholder" << input->placeholderText()
            << "composer-policy" << composer->sizePolicy().verticalPolicy()
            << "scroll-policy" << scroll->sizePolicy().verticalPolicy();
    if (composer->height() != 76 || input->height() != 50 || scroll->height() < 350 ||
        input->placeholderText() != QStringLiteral("请输入消息...")) return 20;
    const int initialScrollHeight = scroll->height();
    const int initialComposerTop = composer->y();

    QTest::keyClicks(input, "@");
    waitEvents();
    if (!popup->isVisible() || list->count() != 4) return 3; // Vivi + three others, not self.
    if (scroll->height() != initialScrollHeight || composer->y() != initialComposerTop) return 21;
    if (argc > 1) window.grab().save(QString::fromLocal8Bit(argv[1]));
    QTest::keyClicks(input, "Vi");
    waitEvents();
    if (list->count() != 1) return 4;
    QTest::keyClick(input, Qt::Key_Return);
    if (input->toPlainText() != QStringLiteral("@Vivi ") || popup->isVisible() || !sent.isEmpty()) return 5;
    QTest::keyClicks(input, "hello");
    QTest::keyClick(input, Qt::Key_Return);
    waitEvents();
    if (sent.size() != 1 || sent.at(0).value("text").toString() != QStringLiteral("@Vivi hello") ||
        sent.at(0).value("mentions").toArray().size() != 1 ||
        sent.at(0).value("mentions").toArray().at(0).toObject().value("id").toString() != QStringLiteral("vivi")) return 6;
    if (!input->toPlainText().isEmpty()) return 7;
    qInfo() << "EMPTY_INPUT" << input->document()->isEmpty() << input->toPlainText().size()
            << input->palette().color(QPalette::PlaceholderText);
    if (argc > 3) window.grab().save(QString::fromLocal8Bit(argv[3]));

    QTest::keyClicks(input, "@");
    input->textCursor().insertText(QStringLiteral("张"));
    waitEvents();
    if (list->count() != 2) return 8;
    QTest::keyClick(input, Qt::Key_Down);
    QTest::keyClick(input, Qt::Key_Tab);
    if (input->toPlainText() != QStringLiteral("@张三 ")) return 9;
    QTest::keyClicks(input, "@");
    input->textCursor().insertText(QStringLiteral("李"));
    waitEvents();
    if (list->count() != 1) return 10;
    QTest::keyClick(input, Qt::Key_Return);
    QTest::keyClicks(input, "hi");
    if (argc > 2) window.grab().save(QString::fromLocal8Bit(argv[2]));
    QMetaObject::invokeMethod(&window, "on_sendButton_clicked", Qt::DirectConnection);
    waitEvents();
    if (sent.size() != 2) return 11;
    const QJsonArray mentions = sent.at(1).value("mentions").toArray();
    if (mentions.size() != 2 || mentions.at(0).toObject().value("id").toString() != "3" ||
        mentions.at(1).toObject().value("id").toString() != "4" ||
        sent.at(1).value("text").toString() != QStringLiteral("@张三 @李四 hi")) return 12;

    QTest::keyClicks(input, "@Vi");
    waitEvents();
    QTest::keyClick(input, Qt::Key_Escape);
    if (popup->isVisible()) return 13;
    QTest::keyClick(input, Qt::Key_Backspace);
    QTest::keyClick(input, Qt::Key_Backspace);
    QTest::keyClick(input, Qt::Key_Backspace);
    if (popup->isVisible()) return 14;
    QInputMethodEvent preedit(QStringLiteral("@"), {});
    QApplication::sendEvent(input, &preedit);
    if (popup->isVisible()) return 15;
    QInputMethodEvent finish(QString(), {});
    QApplication::sendEvent(input, &finish);
    QTest::keyClicks(input, "@Vi");
    waitEvents();
    if (!popup->isVisible() || list->count() != 1) return 16;
    window.resize(830, 560);
    waitEvents();
    if (composer->height() != 76 || scroll->height() < 280) return 22;
    const QRect chat = window.findChild<QFrame *>(QStringLiteral("chatPanel"))->geometry();
    if (!chat.contains(popup->geometry())) return 17;
    const QRect first = list->visualItemRect(list->item(0));
    QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, first.center());
    if (popup->isVisible() || input->toPlainText() != QStringLiteral("@Vivi ")) return 18;
    input->clear();
    QJsonArray onlySelf{QJsonObject{{"id", "1"}, {"name", "A"}}};
    peer->write("USERLIST|A\nUSERLIST2|" + QJsonDocument(onlySelf).toJson(QJsonDocument::Compact).toBase64() + "\n");
    waitEvents();
    QTest::keyClicks(input, "@");
    waitEvents();
    if (list->count() != 1) return 19; // Only Vivi remains, not the local user.
    window.resize(1400, 850);
    waitEvents();
    if (composer->height() != 76 || scroll->height() < 500) return 23;
    auto reply = [&](int id, int accountId, const QString &name, const QString &body) {
        const QJsonObject meta{{"accountId", QString::number(accountId)}, {"nickname", name}};
        const QByteArray encodedMeta = QJsonDocument(meta).toJson(QJsonDocument::Compact).toBase64();
        peer->write("AI|meta|" + QByteArray::number(id) + "|" + encodedMeta + "\n");
        peer->write("AI|reply|" + QByteArray::number(id) + "|" + body.toUtf8().toBase64() + "\n");
        waitEvents();
    };
    reply(101, 3, QStringLiteral("张三"), QStringLiteral("你好"));
    reply(102, 4, QStringLiteral("李四"), QStringLiteral("你好！\n这是第二行。"));
    if (argc > 6) window.grab().save(QString::fromLocal8Bit(argv[6]));
    reply(103, 3, QStringLiteral("张三"), QString(180, QChar(0x4e2d)));
    reply(104, 4, QStringLiteral("李四"), QStringLiteral("- 第一项\n- 第二项\n- 第三项"));
    reply(105, 3, QStringLiteral("张三"), QStringLiteral("```cpp\n#include <iostream>\nint main() {\n    std::cout << \"hello\";\n    return 0;\n}\n```"));
    const auto aiBodies = window.findChildren<AiMarkdownWidget *>();
    const auto labels = window.findChildren<QLabel *>(QStringLiteral("aiReplyLabel"));
    if (aiBodies.size() != 5 || labels.size() != 5 ||
        labels.at(0)->text() != QStringLiteral("回复 @张三") ||
        labels.at(1)->text() != QStringLiteral("回复 @李四")) return 24;
    if (aiBodies.at(0)->height() >= 75 || aiBodies.at(1)->height() >= 105 ||
        aiBodies.at(2)->height() <= aiBodies.at(0)->height() ||
        aiBodies.at(4)->height() <= aiBodies.at(1)->height()) return 25;
    for (AiMarkdownWidget *body : aiBodies) {
        if (body->height() != body->layout()->sizeHint().height()) return 26;
        for (QTextBrowser *browser : body->findChildren<QTextBrowser *>())
            if (browser->height() + 2 < qCeil(browser->document()->size().height())) return 27;
    }
    const auto buttons = window.findChildren<QToolButton *>();
    QToolButton *copy = nullptr, *retry = nullptr;
    QList<QToolButton *> ratings;
    for (QToolButton *button : buttons) {
        if (button->text() == QStringLiteral("复制全文") && !copy) copy = button;
        if (button->text() == QStringLiteral("重新生成") && !retry) retry = button;
        if (button->isCheckable()) ratings.append(button);
    }
    if (!copy || !retry || ratings.size() < 2) return 29;
    copy->click();
    if (QGuiApplication::clipboard()->text() != QStringLiteral("你好")) return 30;
    retry->click();
    waitEvents();
    if (aiCommands != QStringList{QStringLiteral("AI_REGENERATE|101")}) return 31;
    ratings.at(0)->click();
    ratings.at(1)->click();
    if (ratings.at(0)->isChecked() || !ratings.at(1)->isChecked()) return 32;
    const int wideHeight = aiBodies.at(2)->height();
    window.resize(830, 560);
    waitEvents();
    if (aiBodies.at(2)->height() < wideHeight) return 28;
    if (argc > 5) window.grab().save(QString::fromLocal8Bit(argv[5]));
    return 0;
}
