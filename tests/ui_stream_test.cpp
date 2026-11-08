#include "mainwindow.h"
#include "aimarkdownwidget.h"
#include "roomsidebarwidget.h"
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QLayout>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
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
                    {"description", QStringLiteral("隔离测试")}}};
    peers.first()->write("HISTORY_END\n");
    if (!waitUntil([&]() { return roomCommands.join('\n').contains(QStringLiteral("ROOM|list_mine|")); })) return 46;
    peers.first()->write(roomResult(QJsonObject{{"action", "list_mine"}, {"ok", true}, {"rooms", rooms}}));
    auto *sidebar = a.findChild<RoomSidebarWidget *>();
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
        QJsonObject{{"user_id", "11"}, {"nickname", QStringLiteral("甲")}},
        QJsonObject{{"user_id", "22"}, {"nickname", QStringLiteral("乙")}}};
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
    if (argc > 2) {
        a.resize(1200, 780);
        waitEvents();
        a.grab().save(QString::fromLocal8Bit(argv[2]));
    }
    return 0;
}
