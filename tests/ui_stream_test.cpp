#include "mainwindow.h"
#include "aimarkdownwidget.h"
#include "roomsidebarwidget.h"
#include "roomcreatedialog.h"
#include "roominvitedialog.h"
#include "roommanagementdialog.h"
#include "roompermissionsdialog.h"
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QDir>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QListWidget>
#include <QLayout>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStackedWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QToolButton>
#include <QTextBrowser>
#include <functional>

static void waitEvents(int milliseconds = 80)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

static bool waitUntil(const std::function<bool()> &condition, int timeoutMs = 4000)
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (!condition() && elapsed.elapsed() < timeoutMs) waitEvents(25);
    return condition();
}

static QByteArray frame(const QJsonObject &event)
{
    return "AI|stream|" + event.value("request_id").toString().toUtf8() + "|" +
        QJsonDocument(event).toJson(QJsonDocument::Compact).toBase64() + "\n";
}

static QJsonObject event(const char *kind, int request, const char *message, int user,
                         const QString &name, int sequence, const QString &text = {})
{
    QJsonObject object{{"event", QString::fromLatin1(kind)},
        {"request_id", QString::number(request)}, {"message_id", QString::fromLatin1(message)},
        {"generation_id", QStringLiteral("generation-") + QString::number(request)},
        {"reply_to_message_id", QStringLiteral("question-") + QString::number(request)},
        {"user_id", QString::number(user)}, {"nickname", name}, {"sequence", sequence},
        {"room_id", QStringLiteral("class-room")}};
    if (QString::fromLatin1(kind) == QStringLiteral("ai_chunk")) object.insert("text", text);
    else if (QString::fromLatin1(kind) == QStringLiteral("ai_complete") ||
             QString::fromLatin1(kind) == QStringLiteral("ai_start")) object.insert("full_text", text);
    else object.insert("error", text);
    return object;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       QDir::tempPath() + QStringLiteral("/VChatUiTest"));
    QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
    if (qEnvironmentVariableIsSet("VCHAT_ROOM_DIALOG_SMOKE")) {
        auto *dialog = new RoomManagementDialog(QStringLiteral("room-smoke"), 11);
        const QString smokeDescription = qEnvironmentVariableIsSet("VCHAT_ROOM_DIALOG_LONG")
            ? QStringLiteral("这是一个用于检查长简介换行和窗口高度的聊天室。").repeated(7)
            : QStringLiteral("C++ 开发与项目协作");
        const QString smokeName = qEnvironmentVariableIsSet("VCHAT_ROOM_DIALOG_LONG")
            ? QStringLiteral("跨平台即时通讯与人工智能协作开发项目组")
            : QStringLiteral("C++ 项目组");
        dialog->setRoom(QJsonObject{{"room_id", "room-smoke"}, {"owner_id", "11"},
            {"room_name", smokeName},
            {"description", smokeDescription},
            {"room_type", "invite_only"}, {"owner_name", QStringLiteral("甲")},
            {"member_count", 2}, {"created_at", "2026-11-09T03:37:00.000"}});
        dialog->setMembers(QJsonArray{
            QJsonObject{{"user_id", "11"}, {"nickname", QStringLiteral("徐一豪")},
                {"role", "owner"}, {"online", true}},
            QJsonObject{{"user_id", "22"}, {"nickname", QStringLiteral("伍锐")},
                {"role", "member"}, {"online", false}}});
        dialog->show();
        waitEvents(100);
        if (!dialog->windowFlags().testFlag(Qt::FramelessWindowHint)) return 201;
        for (QLabel *label : dialog->findChildren<QLabel *>()) {
            if (label->text() != smokeDescription) continue;
            const int needed = label->fontMetrics()
                .boundingRect(QRect(0, 0, label->width(), 10000), Qt::TextWordWrap,
                              smokeDescription).height();
            if (label->height() < needed) return 202;
        }
        const int detailsHeight = dialog->height();
        if (argc > 1) dialog->grab().save(QString::fromLocal8Bit(argv[1]));
        dialog->showPage(QStringLiteral("edit"));
        waitEvents(50);
        if (argc > 2) dialog->grab().save(QString::fromLocal8Bit(argv[2]));
        dialog->showPage(QStringLiteral("members"));
        waitEvents(50);
        if (dialog->height() >= detailsHeight || dialog->height() > 480) return 203;
        const auto cards = dialog->findChildren<QFrame *>(QStringLiteral("memberCard"));
        if (cards.size() != 2) return 206;
        auto columnX = [dialog](QWidget *widget) { return widget->mapTo(dialog, QPoint(0, 0)).x(); };
        for (const QString &column : {QStringLiteral("memberRole"),
                                      QStringLiteral("memberStatusDot"),
                                      QStringLiteral("memberActionSlot")}) {
            QWidget *first = cards[0]->findChild<QWidget *>(column);
            QWidget *second = cards[1]->findChild<QWidget *>(column);
            if (!first || !second || columnX(first) != columnX(second)) return 207;
        }
        if (cards[0]->findChild<QToolButton *>(QStringLiteral("memberMore"))) return 208;
        auto *memberMore = cards[1]->findChild<QToolButton *>(QStringLiteral("memberMore"));
        if (!memberMore || memberMore->size() != QSize(32, 32) || memberMore->icon().isNull()) return 209;
        memberMore->click();
        waitEvents(20);
        QMenu *memberMenu = memberMore->findChild<QMenu *>();
        if (!memberMenu || memberMenu->actions().isEmpty() ||
            memberMenu->actions().first()->text() != QStringLiteral("移除成员")) return 210;
        memberMenu->close();
        if (argc > 3) dialog->grab().save(QString::fromLocal8Bit(argv[3]));
        QJsonArray crowdedMembers;
        for (int i = 0; i < 8; ++i)
            crowdedMembers.append(QJsonObject{{"user_id", QString::number(11 + i)},
                {"nickname", i == 2 ? QStringLiteral("非常非常长的成员昵称用于检查省略号")
                                     : QStringLiteral("成员%1").arg(i + 1)},
                {"role", i == 0 ? "owner" : "member"}, {"online", i % 2 == 0}});
        dialog->setMembers(crowdedMembers);
        waitEvents(50);
        if (dialog->findChild<QScrollArea *>()->verticalScrollBar()->maximum() <= 0) return 205;
        const auto manyCards = dialog->findChildren<QFrame *>(QStringLiteral("memberCard"));
        if (manyCards.size() != 8) return 211;
        for (const QString &column : {QStringLiteral("memberRole"),
                                      QStringLiteral("memberStatusDot"),
                                      QStringLiteral("memberActionSlot")}) {
            const int expectedX = manyCards.first()->findChild<QWidget *>(column)->geometry().x();
            for (QFrame *card : manyCards)
                if (card->findChild<QWidget *>(column)->geometry().x() != expectedX) return 212;
        }
        bool elided = false;
        for (QFrame *card : manyCards)
            if (card->findChild<QLabel *>(QStringLiteral("memberName"))->text().endsWith(QChar(0x2026)))
                elided = true;
        if (!elided) return 213;
        delete dialog;
        auto *create = new RoomCreateDialog;
        create->show();
        waitEvents(50);
        if (!create->windowFlags().testFlag(Qt::FramelessWindowHint)) return 204;
        if (argc > 4) create->grab().save(QString::fromLocal8Bit(argv[4]));
        delete create;
        return 0;
    }
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) return 1;
    QList<QTcpSocket *> peers;
    QStringList clientCommands;
    QStringList roomCommands;
    QObject::connect(&server, &QTcpServer::newConnection, &app, [&]() {
        QTcpSocket *peer = server.nextPendingConnection();
        peers.append(peer);
        auto *buffer = new QByteArray;
        QObject::connect(peer, &QObject::destroyed, &app, [buffer]() { delete buffer; });
        QObject::connect(peer, &QTcpSocket::readyRead, peer, [peer, buffer, &clientCommands, &roomCommands]() {
            buffer->append(peer->readAll());
            int end = -1;
            while ((end = buffer->indexOf('\n')) >= 0) {
                const QByteArray line = buffer->left(end);
                buffer->remove(0, end + 1);
                if (line.startsWith("AI_CANCEL|") || line.startsWith("AI_REGENERATE|"))
                    clientCommands.append(QString::fromUtf8(line));
                if (line.startsWith("ROOM|")) roomCommands.append(QString::fromUtf8(line));
            }
        });
    });
    const QString address = QStringLiteral("127.0.0.1:%1").arg(server.serverPort());
    MainWindow a(QStringLiteral("甲"), address, nullptr, 11, QStringLiteral("test-a"));
    MainWindow b(QStringLiteral("乙"), address, nullptr, 22, QStringLiteral("test-b"));
    a.resize(900, 620);
    b.resize(900, 620);
    a.show(); b.show();
    waitEvents(180);
    if (peers.size() != 2) return 2;
    auto broadcast = [&](const QJsonObject &object) {
        const QByteArray bytes = frame(object);
        for (QTcpSocket *peer : peers) {
            peer->write(bytes.left(9));
        }
        waitEvents(10);
        for (QTcpSocket *peer : peers)
            peer->write(bytes.mid(9)); // TCP framing must survive separate readyRead calls.
        waitEvents();
    };
    broadcast(event("ai_start", 1, "reply-1", 11, QStringLiteral("甲"), 0));
    auto bodiesA = a.findChildren<AiMarkdownWidget *>();
    auto bodiesB = b.findChildren<AiMarkdownWidget *>();
    if (bodiesA.size() != 1 || bodiesB.size() != 1) return 3;
    AiMarkdownWidget *firstA = bodiesA.first();
    AiMarkdownWidget *firstB = bodiesB.first();
    broadcast(event("ai_chunk", 1, "reply-1", 11, QStringLiteral("甲"), 1, QStringLiteral("你好")));
    if (firstA->plainText() != QStringLiteral("你好") || firstB->plainText() != QStringLiteral("你好")) return 4;
    broadcast(event("ai_start", 2, "reply-2", 22, QStringLiteral("乙"), 0));
    broadcast(event("ai_chunk", 2, "reply-2", 22, QStringLiteral("乙"), 1, QStringLiteral("第二条")));
    broadcast(event("ai_chunk", 1, "reply-1", 11, QStringLiteral("甲"), 2, QStringLiteral("，世界")));
    broadcast(event("ai_chunk", 1, "reply-1", 11, QStringLiteral("甲"), 2, QStringLiteral("，世界")));
    if (firstA->plainText() != QStringLiteral("你好，世界") || firstB->plainText() != QStringLiteral("你好，世界") ||
        a.findChildren<AiMarkdownWidget *>().size() != 2) return 5;
    const QString markdown = QStringLiteral("# 你好\n```cpp\nint x = 1;\n```\n完成。");
    broadcast(event("ai_complete", 1, "reply-1", 11, QStringLiteral("甲"), 2, markdown));
    if (!waitUntil([&]() { return !firstA->findChildren<QPlainTextEdit *>().isEmpty() &&
                                !firstB->findChildren<QPlainTextEdit *>().isEmpty(); })) return 6;
    for (QTcpSocket *peer : peers)
        peer->write("AI|reply|1|" + markdown.toUtf8().toBase64() + "\n");
    waitEvents();
    // Nested final Markdown widgets are expected; count top-level AI message rows instead.
    for (MainWindow *window : {&a, &b}) {
        int rows = 0;
        for (QWidget *widget : window->findChildren<QWidget *>())
            if (widget->property("aiMessageId").toString() == QStringLiteral("reply-1")) ++rows;
        if (rows != 1) return 7;
    }
    broadcast(event("ai_complete", 2, "reply-2", 22, QStringLiteral("乙"), 1, QStringLiteral("第二条完成")));
    AiMarkdownWidget *secondA = nullptr;
    QWidget *secondRow = nullptr;
    for (QWidget *widget : a.findChildren<QWidget *>())
        if (widget->property("aiMessageId").toString() == QStringLiteral("reply-2")) {
            secondRow = widget;
            secondA = widget->findChild<AiMarkdownWidget *>();
        }
    if (!secondA || !waitUntil([&]() { return secondA->plainText() == QStringLiteral("第二条完成") &&
                                  secondA->findChild<QTextBrowser *>() != nullptr; })) return 20;
    QTextBrowser *secondBrowser = secondA->findChild<QTextBrowser *>();
    if (!secondBrowser->isVisible() || secondBrowser->height() < 16 ||
        secondA->height() < secondBrowser->height()) return 21;
    QToolButton *copyFull = nullptr;
    for (QToolButton *button : secondRow->findChildren<QToolButton *>())
        if (button->text() == QStringLiteral("复制全文")) copyFull = button;
    if (!copyFull || !copyFull->isEnabled()) return 22;
    copyFull->click();
    if (QGuiApplication::clipboard()->text() != QStringLiteral("第二条完成")) return 23;
    for (MainWindow *window : {&a, &b}) {
        const auto labels = window->findChildren<QLabel *>(QStringLiteral("aiReplyLabel"));
        if (labels.size() != 2 || labels.at(0)->text() != QStringLiteral("回复 @甲") ||
            labels.at(1)->text() != QStringLiteral("回复 @乙")) return 8;
    }
    MainWindow rejoined(QStringLiteral("新成员"), address, nullptr, 33, QStringLiteral("test-c"));
    rejoined.resize(900, 620);
    rejoined.show();
    waitEvents(120);
    if (peers.size() != 3) return 13;
    peers.last()->write(frame(event("ai_complete", 1, "reply-1", 11, QStringLiteral("甲"), 2, markdown)));
    waitEvents();
    int restoredRows = 0;
    for (QWidget *widget : rejoined.findChildren<QWidget *>())
        if (widget->property("aiMessageId").toString() == QStringLiteral("reply-1")) ++restoredRows;
    if (restoredRows != 1 || rejoined.findChildren<QPlainTextEdit *>().isEmpty()) return 14;
    if (argc > 1) a.grab().save(QString::fromLocal8Bit(argv[1]));
    a.resize(760, 520);
    waitEvents();
    if (firstA->height() <= 0 || firstA->height() != firstA->layout()->sizeHint().height()) return 9;
    // A reading history must stay put while B at the bottom follows new chunks.
    const QJsonObject chat{{"sender", QStringLiteral("丙")}, {"text", QStringLiteral("测试消息")},
                           {"time", QStringLiteral("12:00:00")}};
    const QByteArray chatLine = "CHAT2|" + QJsonDocument(chat).toJson(QJsonDocument::Compact).toBase64() + "\n";
    for (int i = 0; i < 35; ++i)
        for (QTcpSocket *peer : peers) peer->write(chatLine);
    waitEvents(160);
    auto *scrollA = a.findChild<QScrollArea *>(QStringLiteral("messageScroll"))->verticalScrollBar();
    auto *scrollB = b.findChild<QScrollArea *>(QStringLiteral("messageScroll"))->verticalScrollBar();
    if (scrollA->maximum() <= 0 || scrollB->maximum() <= 0) return 10;
    scrollA->setValue(0);
    scrollB->setValue(scrollB->maximum());
    broadcast(event("ai_start", 3, "reply-3", 11, QStringLiteral("甲"), 0));
    QToolButton *cancel = nullptr;
    for (QToolButton *button : a.findChildren<QToolButton *>())
        if (button->text() == QStringLiteral("取消生成") && button->isVisible()) cancel = button;
    if (!cancel) return 15;
    cancel->click();
    waitEvents();
    if (!clientCommands.contains(QStringLiteral("AI_CANCEL|3"))) return 16;
    broadcast(event("ai_chunk", 3, "reply-3", 11, QStringLiteral("甲"), 1, QString(180, QChar(0x4e2d))));
    waitEvents(160);
    AiMarkdownWidget *thirdA = nullptr;
    for (QWidget *widget : a.findChildren<QWidget *>())
        if (widget->property("aiMessageId").toString() == QStringLiteral("reply-3"))
            thirdA = widget->findChild<AiMarkdownWidget *>();
    if (!thirdA || thirdA->plainText().isEmpty() || thirdA->plainText().size() >= 180) return 17;
    if (scrollA->value() != 0 || scrollB->value() < scrollB->maximum() - 24) return 11;
    broadcast(event("ai_error", 3, "reply-3", 11, QStringLiteral("甲"), 1,
                    QStringLiteral("AI 回复中断，请稍后重试。")));
    int errorRows = 0;
    for (QWidget *widget : a.findChildren<QWidget *>())
        if (widget->property("aiMessageId").toString() == QStringLiteral("reply-3")) ++errorRows;
    if (errorRows != 1) return 12;
    if (!waitUntil([&]() { return thirdA->plainText().contains(QStringLiteral("AI 回复中断")); })) return 18;
    const QString unicode = QStringLiteral("中\U0001F600e\u0301文");
    broadcast(event("ai_start", 4, "reply-4", 22, QStringLiteral("乙"), 0));
    broadcast(event("ai_chunk", 4, "reply-4", 22, QStringLiteral("乙"), 1, unicode));
    broadcast(event("ai_complete", 4, "reply-4", 22, QStringLiteral("乙"), 1, unicode));
    AiMarkdownWidget *unicodeBody = nullptr;
    for (QWidget *widget : a.findChildren<QWidget *>())
        if (widget->property("aiMessageId").toString() == QStringLiteral("reply-4"))
            unicodeBody = widget->findChild<AiMarkdownWidget *>();
    if (!unicodeBody || !waitUntil([&]() { return unicodeBody->plainText() == unicode; })) return 19;
    const QString longAnswer(1101, QChar(0x4e2d));
    scrollA->setValue(0);
    broadcast(event("ai_start", 5, "reply-5", 11, QStringLiteral("甲"), 0));
    broadcast(event("ai_chunk", 5, "reply-5", 11, QStringLiteral("甲"), 1, longAnswer));
    broadcast(event("ai_complete", 5, "reply-5", 11, QStringLiteral("甲"), 1, longAnswer));
    AiMarkdownWidget *longBody = nullptr;
    for (QWidget *widget : a.findChildren<QWidget *>())
        if (widget->property("aiMessageId").toString() == QStringLiteral("reply-5"))
            longBody = widget->findChild<AiMarkdownWidget *>();
    if (!longBody || longBody->plainText().size() >= longAnswer.size() || scrollA->value() != 0) return 24;
    if (!waitUntil([&]() { return longBody->findChild<QTextBrowser *>() &&
                                longBody->plainText() == longAnswer; }, 20000)) return 25;
    if (longBody->height() < longBody->findChild<QTextBrowser *>()->height() || scrollA->value() != 0) return 26;
    broadcast(event("ai_start", 6, "reply-6", 22, QStringLiteral("乙"), 0));
    broadcast(event("ai_chunk", 6, "reply-6", 22, QStringLiteral("乙"), 1, QStringLiteral("保留正文")));
    broadcast(event("ai_complete", 6, "reply-6", 22, QStringLiteral("乙"), 1));
    AiMarkdownWidget *fallbackBody = nullptr;
    for (QWidget *widget : a.findChildren<QWidget *>())
        if (widget->property("aiMessageId").toString() == QStringLiteral("reply-6"))
            fallbackBody = widget->findChild<AiMarkdownWidget *>();
    if (!fallbackBody || !waitUntil([&]() { return fallbackBody->findChild<QTextBrowser *>() &&
                                    fallbackBody->plainText() == QStringLiteral("保留正文"); })) return 27;
    QWidget *firstRow = nullptr;
    for (QWidget *widget : a.findChildren<QWidget *>())
        if (widget->property("aiMessageId").toString() == QStringLiteral("reply-1")) firstRow = widget;
    QToolButton *retry = nullptr;
    QToolButton *copy = nullptr;
    for (QToolButton *button : firstRow->findChildren<QToolButton *>()) {
        if (button->text() == QStringLiteral("重新生成")) retry = button;
        if (button->text() == QStringLiteral("复制全文")) copy = button;
    }
    if (!retry || !retry->isEnabled() || !copy) return 28;
    retry->click();
    retry->click(); // The second click must not send a second request.
    waitEvents();
    if (clientCommands.count(QStringLiteral("AI_REGENERATE|1")) != 1) return 29;
    auto regeneration = [&](const char *kind, int request, int sequence, const QString &text,
                            const QString &previous = {}) {
        QJsonObject item = event(kind, request, "reply-1", 11, QStringLiteral("甲"), sequence, text);
        item.insert("original_request_id", QStringLiteral("1"));
        item.insert("reply_to_message_id", QStringLiteral("question-1"));
        item.insert("regeneration", true);
        if (!previous.isEmpty()) item.insert("previous_text", previous);
        return item;
    };
    const QString revised = QStringLiteral("# 新回答\n```cpp\nint revised = 1;\n```");
    broadcast(regeneration("ai_start", 7, 0, {}, markdown));
    broadcast(regeneration("ai_chunk", 7, 1, revised));
    broadcast(regeneration("ai_complete", 7, 1, revised));
    if (!waitUntil([&]() { return firstA->plainText().contains(QStringLiteral("int revised = 1;")) &&
                                firstB->plainText().contains(QStringLiteral("int revised = 1;")) &&
                                firstA->findChild<QPlainTextEdit *>() != nullptr; })) return 30;
    for (MainWindow *window : {&a, &b, &rejoined}) {
        int rows = 0;
        for (QWidget *widget : window->findChildren<QWidget *>())
            if (widget->property("aiMessageId").toString() == QStringLiteral("reply-1")) ++rows;
        if (rows != 1) return 31;
    }
    copy->click();
    if (!QGuiApplication::clipboard()->text().contains(QStringLiteral("int revised = 1;"))) return 32;
    const QString secondRevision = QStringLiteral("第二次更新");
    broadcast(regeneration("ai_start", 8, 0, {}, revised));
    broadcast(regeneration("ai_chunk", 8, 1, secondRevision));
    broadcast(regeneration("ai_complete", 8, 1, secondRevision));
    if (!waitUntil([&]() { return firstA->plainText() == secondRevision &&
                                firstB->plainText() == secondRevision; })) return 33;
    broadcast(regeneration("ai_start", 9, 0, {}, secondRevision));
    broadcast(regeneration("ai_chunk", 9, 1, QStringLiteral("无效半截")));
    broadcast(regeneration("ai_error", 9, 1, QStringLiteral("AI 服务暂时不可用"), secondRevision));
    if (!waitUntil([&]() { return firstA->plainText() == secondRevision &&
                                firstB->plainText() == secondRevision; })) return 34;
    broadcast(regeneration("ai_chunk", 7, 2, QStringLiteral("过期增量")));
    waitEvents();
    if (firstA->plainText() != secondRevision || firstB->plainText() != secondRevision) return 35;
    copy->click();
    if (QGuiApplication::clipboard()->text() != secondRevision) return 36;
    MainWindow latest(QStringLiteral("后来者"), address, nullptr, 44, QStringLiteral("test-d"));
    latest.show();
    waitEvents(120);
    if (peers.size() != 4) return 37;
    peers.last()->write(frame(regeneration("ai_complete", 8, 1, secondRevision)));
    AiMarkdownWidget *latestBody = nullptr;
    if (!waitUntil([&]() {
        for (QWidget *widget : latest.findChildren<QWidget *>())
            if (widget->property("aiMessageId").toString() == QStringLiteral("reply-1"))
                latestBody = widget->findChild<AiMarkdownWidget *>();
        return latestBody && latestBody->plainText() == secondRevision;
    })) return 38;
    const QJsonObject question{{"room_id", "class-room"}, {"message_id", "saved-question"}, {"sender_id", "11"},
        {"sender", QStringLiteral("甲")}, {"type", "user"},
        {"text", QStringLiteral("@Vivi 请给 C++ 代码")},
        {"time", "2026-11-07T12:01:02.000"}};
    const QJsonObject answer{{"room_id", "class-room"}, {"message_id", "saved-answer"}, {"sender_id", "11"},
        {"sender", "Vivi"}, {"type", "ai"},
        {"text", QStringLiteral("```cpp\nint value = 42;\n```")},
        {"reply_to_message_id", "saved-question"}, {"reply_to_name", QStringLiteral("甲")}};
    const auto historyFrame = [](const QJsonObject &item) {
        return "HISTORY|" + QJsonDocument(item).toJson(QJsonDocument::Compact).toBase64() + "\n";
    };
    for (int i = 0; i < 2; ++i) {
        peers.last()->write(historyFrame(question));
        peers.last()->write(historyFrame(answer));
    }
    if (!waitUntil([&]() {
        int questions = 0, answers = 0;
        for (QWidget *widget : latest.findChildren<QWidget *>()) {
            if (widget->property("userMessageId") == QStringLiteral("saved-question")) ++questions;
            if (widget->property("aiMessageId") == QStringLiteral("saved-answer")) ++answers;
        }
        return questions == 1 && answers == 1;
    })) return 39;
    AiMarkdownWidget *savedBody = nullptr;
    for (QWidget *widget : latest.findChildren<QWidget *>())
        if (widget->property("aiMessageId") == QStringLiteral("saved-answer"))
            savedBody = widget->findChild<AiMarkdownWidget *>();
    if (!savedBody || !waitUntil([&]() { return savedBody->plainText().contains("int value = 42;") &&
                                            savedBody->findChild<QPlainTextEdit *>() != nullptr; })) return 40;
    // A reconnect during generation receives the persisted pending row first,
    // then AiRoom's live snapshot. Both must update the same bubble.
    MainWindow resumed(QStringLiteral("甲"), address, nullptr, 11, QStringLiteral("test-e"));
    resumed.show();
    waitEvents(120);
    if (peers.size() != 5) return 41;
    QJsonObject pendingQuestion = question;
    pendingQuestion.insert(QStringLiteral("message_id"), QStringLiteral("pending-question"));
    QJsonObject pendingAnswer = answer;
    pendingAnswer.insert(QStringLiteral("message_id"), QStringLiteral("pending-answer"));
    pendingAnswer.insert(QStringLiteral("reply_to_message_id"), QStringLiteral("pending-question"));
    pendingAnswer.insert(QStringLiteral("text"), QStringLiteral(""));
    pendingAnswer.insert(QStringLiteral("ai_state"), QStringLiteral("pending"));
    peers.last()->write(historyFrame(pendingQuestion));
    peers.last()->write(historyFrame(pendingAnswer));
    AiMarkdownWidget *pendingBody = nullptr;
    if (!waitUntil([&]() {
        for (QWidget *widget : resumed.findChildren<QWidget *>())
            if (widget->property("aiMessageId") == QStringLiteral("pending-answer"))
                pendingBody = widget->findChild<AiMarkdownWidget *>();
        return pendingBody && pendingBody->plainText().contains(QStringLiteral("正在思考"));
    })) return 42;
    QJsonObject liveStart = event("ai_start", 9, "pending-answer", 11, QStringLiteral("甲"), 0);
    liveStart.insert(QStringLiteral("reply_to_message_id"), QStringLiteral("pending-question"));
    peers.last()->write(frame(liveStart));
    QJsonObject liveChunk = event("ai_chunk", 9, "pending-answer", 11, QStringLiteral("甲"), 1,
                                  QStringLiteral("恢复后继续生成"));
    peers.last()->write(frame(liveChunk));
    if (!waitUntil([&]() { return pendingBody->plainText() == QStringLiteral("恢复后继续生成"); })) return 43;
    QJsonObject liveComplete = event("ai_complete", 9, "pending-answer", 11, QStringLiteral("甲"), 1,
                                     QStringLiteral("恢复后继续生成"));
    peers.last()->write(frame(liveComplete));
    if (!waitUntil([&]() { return pendingBody->plainText() == QStringLiteral("恢复后继续生成") &&
        pendingBody->findChild<QTextBrowser *>() != nullptr; })) return 44;
    int pendingRows = 0;
    for (QWidget *widget : resumed.findChildren<QWidget *>())
        if (widget->property("aiMessageId") == QStringLiteral("pending-answer")) ++pendingRows;
    if (pendingRows != 1) return 45;

    // The navigation uses real protocol frames and never changes the visible
    // room before a successful switch response.
    const auto roomResult = [](const QJsonObject &item) {
        return "ROOM|result|" + QJsonDocument(item).toJson(QJsonDocument::Compact).toBase64() + "\n";
    };
    const QJsonArray rooms{
        QJsonObject{{"room_id", "class-room"}, {"room_name", QStringLiteral("VChat 大厅")}},
        QJsonObject{{"room_id", "room-two"}, {"room_name", QStringLiteral("任务讨论")},
                    {"description", QStringLiteral("隔离测试")}, {"owner_id", QStringLiteral("11")}}};
    peers.first()->write("HISTORY_END\n");
    if (!waitUntil([&]() { return roomCommands.join('\n').contains(QStringLiteral("ROOM|list_mine|")); })) return 46;
    peers.first()->write(roomResult(QJsonObject{{"action", "list_mine"}, {"ok", false},
        {"error", QStringLiteral("聊天室列表查询失败，请检查数据库结构和连接")}}));
    auto *sidebar = a.findChild<RoomSidebarWidget *>();
    if (!sidebar || !waitUntil([&]() {
        for (QLabel *label : sidebar->findChildren<QLabel *>())
            if (label->text().contains(QStringLiteral("聊天室列表查询失败"))) return true;
        return false;
    })) return 129;
    peers.first()->write(roomResult(QJsonObject{{"action", "list_mine"}, {"ok", true}, {"rooms", rooms}}));
    if (!sidebar || !waitUntil([&]() { return sidebar->findChild<QListWidget *>()->count() == 2; })) return 47;
    auto *roomList = sidebar->findChild<QListWidget *>();
    roomList->itemClicked(roomList->item(1));
    if (!waitUntil([&]() { return roomCommands.join('\n').contains(QStringLiteral("ROOM|switch|")); })) return 48;
    if (a.findChildren<AiMarkdownWidget *>().isEmpty()) return 49;
    peers.first()->write(roomResult(QJsonObject{{"action", "switch"}, {"ok", false},
        {"error", QStringLiteral("无权进入聊天室")}}));
    waitEvents();
    if (a.findChildren<AiMarkdownWidget *>().isEmpty() ||
        !roomList->item(0)->isSelected()) return 50;
    roomList->itemClicked(roomList->item(1));
    peers.first()->write(roomResult(QJsonObject{{"action", "switch"}, {"ok", true},
        {"room_id", "room-two"}}));
    QJsonObject secondRoomMessage{{"room_id", "room-two"}, {"message_id", "only-room-two"},
        {"sender_id", "22"}, {"sender", QStringLiteral("乙")}, {"type", "user"},
        {"text", QStringLiteral("仅任务讨论可见")}, {"time", "2026-11-07T12:01:02.000"}};
    peers.first()->write(historyFrame(secondRoomMessage));
    peers.first()->write("HISTORY_END\n");
    const QJsonArray online{QJsonObject{{"id", "11"}, {"name", QStringLiteral("甲")}}};
    peers.first()->write("USERLIST2|" + QJsonDocument(online).toJson(QJsonDocument::Compact).toBase64() + "\n");
    const QJsonArray roomMembers{
        QJsonObject{{"user_id", "11"}, {"nickname", QStringLiteral("甲")}, {"role", "owner"}},
        QJsonObject{{"user_id", "22"}, {"nickname", QStringLiteral("乙")}, {"role", "member"}}};
    peers.first()->write(roomResult(QJsonObject{{"action", "members"}, {"ok", true},
        {"room_id", "room-two"}, {"members", roomMembers}}));
    if (!waitUntil([&]() {
        int newRows = 0, oldRows = 0;
        for (QWidget *widget : a.findChildren<QWidget *>()) {
            if (widget->property("userMessageId") == QStringLiteral("only-room-two")) ++newRows;
            if (widget->property("aiMessageId") == QStringLiteral("reply-1")) ++oldRows;
        }
        return newRows == 1 && oldRows == 0 && roomList->item(1)->isSelected();
    })) return 51;
    if (a.findChild<QListWidget *>(QStringLiteral("userList"))->count() != 2 ||
        !a.findChild<QLabel *>(QStringLiteral("onlineBadge"))->text().contains(QStringLiteral("1 人在线"))) return 52;
    auto *ownerInvite = a.findChild<QPushButton *>(QStringLiteral("inviteMembersButton"));
    if (!ownerInvite || !ownerInvite->isVisible() || !ownerInvite->isEnabled() ||
        a.findChild<QLabel *>(QStringLiteral("subtitleLabel"))->text() != QStringLiteral("轻松连接每一次交流") ||
        a.findChild<QLabel *>(QStringLiteral("roomNameLabel"))->text() != QStringLiteral("任务讨论")) return 107;
    auto *roomMore = a.findChild<QToolButton *>(QStringLiteral("roomMoreButton"));
    if (!roomMore || !roomMore->isVisible() || !roomMore->isEnabled() ||
        roomMore->text() != QStringLiteral("···")) return 108;
    peers.first()->write(frame(event("ai_start", 80, "old-room-event", 11, QStringLiteral("甲"), 0)));
    waitEvents();
    for (QWidget *widget : a.findChildren<QWidget *>())
        if (widget->property("aiMessageId") == QStringLiteral("old-room-event")) return 53;
    sidebar->setCollapsed(true);
    if (sidebar->width() > 70 || !roomList->item(1)->isSelected()) return 54;
    sidebar->setCollapsed(false);
    if (sidebar->width() < 210) return 55;
    auto *toggle = sidebar->findChildren<QToolButton *>().first();
    toggle->click();
    QSettings settings(QStringLiteral("VChat"), QStringLiteral("VChatClient"));
    settings.sync();
    if (!sidebar->isCollapsed() || !settings.value(QStringLiteral("rooms/sidebarCollapsed")).toBool()) return 66;
    toggle->click();
    settings.sync();
    if (sidebar->isCollapsed() || settings.value(QStringLiteral("rooms/sidebarCollapsed")).toBool()) return 67;
    auto switchCount = [&]() {
        int count = 0;
        for (const QString &command : roomCommands)
            if (command.startsWith(QStringLiteral("ROOM|switch|"))) ++count;
        return count;
    };
    const int beforeRapid = switchCount();
    roomList->itemClicked(roomList->item(0));
    roomList->itemClicked(roomList->item(1));
    waitEvents();
    if (switchCount() != beforeRapid + 1) return 56; // Second click is queued.
    peers.first()->write(roomResult(QJsonObject{{"action", "switch"}, {"ok", true},
        {"room_id", "class-room"}}));
    QJsonObject lobbyMessage = secondRoomMessage;
    lobbyMessage.insert(QStringLiteral("room_id"), QStringLiteral("class-room"));
    lobbyMessage.insert(QStringLiteral("message_id"), QStringLiteral("lobby-rapid"));
    peers.first()->write(historyFrame(lobbyMessage));
    peers.first()->write("HISTORY_END\n");
    if (!waitUntil([&]() { return switchCount() == beforeRapid + 2; })) return 57;
    peers.first()->write(roomResult(QJsonObject{{"action", "switch"}, {"ok", true},
        {"room_id", "room-two"}}));
    peers.first()->write(historyFrame(secondRoomMessage));
    peers.first()->write("HISTORY_END\n");
    if (!waitUntil([&]() {
        int roomTwo = 0, lobby = 0;
        for (QWidget *widget : a.findChildren<QWidget *>()) {
            if (widget->property("userMessageId") == QStringLiteral("only-room-two")) ++roomTwo;
            if (widget->property("userMessageId") == QStringLiteral("lobby-rapid")) ++lobby;
        }
        return roomTwo == 1 && lobby == 0 && roomList->item(1)->isSelected();
    })) return 58;
    MainWindow restored(QStringLiteral("甲"), address, nullptr, 11, QStringLiteral("test-restore"));
    restored.show();
    if (!waitUntil([&]() { return peers.size() == 6; })) return 59;
    peers.last()->write("HISTORY_END\n");
    if (!waitUntil([&]() {
        int count = 0;
        for (const QString &command : roomCommands)
            if (command.startsWith(QStringLiteral("ROOM|list_mine|"))) ++count;
        return count >= 2;
    })) return 60;
    peers.last()->write(roomResult(QJsonObject{{"action", "list_mine"}, {"ok", true}, {"rooms", rooms}}));
    if (!waitUntil([&]() { return switchCount() == beforeRapid + 3; })) return 61;
    peers.last()->write(roomResult(QJsonObject{{"action", "switch"}, {"ok", true},
        {"room_id", "room-two"}}));
    peers.last()->write(historyFrame(secondRoomMessage));
    peers.last()->write("HISTORY_END\n");
    if (!waitUntil([&]() {
        auto *list = restored.findChild<QListWidget *>(QStringLiteral("roomList"));
        return list && list->count() == 2 && list->item(1)->isSelected();
    })) return 62;
    auto *restoredInvite = restored.findChild<QPushButton *>(QStringLiteral("inviteMembersButton"));
    if (!restoredInvite || !restoredInvite->isVisible() || !restoredInvite->isEnabled()) return 109;
    // Switching away from an active Vivi reply must discard only this room's
    // widgets. Returning replays the pending history row plus one live snapshot.
    roomList->itemClicked(roomList->item(0));
    peers.first()->write(roomResult(QJsonObject{{"action", "switch"}, {"ok", true},
        {"room_id", "class-room"}}));
    QJsonObject pendingRoomAi = answer;
    pendingRoomAi.insert(QStringLiteral("message_id"), QStringLiteral("room-ai-pending"));
    pendingRoomAi.insert(QStringLiteral("ai_state"), QStringLiteral("pending"));
    pendingRoomAi.insert(QStringLiteral("text"), QString());
    peers.first()->write(historyFrame(pendingRoomAi));
    peers.first()->write("HISTORY_END\n");
    QJsonObject partial = event("ai_start", 90, "room-ai-pending", 11,
                                QStringLiteral("甲"), 1, QStringLiteral("已生成片段"));
    peers.first()->write(frame(partial));
    if (!waitUntil([&]() {
        for (QWidget *widget : a.findChildren<QWidget *>())
            if (widget->property("aiMessageId") == QStringLiteral("room-ai-pending")) {
                auto *body = widget->findChild<AiMarkdownWidget *>();
                return body && body->plainText() == QStringLiteral("已生成片段");
            }
        return false;
    })) return 63;
    roomList->itemClicked(roomList->item(1));
    peers.first()->write(roomResult(QJsonObject{{"action", "switch"}, {"ok", true},
        {"room_id", "room-two"}}));
    peers.first()->write(historyFrame(secondRoomMessage));
    peers.first()->write("HISTORY_END\n");
    peers.first()->write(frame(event("ai_chunk", 90, "room-ai-pending", 11,
                                   QStringLiteral("甲"), 2, QStringLiteral("旧房间增量"))));
    waitEvents();
    for (QWidget *widget : a.findChildren<QWidget *>())
        if (widget->property("aiMessageId") == QStringLiteral("room-ai-pending")) return 64;
    roomList->itemClicked(roomList->item(0));
    peers.first()->write(roomResult(QJsonObject{{"action", "switch"}, {"ok", true},
        {"room_id", "class-room"}}));
    peers.first()->write(historyFrame(pendingRoomAi));
    peers.first()->write("HISTORY_END\n");
    partial.insert(QStringLiteral("sequence"), 2);
    partial.insert(QStringLiteral("full_text"), QStringLiteral("已生成片段旧房间增量"));
    peers.first()->write(frame(partial));
    if (!waitUntil([&]() {
        int rows = 0;
        bool complete = false;
        for (QWidget *widget : a.findChildren<QWidget *>())
            if (widget->property("aiMessageId") == QStringLiteral("room-ai-pending")) {
                ++rows;
                auto *body = widget->findChild<AiMarkdownWidget *>();
                complete = body && body->plainText() == QStringLiteral("已生成片段旧房间增量");
            }
        return rows == 1 && complete;
    })) return 65;
    // Folding must resize the existing chat panel without replacing messages.
    auto *chatPanel = a.findChild<QFrame *>(QStringLiteral("chatPanel"));
    auto *rightPanel = a.findChild<QFrame *>(QStringLiteral("rightPanel"));
    auto *fold = a.findChild<QToolButton *>(QStringLiteral("memberCollapseButton"));
    auto *unfold = a.findChild<QToolButton *>(QStringLiteral("memberExpandButton"));
    if (!chatPanel || !rightPanel || !fold || !unfold) return 68;
    const int originalWidth = chatPanel->width();
    fold->click();
    waitEvents();
    settings.sync();
    if (rightPanel->isVisible() || !unfold->isVisible() || chatPanel->width() <= originalWidth ||
        !settings.value(QStringLiteral("members/collapsed/11")).toBool()) return 69;
    if (argc > 4) a.grab().save(QString::fromLocal8Bit(argv[4]));

    // The dialog validates locally; ROOM create is sent once per submission.
    auto *createButton = sidebar->findChild<QToolButton *>(QStringLiteral("createRoomButton"));
    if (!createButton || !createButton->isEnabled()) return 71;
    createButton->click();
    QPointer<RoomCreateDialog> dialog = a.findChild<RoomCreateDialog *>();
    if (!dialog || !dialog->isVisible()) return 72;
    if (argc > 3) dialog->grab().save(QString::fromLocal8Bit(argv[3]));
    auto *nameInput = dialog->findChild<QLineEdit *>(QStringLiteral("roomNameInput"));
    auto *descriptionInput = dialog->findChild<QPlainTextEdit *>(QStringLiteral("roomDescriptionInput"));
    auto *publicRadio = dialog->findChild<QRadioButton *>(QStringLiteral("publicRoomOption"));
    auto *submit = dialog->findChild<QPushButton *>(QStringLiteral("createRoomSubmit"));
    if (!nameInput || !descriptionInput || !publicRadio || !submit) return 73;
    const auto createCount = [&]() {
        int count = 0;
        for (const QString &command : roomCommands)
            if (command.startsWith(QStringLiteral("ROOM|create|"))) ++count;
        return count;
    };
    const int beforeCreate = createCount();
    submit->click();
    waitEvents();
    if (createCount() != beforeCreate) return 74;
    nameInput->setText(QStringLiteral("测试公开房间"));
    descriptionInput->setPlainText(QStringLiteral("客户端创建流程"));
    publicRadio->setChecked(true);
    submit->click();
    submit->click();
    if (!waitUntil([&]() { return createCount() == beforeCreate + 1; })) return 75;
    const QString command = roomCommands.last();
    const QStringList parts = command.split('|');
    if (parts.size() != 3) return 76;
    const QJsonObject submitted = QJsonDocument::fromJson(QByteArray::fromBase64(parts.at(2).toLatin1())).object();
    if (submitted.value(QStringLiteral("room_name")).toString() != QStringLiteral("测试公开房间") ||
        submitted.value(QStringLiteral("room_type")).toString() != QStringLiteral("public")) return 77;
    peers.first()->write(roomResult(QJsonObject{{"action", "create"}, {"ok", false},
        {"error", QStringLiteral("拒绝创建")}}));
    if (!waitUntil([&]() { return submit->isEnabled(); }) || !dialog->isVisible() ||
        nameInput->text() != QStringLiteral("测试公开房间")) return 78;
    submit->click();
    if (!waitUntil([&]() { return createCount() == beforeCreate + 2; })) return 79;
    const QJsonObject created{{"room_id", "room-created"}, {"room_name", QStringLiteral("测试公开房间")},
        {"description", QStringLiteral("客户端创建流程")}, {"room_type", "public"},
        {"owner_id", QStringLiteral("11")}};
    const int beforeNewSwitch = switchCount();
    peers.first()->write(roomResult(QJsonObject{{"action", "create"}, {"ok", true}, {"room", created}}));
    if (!waitUntil([&]() { return switchCount() == beforeNewSwitch + 1 && roomList->count() == 3; })) return 80;
    peers.first()->write(roomResult(QJsonObject{{"action", "switch"}, {"ok", true},
        {"room_id", "room-created"}}));
    peers.first()->write("HISTORY_END\n");
    QJsonArray updatedRooms = rooms;
    updatedRooms.append(created);
    peers.first()->write(roomResult(QJsonObject{{"action", "list_mine"}, {"ok", true},
        {"rooms", updatedRooms}}));
    if (!waitUntil([&]() { return roomList->count() == 3 && roomList->item(2)->isSelected(); })) return 81;
    if (dialog && dialog->isVisible()) return 82;
    if (rightPanel->isVisible() || !unfold->isVisible()) return 83; // Switch preserved the fold.
    unfold->click();
    waitEvents();
    if (!rightPanel->isVisible() || unfold->isVisible()) return 70;

    // A timed-out create remains unresolved; a second click cannot duplicate it.
    createButton->click();
    QPointer<RoomCreateDialog> inviteDialog = a.findChild<RoomCreateDialog *>();
    if (!inviteDialog) return 84;
    auto *inviteName = inviteDialog->findChild<QLineEdit *>(QStringLiteral("roomNameInput"));
    auto *inviteSubmit = inviteDialog->findChild<QPushButton *>(QStringLiteral("createRoomSubmit"));
    inviteName->setText(QStringLiteral("邀请房间测试"));
    inviteSubmit->click();
    if (!waitUntil([&]() { return createCount() == beforeCreate + 3; })) return 85;
    const QStringList inviteParts = roomCommands.last().split('|');
    if (inviteParts.size() != 3 ||
        QJsonDocument::fromJson(QByteArray::fromBase64(inviteParts.at(2).toLatin1())).object()
            .value(QStringLiteral("room_type")).toString() != QStringLiteral("invite_only")) return 86;
    if (!waitUntil([&]() { return inviteSubmit->isEnabled(); }, 17000)) return 87;
    inviteSubmit->click();
    waitEvents();
    if (createCount() != beforeCreate + 3) return 88;
    peers.first()->write(roomResult(QJsonObject{{"action", "create"}, {"ok", false},
        {"error", QStringLiteral("稍后重试")}}));
    waitEvents();
    if (!inviteDialog || !inviteSubmit->isEnabled()) return 89;
    inviteDialog->close();

    // Addressed invitations stay bound to the room at dialog creation. A fake
    // server controls results; this test never changes a real database.
    auto *inviteButton = a.findChild<QPushButton *>(QStringLiteral("inviteMembersButton"));
    if (!inviteButton || !inviteButton->isEnabled()) return 90;
    const auto lastRoomRequest = [&roomCommands](const QString &action) {
        for (auto it = roomCommands.crbegin(); it != roomCommands.crend(); ++it) {
            const QStringList parts = it->split(QLatin1Char('|'));
            if (parts.size() == 3 && parts.at(1) == action)
                return QJsonDocument::fromJson(QByteArray::fromBase64(parts.at(2).toLatin1())).object();
        }
        return QJsonObject{};
    };
    inviteButton->click();
    QPointer<RoomInviteDialog> chooser = a.findChild<RoomInviteDialog *>();
    if (!chooser || !chooser->isVisible()) return 91;
    if (!waitUntil([&]() { return !lastRoomRequest(QStringLiteral("search_users")).isEmpty(); })) return 92;
    QJsonObject searchRequest = lastRoomRequest(QStringLiteral("search_users"));
    if (searchRequest.value(QStringLiteral("room_id")) != QStringLiteral("room-created")) return 93;
    const QJsonArray candidates{
        QJsonObject{{"user_id", "22"}, {"nickname", QStringLiteral("乙")}, {"online", true}},
        QJsonObject{{"user_id", "33"}, {"nickname", QStringLiteral("丙")}, {"online", false}},
        QJsonObject{{"user_id", "44"}, {"nickname", QStringLiteral("已加入")}, {"member", true}},
        QJsonObject{{"user_id", "55"}, {"nickname", QStringLiteral("已邀请")}, {"pending", true}}};
    peers.first()->write(roomResult(QJsonObject{{"action", "search_users"}, {"ok", true},
        {"request_id", searchRequest.value(QStringLiteral("request_id"))},
        {"room_id", "room-created"}, {"users", candidates}}));
    if (!waitUntil([&]() { return chooser && chooser->findChildren<QCheckBox *>().size() == 4; })) return 94;
    if (argc > 5) chooser->grab().save(QString::fromLocal8Bit(argv[5]));
    auto checks = chooser->findChildren<QCheckBox *>();
    if (checks.at(2)->isEnabled() || checks.at(3)->isEnabled()) return 95;
    checks.at(0)->click();
    checks.at(1)->click();
    QPushButton *sendInvites = nullptr;
    for (QPushButton *button : chooser->findChildren<QPushButton *>())
        if (button->text() == QStringLiteral("发送邀请")) sendInvites = button;
    if (!sendInvites || !sendInvites->isEnabled()) return 96;
    sendInvites->click(); sendInvites->click();
    if (!waitUntil([&]() { return !lastRoomRequest(QStringLiteral("send_user_invites")).isEmpty(); })) return 97;
    const QJsonObject sendRequest = lastRoomRequest(QStringLiteral("send_user_invites"));
    int sends = 0;
    for (const QString &cmd : roomCommands)
        if (cmd.startsWith(QStringLiteral("ROOM|send_user_invites|"))) ++sends;
    if (sends != 1 || sendRequest.value(QStringLiteral("room_id")) != QStringLiteral("room-created") ||
        sendRequest.value(QStringLiteral("user_ids")).toArray().size() != 2) return 98;
    peers.first()->write(roomResult(QJsonObject{{"action", "send_user_invites"}, {"ok", true},
        {"request_id", sendRequest.value(QStringLiteral("request_id"))},
        {"sent", QJsonArray{QJsonObject{{"user_id", "22"}}, QJsonObject{{"user_id", "33"}}}}}));
    if (!waitUntil([&]() { return chooser && sendInvites->text() == QStringLiteral("发送邀请"); })) return 99;
    chooser->close();

    auto *inboxButton = a.findChild<QToolButton *>(QStringLiteral("inviteInboxButton"));
    if (!inboxButton) return 100;
    const QJsonArray inboxItems{QJsonObject{{"invite_id", "invite-1"},
        {"room_id", "room-invited"}, {"room_name", QStringLiteral("受邀房间")},
        {"description", QStringLiteral("离线邀请")}, {"inviter", QStringLiteral("乙")}}};
    peers.first()->write(roomResult(QJsonObject{{"action", "pending_invites"}, {"ok", true},
        {"invites", inboxItems}}));
    if (!waitUntil([&]() { return inboxButton->text() == QStringLiteral("邀请") &&
        inboxButton->property("hasPendingInvite").toBool(); })) return 101;
    if (argc > 7) a.grab().save(QString::fromLocal8Bit(argv[7]));
    inboxButton->click();
    QPointer<RoomInviteInboxDialog> inbox = a.findChild<RoomInviteInboxDialog *>();
    if (!inbox || !inbox->isVisible()) return 102;
    if (!inboxButton->property("hasPendingInvite").toBool()) return 110;
    if (argc > 6) inbox->grab().save(QString::fromLocal8Bit(argv[6]));
    QPushButton *acceptInvite = nullptr;
    for (QPushButton *button : inbox->findChildren<QPushButton *>())
        if (button->text() == QStringLiteral("接受")) acceptInvite = button;
    if (!acceptInvite) return 103;
    acceptInvite->click();
    if (!waitUntil([&]() { return !lastRoomRequest(QStringLiteral("respond_invite")).isEmpty(); })) return 104;
    const QJsonObject decision = lastRoomRequest(QStringLiteral("respond_invite"));
    if (decision.value(QStringLiteral("invite_id")) != QStringLiteral("invite-1") ||
        decision.value(QStringLiteral("decision")) != QStringLiteral("accept")) return 105;
    int listBefore = 0;
    for (const QString &cmd : roomCommands)
        if (cmd.startsWith(QStringLiteral("ROOM|list_mine|"))) ++listBefore;
    peers.first()->write(roomResult(QJsonObject{{"action", "respond_invite"}, {"ok", true},
        {"request_id", decision.value(QStringLiteral("request_id"))},
        {"decision", "accept"}, {"room_id", "room-invited"}}));
    if (!waitUntil([&]() {
        int count = 0;
        for (const QString &cmd : roomCommands)
            if (cmd.startsWith(QStringLiteral("ROOM|list_mine|"))) ++count;
        return count == listBefore + 1;
    })) return 106;
    peers.first()->write(roomResult(QJsonObject{{"action", "pending_invites"}, {"ok", true},
        {"invites", QJsonArray{}}}));
    if (!waitUntil([&]() { return !inboxButton->property("hasPendingInvite").toBool() &&
        inboxButton->text() == QStringLiteral("邀请"); })) return 111;
    inbox->close();
    waitEvents();
    const QJsonArray declinedItem{QJsonObject{{"invite_id", "invite-2"},
        {"room_id", "room-declined"}, {"room_name", QStringLiteral("待拒绝房间")},
        {"inviter", QStringLiteral("乙")}}};
    peers.first()->write(roomResult(QJsonObject{{"action", "pending_invites"}, {"ok", true},
        {"invites", declinedItem}}));
    if (!waitUntil([&]() { return inboxButton->property("hasPendingInvite").toBool(); })) return 114;
    inboxButton->click();
    QPointer<RoomInviteInboxDialog> declineInbox = a.findChild<RoomInviteInboxDialog *>();
    if (!declineInbox || !declineInbox->isVisible()) return 115;
    QPushButton *declineButton = nullptr;
    for (QPushButton *button : declineInbox->findChildren<QPushButton *>())
        if (button->text() == QStringLiteral("拒绝")) declineButton = button;
    if (!declineButton) return 116;
    declineButton->click();
    if (!waitUntil([&]() { return lastRoomRequest(QStringLiteral("respond_invite"))
        .value(QStringLiteral("invite_id")) == QStringLiteral("invite-2"); })) return 117;
    const QJsonObject declined = lastRoomRequest(QStringLiteral("respond_invite"));
    if (declined.value(QStringLiteral("decision")) != QStringLiteral("decline")) return 118;
    peers.first()->write(roomResult(QJsonObject{{"action", "respond_invite"}, {"ok", true},
        {"request_id", declined.value(QStringLiteral("request_id"))},
        {"decision", "decline"}, {"room_id", "room-declined"}}));
    peers.first()->write(roomResult(QJsonObject{{"action", "pending_invites"}, {"ok", true},
        {"invites", QJsonArray{}}}));
    if (!waitUntil([&]() { return !inboxButton->property("hasPendingInvite").toBool(); })) return 119;
    declineInbox->close();

    // The same server room is visible to account 22, but its owner ID and
    // member role must not expose the invitation action to this account.
    peers.at(1)->write("HISTORY_END\n");
    waitEvents();
    peers.at(1)->write(roomResult(QJsonObject{{"action", "list_mine"}, {"ok", true},
        {"rooms", rooms}}));
    auto *memberRooms = b.findChild<QListWidget *>(QStringLiteral("roomList"));
    if (!waitUntil([&]() { return memberRooms && memberRooms->count() == 2; })) return 112;
    memberRooms->itemClicked(memberRooms->item(1));
    peers.at(1)->write(roomResult(QJsonObject{{"action", "switch"}, {"ok", true},
        {"room_id", "room-two"}}));
    peers.at(1)->write("HISTORY_END\n");
    peers.at(1)->write(roomResult(QJsonObject{{"action", "members"}, {"ok", true},
        {"room_id", "room-two"}, {"members", roomMembers}}));
    auto *memberInvite = b.findChild<QPushButton *>(QStringLiteral("inviteMembersButton"));
    if (!waitUntil([&]() { return memberInvite && !memberInvite->isVisible() &&
        b.findChild<QLabel *>(QStringLiteral("subtitleLabel"))->text() ==
            QStringLiteral("轻松连接每一次交流"); })) return 113;

    // The management dialog uses the selected room and the server response.
    // No real database is touched by this fake TCP peer.
    roomMore->click();
    QMenu *managementMenu = roomMore->findChild<QMenu *>();
    if (!managementMenu || managementMenu->actions().isEmpty() ||
        managementMenu->actions().first()->text() != QStringLiteral("聊天室详情")) return 120;
    waitEvents(30);
    const auto menuRows = managementMenu->findChildren<QPushButton *>(QStringLiteral("roomMenuRow"));
    if (menuRows.size() != 3) return 125;
    int iconX = -1;
    int textX = -1;
    for (QPushButton *row : menuRows) {
        auto *icon = row->findChild<QLabel *>(QStringLiteral("roomMenuIcon"));
        auto *caption = row->findChild<QLabel *>(QStringLiteral("roomMenuText"));
        if (!icon || !caption || icon->size() != QSize(20, 20) || row->height() != 44) return 126;
        const int currentIconX = icon->mapTo(managementMenu, QPoint(0, 0)).x();
        const int currentTextX = caption->mapTo(managementMenu, QPoint(0, 0)).x();
        if (iconX >= 0 && (currentIconX != iconX || currentTextX != textX)) return 127;
        iconX = currentIconX;
        textX = currentTextX;
    }
    if (textX - iconX != 32) return 128; // 20 px icon area + 12 px gap.
    if (argc > 11) {
        managementMenu->grab().save(QString::fromLocal8Bit(argv[11]));
    }
    menuRows.first()->click();
    QPointer<RoomManagementDialog> management = a.findChild<RoomManagementDialog *>();
    if (!management || management->roomId() != QStringLiteral("room-created") ||
        !management->windowFlags().testFlag(Qt::FramelessWindowHint) ||
        !waitUntil([&]() { return lastRoomRequest(QStringLiteral("details"))
            .value(QStringLiteral("room_id")).toString() == QStringLiteral("room-created"); })) return 121;
    QJsonObject managed = created;
    managed.insert(QStringLiteral("owner_name"), QStringLiteral("甲"));
    managed.insert(QStringLiteral("member_count"), 2);
    managed.insert(QStringLiteral("created_at"), QStringLiteral("2026-01-01T12:00:00.000"));
    peers.first()->write(roomResult(QJsonObject{{"action", "details"}, {"ok", true},
        {"room_id", "room-created"}, {"room", managed}}));
    const QJsonArray managementMembers{
        QJsonObject{{"user_id", "11"}, {"nickname", QStringLiteral("甲")},
                    {"role", "owner"}, {"online", true}},
        QJsonObject{{"user_id", "22"}, {"nickname", QStringLiteral("乙")},
                    {"role", "member"}, {"online", false}}};
    peers.first()->write(roomResult(QJsonObject{{"action", "members"}, {"ok", true},
        {"room_id", "room-created"}, {"members", managementMembers}}));
    QPushButton *editRoom = nullptr;
    for (QPushButton *button : management->findChildren<QPushButton *>())
        if (button->accessibleName() == QStringLiteral("编辑聊天室信息")) editRoom = button;
    if (!waitUntil([&]() {
        if (!editRoom || !editRoom->isVisible()) return false;
        for (QLabel *label : management->findChildren<QLabel *>())
            if (label->text() == QStringLiteral("测试公开房间")) return true;
        return false;
    })) return 122;
    QPushButton *privacyRow = nullptr;
    for (QPushButton *button : management->findChildren<QPushButton *>())
        if (button->accessibleName() == QStringLiteral("聊天室权限与隐私")) privacyRow = button;
    if (!privacyRow || !privacyRow->isEnabled()) return 130;
    privacyRow->click();
    if (!waitUntil([&]() { return lastRoomRequest(QStringLiteral("permissions"))
        .value(QStringLiteral("room_id")) == QStringLiteral("room-created"); })) return 131;
    QPointer<RoomPermissionsDialog> privacy = a.findChild<RoomPermissionsDialog *>();
    if (!privacy || !privacy->windowFlags().testFlag(Qt::FramelessWindowHint)) return 132;
    auto *permissionsPages = privacy->findChild<QStackedWidget *>();
    QList<QPushButton *> navigation;
    for (QPushButton *button : privacy->findChildren<QPushButton *>())
        if (button->objectName() == QStringLiteral("permissionsNav")) navigation.append(button);
    if (!permissionsPages || permissionsPages->count() != 3 || navigation.size() != 3) return 138;
    for (QLabel *label : privacy->findChildren<QLabel *>())
        if (label->text() == QStringLiteral("可见范围与加入权限") ||
            label->text() == QStringLiteral("公开聊天室的加入方式") ||
            label->text() == QStringLiteral("普通成员的管理权限")) return 139;
    const QStringList navTitles = {QStringLiteral("聊天室隐私"), QStringLiteral("加入规则"),
                                   QStringLiteral("成员权限")};
    for (int i = 0; i < 3; ++i) {
        QPushButton *button = nullptr;
        for (auto *candidate : navigation)
            if (candidate->text() == navTitles.at(i)) button = candidate;
        if (!button || button->icon().isNull()) return 140;
        button->click();
        if (permissionsPages->currentIndex() != i) return 141;
    }
    const QJsonObject readRequest = lastRoomRequest(QStringLiteral("permissions"));
    const QJsonObject initialPermissions{{"room_type", "public"}, {"join_policy", "direct"},
        {"allow_member_invite", false}, {"allow_member_edit", false},
        {"allow_member_kick", false}, {"allow_member_manage_permissions", false}};
    peers.first()->write(roomResult(QJsonObject{{"action", "permissions"}, {"ok", true},
        {"request_id", readRequest.value(QStringLiteral("request_id"))},
        {"room_id", "room-created"}, {"permissions", initialPermissions}}));
    QCheckBox *memberInviteOption = nullptr;
    for (QCheckBox *box : privacy->findChildren<QCheckBox *>())
        if (box->text() == QStringLiteral("允许成员邀请他人")) memberInviteOption = box;
    if (!waitUntil([&]() { return memberInviteOption && memberInviteOption->isEnabled(); })) return 133;
    memberInviteOption->setChecked(true);
    QPushButton *savePermissions = nullptr;
    for (QPushButton *button : privacy->findChildren<QPushButton *>())
        if (button->text() == QStringLiteral("保存设置")) savePermissions = button;
    if (!savePermissions || !savePermissions->isEnabled()) return 134;
    savePermissions->click();
    if (!waitUntil([&]() { return lastRoomRequest(QStringLiteral("update_permissions"))
        .value(QStringLiteral("room_id")) == QStringLiteral("room-created"); })) return 135;
    const QJsonObject saveRequest = lastRoomRequest(QStringLiteral("update_permissions"));
    if (!saveRequest.value(QStringLiteral("permissions")).toObject()
            .value(QStringLiteral("allow_member_invite")).toBool()) return 136;
    peers.first()->write(roomResult(QJsonObject{{"action", "update_permissions"}, {"ok", true},
        {"request_id", saveRequest.value(QStringLiteral("request_id"))},
        {"room_id", "room-created"}}));
    if (!waitUntil([&]() { return !privacy; })) return 137;
    if (argc > 8) management->grab().save(QString::fromLocal8Bit(argv[8]));
    management->showPage(QStringLiteral("members"));
    if (argc > 10) management->grab().save(QString::fromLocal8Bit(argv[10]));
    management->showPage(QStringLiteral("details"));
    editRoom->click();
    if (argc > 9) management->grab().save(QString::fromLocal8Bit(argv[9]));
    auto *editName = management->findChild<QLineEdit *>();
    QPushButton *saveRoom = nullptr;
    for (QPushButton *button : management->findChildren<QPushButton *>())
        if (button->text() == QStringLiteral("保存修改")) saveRoom = button;
    if (!editName || !saveRoom || editName->text() != QStringLiteral("测试公开房间")) return 123;
    editName->setText(QStringLiteral("项目组改名"));
    saveRoom->click();
    if (!waitUntil([&]() { return lastRoomRequest(QStringLiteral("update_details"))
        .value(QStringLiteral("room_name")).toString() == QStringLiteral("项目组改名"); })) return 124;
    const QJsonObject editRequest = lastRoomRequest(QStringLiteral("update_details"));
    managed.insert(QStringLiteral("room_name"), QStringLiteral("项目组改名"));
    peers.first()->write(roomResult(QJsonObject{{"action", "update_details"}, {"ok", true},
        {"request_id", editRequest.value(QStringLiteral("request_id"))}, {"room", managed}}));
    if (!waitUntil([&]() { return a.findChild<QLabel *>(QStringLiteral("roomNameLabel"))->text()
        == QStringLiteral("项目组改名") && roomList->item(2)->toolTip().contains(QStringLiteral("项目组改名")); }))
        return 125;
    management->close();
    const int beforeRevokedSwitch = switchCount();
    const QJsonObject revoked{{"event", "access_revoked"}, {"room_id", "room-created"}};
    peers.first()->write("ROOM|notice|" +
        QJsonDocument(revoked).toJson(QJsonDocument::Compact).toBase64() + "\n");
    if (!waitUntil([&]() { return roomList->count() == 2 &&
        switchCount() == beforeRevokedSwitch + 1; })) return 126;
    peers.first()->write(roomResult(QJsonObject{{"action", "switch"}, {"ok", true},
        {"room_id", "class-room"}}));
    peers.first()->write("HISTORY_END\n");
    if (!waitUntil([&]() { return !roomMore->isVisible() &&
        a.findChild<QLabel *>(QStringLiteral("roomNameLabel"))->text() == QStringLiteral("VChat 大厅"); }))
        return 127;

    if (argc > 2) {
        a.resize(1200, 780);
        waitEvents();
        a.grab().save(QString::fromLocal8Bit(argv[2]));
    }
    return 0;
}
