#include "mainwindow.h"
#include "aimarkdownwidget.h"
#include "roomsidebarwidget.h"
#include "roomcreatedialog.h"
#include "roominvitedialog.h"
#include "roommanagementdialog.h"
#include "roompermissionsdialog.h"
#include <QApplication>
#include <QAction>
#include <QClipboard>
#include <QCheckBox>
#include <QDir>
#include <QDateTime>
#include <QCursor>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QGraphicsOpacityEffect>
#include <QHelpEvent>
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
#include <QProcess>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QToolTip>
#include <QStackedWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QTimeZone>
#include <QToolButton>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
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

static int testRoomWelcomeToast(const QString &capturePath)
{
    QTemporaryDir settingsDir;
    if (!settingsDir.isValid()) return 1;
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());

    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) return 2;
    const QString address = QStringLiteral("127.0.0.1:%1").arg(server.serverPort());
    MainWindow window(QStringLiteral("测试用户"), address, nullptr, 11, QStringLiteral("test-token"));
    window.resize(1050, 700);
    window.show();
    if (!waitUntil([&] { return server.hasPendingConnections(); })) return 3;
    QTcpSocket *peer = server.nextPendingConnection();
    QByteArray commands;
    if (!waitUntil([&] {
        commands += peer->readAll();
        return commands.contains("JOIN|");
    })) return 4;

    auto *toast = window.findChild<QWidget *>(QStringLiteral("roomWelcomeToast"));
    auto *connectedPill = window.findChild<QWidget *>(QStringLiteral("roomConnectedToastPill"));
    auto *welcomePill = window.findChild<QWidget *>(QStringLiteral("roomWelcomeToastPill"));
    auto *scroll = window.findChild<QScrollArea *>(QStringLiteral("messageScroll"));
    auto *sidebar = window.findChild<RoomSidebarWidget *>();
    if (!toast || !connectedPill || !welcomePill || !scroll || !sidebar || !toast->isHidden()) return 5;
    QWidget *viewport = scroll->viewport();
    auto *connectedEffect = qobject_cast<QGraphicsOpacityEffect *>(connectedPill->graphicsEffect());
    auto *welcomeEffect = qobject_cast<QGraphicsOpacityEffect *>(welcomePill->graphicsEffect());
    if (toast->parentWidget() != viewport || !connectedEffect || !welcomeEffect ||
        connectedEffect == welcomeEffect ||
        !toast->testAttribute(Qt::WA_TransparentForMouseEvents)) return 6;

    const auto sendRoom = [&](const QByteArray &kind, const QJsonObject &object) {
        peer->write("ROOM|" + kind + '|' + QJsonDocument(object).toJson(QJsonDocument::Compact).toBase64() + '\n');
    };
    const QString lobby = QStringLiteral("class-room");
    const QString cppRoom = QStringLiteral("room-cpp");
    const QString project = QStringLiteral("room-project");
    const QString longRoom = QStringLiteral("跨平台客户端构建与持续集成讨论组 QtWidgetsIntegrationBuildVerificationReleaseCandidate");
    const QString unnamed = QStringLiteral("room-unnamed");
    const auto room = [](const QString &id, const QString &name) {
        return QJsonObject{{"room_id", id}, {"room_name", name}};
    };
    const QJsonArray rooms{room(lobby, QStringLiteral("VChat 大厅")),
                           room(cppRoom, QStringLiteral("C++ 聊天室")),
                           room(project, longRoom), room(unnamed, QStringLiteral("  "))};
    const QJsonObject roomList{{"action", "list_mine"}, {"ok", true}, {"rooms", rooms}};
    const auto textLabel = [](QWidget *pill) -> QLabel * {
        for (auto *label : pill->findChildren<QLabel *>())
            if (!label->text().isEmpty()) return label;
        return nullptr;
    };
    const auto wordOrder = [](QString text) {
        // Layout can wrap at a space or split a very long word.
        text.remove(QLatin1Char('\n'));
        text.remove(QLatin1Char(' '));
        return text;
    };
    const auto showsText = [&](QWidget *pill, const QString &expected, bool multiline) {
        auto *label = textLabel(pill);
        if (!label || wordOrder(label->text()) != wordOrder(expected) ||
            label->text().contains(QLatin1Char('\n')) != multiline ||
            !pill->rect().contains(label->geometry())) return false;
        const auto lines = label->text().split(QLatin1Char('\n'));
        const QFontMetricsF metrics(label->font(), label);
        for (const QString &line : lines)
            if (metrics.horizontalAdvance(line) > label->contentsRect().width() + 0.5) return false;
        if (metrics.height() + (lines.size() - 1) * metrics.lineSpacing() > label->contentsRect().height() + 1)
            return false;
        for (auto *icon : pill->findChildren<QLabel *>())
            if (!icon->pixmap().isNull()) return label->x() - icon->geometry().right() >= 10;
        return false;
    };
    const auto showsConnection = [&] {
        return connectedPill->isVisible() && !welcomePill->isVisible() && connectedEffect->opacity() > 0.98 &&
            showsText(connectedPill, QStringLiteral("已连接到聊天室服务器"), false);
    };
    const auto showsWelcome = [&](const QString &name, bool multiline = false) {
        return welcomePill->isVisible() && !connectedPill->isVisible() && welcomeEffect->opacity() > 0.98 &&
            showsText(welcomePill, QStringLiteral("欢迎来到 %1").arg(name), multiline);
    };
    const auto topCentered = [&] {
        QWidget *pill = connectedPill->isVisible() ? connectedPill : welcomePill;
        return pill->isVisible() && pill->geometry() == toast->rect() &&
            qAbs(toast->geometry().center().x() - viewport->rect().center().x()) <= 1 &&
            toast->y() > 0 && toast->y() <= 24 && viewport->rect().contains(toast->geometry()) &&
            viewport->childAt(toast->geometry().center()) == scroll->widget();
    };
    const auto switchReply = [&](const QString &id) {
        sendRoom("result", {{"action", "switch"}, {"ok", true}, {"room_id", id}});
    };

    peer->write(QStringLiteral("[System] Connected to chat server\n[System] Welcome 测试用户\n").toUtf8());
    waitEvents();
    if (toast->isVisible() || scroll->widget()->findChild<QLabel *>(QStringLiteral("systemMessageText"))) return 7;
    peer->write("HISTORY_END\n");
    waitEvents();
    if (toast->isVisible()) return 8; // The server's room name hasn't arrived yet.
    QElapsedTimer shownFor;
    shownFor.start();
    sendRoom("result", roomList);
    if (!waitUntil(showsConnection)) return 9;
    if (!topCentered()) return 10;
    auto *composer = window.findChild<QWidget *>(QStringLiteral("composer"));
    auto *rightPanel = window.findChild<QWidget *>(QStringLiteral("rightPanel"));
    if (!composer || !rightPanel) return 11;
    const auto layoutState = [&]() {
        return QList<QRect>{scroll->geometry(), viewport->geometry(), composer->geometry(),
                            sidebar->geometry(), rightPanel->geometry()};
    };
    const auto visibleLayout = layoutState();
    bool overlap = false;
    const auto checkOverlap = [&] { overlap |= connectedPill->isVisible() && welcomePill->isVisible(); };
    QObject::connect(connectedEffect, &QGraphicsOpacityEffect::opacityChanged, &window, checkOverlap);
    QObject::connect(welcomeEffect, &QGraphicsOpacityEffect::opacityChanged, &window, checkOverlap);
    if (!waitUntil([&] {
        return connectedPill->isVisible() && connectedEffect->opacity() < 0.9 &&
            !welcomePill->isVisible();
    }, 2000)) return 23;
    if (!waitUntil([&] { return showsWelcome(QStringLiteral("VChat 大厅")); }) ||
        !topCentered() || layoutState() != visibleLayout || overlap) return 24;
    if (!capturePath.isEmpty() && !window.grab().save(capturePath)) return 12;
    if (!waitUntil([&] { return toast->isHidden(); }, 3500) || shownFor.elapsed() < 2500 ||
        connectedEffect->opacity() > 0.02 || welcomeEffect->opacity() > 0.02 || overlap ||
        layoutState() != visibleLayout) return 13;
    sendRoom("result", roomList);
    sendRoom("notice", {{"event", "members_changed"}, {"room_id", lobby}});
    peer->write("HISTORY_END\n[12:00:00] other: regular message\n");
    waitEvents(150);
    if (!toast->isHidden()) return 14;

    sidebar->roomSelected(cppRoom);
    switchReply(cppRoom);
    waitEvents();
    if (!toast->isHidden()) return 15;
    peer->write("HISTORY_END\n");
    if (!waitUntil(showsConnection) ||
        !waitUntil([&] { return showsWelcome(QStringLiteral("C++ 聊天室")) && topCentered(); })) return 16;

    sidebar->roomSelected(lobby);
    sidebar->roomSelected(project);
    if (!toast->isHidden()) return 17;
    switchReply(lobby);
    peer->write("HISTORY_END\n");
    waitEvents();
    if (!toast->isHidden()) return 18;
    switchReply(project);
    peer->write("HISTORY_END\n");
    if (!waitUntil(showsConnection) ||
        !waitUntil([&] { return showsWelcome(longRoom, true); }) ||
        window.findChildren<QWidget *>(QStringLiteral("roomWelcomeToast")).size() != 1) return 19;
    const int wideHeight = toast->height();
    window.resize(790, 560);
    if (!waitUntil([&] { return topCentered() && showsWelcome(longRoom, true); }) ||
        toast->height() < wideHeight) return 20;

    sidebar->roomSelected(unnamed);
    switchReply(unnamed);
    peer->write("HISTORY_END\n");
    if (!waitUntil(showsConnection) ||
        !waitUntil([&] { return showsWelcome(QStringLiteral("聊天室")); }) || overlap) return 21;
    qInfo("Room welcome toast verified at device scale %.2f", double(viewport->devicePixelRatioF()));
    peer->disconnectFromHost();
    return waitUntil([&] { return toast->isHidden(); }) ? 0 : 22;
}

static int testMessageNotifications(const QString &capturePath)
{
    QTemporaryDir settingsDir;
    if (!settingsDir.isValid()) return 1;
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settingsDir.path());
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) return 2;
    const QString address = QStringLiteral("127.0.0.1:%1").arg(server.serverPort());
    MainWindow window(QStringLiteral("用户B"), address, nullptr, 22, QStringLiteral("test-token"));
    window.resize(1050, 700);
    window.show();
    if (!waitUntil([&] { return server.hasPendingConnections(); })) return 3;
    QTcpSocket *peer = server.nextPendingConnection();
    QByteArray requests;
    auto saw = [&](const QByteArray &request) {
        return waitUntil([&] { requests += peer->readAll(); return requests.contains(request); });
    };
    if (!saw("JOIN|")) return 4;
    auto send = [&](const QByteArray &prefix, const QJsonObject &data) {
        peer->write(prefix + QJsonDocument(data).toJson(QJsonDocument::Compact).toBase64() + '\n');
    };
    auto state = [](const QString &room, int unread, int mentions, qint64 read, qint64 observed) {
        return QJsonObject{{"room_id", room}, {"unread", unread}, {"mentions", mentions},
            {"read_sequence_id", QString::number(read)}, {"through_sequence_id", QString::number(observed)}};
    };
    auto notify = [&](int sequence, bool mentioned = false, const QString &sender = QStringLiteral("11")) {
        send("NOTIFY|message|", {{"room_id", "cpp"}, {"message_id", QString::number(sequence)},
            {"sequence_id", QString::number(sequence)}, {"sender_id", sender}, {"type", "user"}, {"mentioned", mentioned}});
    };
    auto expectedRead = [](int sequence) {
        return "NOTIFY|read|" + QJsonDocument(QJsonObject{{"room_id", "cpp"},
            {"sequence_id", QString::number(sequence)}}).toJson(QJsonDocument::Compact).toBase64();
    };

    auto *sound = window.findChild<QAction *>(QStringLiteral("messageSoundSetting"));
    auto *mentions = window.findChild<QAction *>(QStringLiteral("mentionAlertsSetting"));
    auto *sidebar = window.findChild<RoomSidebarWidget *>();
    auto *list = window.findChild<QListWidget *>(QStringLiteral("roomList"));
    auto *scroll = window.findChild<QScrollArea *>(QStringLiteral("messageScroll"));
    auto *button = window.findChild<QPushButton *>(QStringLiteral("newMessagesButton"));
    if (!sound || !mentions || !sound->isChecked() || !mentions->isChecked() || !list || !sidebar || !scroll || !button) {
        qWarning() << "Notification controls or defaults unavailable" << (sound != nullptr)
                   << (mentions != nullptr) << (sound && sound->isChecked())
                   << (mentions && mentions->isChecked()) << (list != nullptr)
                   << (sidebar != nullptr) << (scroll != nullptr) << (button != nullptr);
        return 5;
    }
    sound->setChecked(false);
    peer->write("HISTORY_END\n");
    if (!saw("ROOM|list_mine|")) return 6;
    send("ROOM|result|", {{"action", "list_mine"}, {"ok", true}, {"notifications_supported", true},
        {"rooms", QJsonArray{QJsonObject{{"room_id", "class-room"}, {"room_name", "VChat 大厅"}},
                            QJsonObject{{"room_id", "cpp"}, {"room_name", "C++"}}}}});
    if (!saw("NOTIFY|sync|")) return 7;
    send("NOTIFY|snapshot|", {{"rooms", QJsonArray{state("cpp", 0, 0, 0, 0), state("class-room", 0, 0, 0, 0)}}});
    auto badge = [&](const QString &count, bool mentioned) {
        for (int i = 0; i < list->count(); ++i) {
            if (list->item(i)->data(Qt::UserRole).toString() != QStringLiteral("cpp")) continue;
            QWidget *row = list->itemWidget(list->item(i));
            auto *number = row->findChild<QLabel *>(QStringLiteral("roomUnreadCount"));
            auto *mention = row->findChild<QLabel *>(QStringLiteral("roomUnreadMention"));
            return number && mention && (count.isEmpty() ? number->isHidden() :
                !number->isHidden() && number->text() == count) && mention->isHidden() != mentioned;
        }
        return false;
    };
    notify(39);
    notify(40, true);
    notify(40, true);
    if (!waitUntil([&] { return badge(QStringLiteral("2"), true); }) || !button->isHidden()) return 8;
    mentions->setChecked(false);
    if (!badge(QStringLiteral("2"), false)) return 9;
    mentions->setChecked(true);
    if (!badge(QStringLiteral("2"), true)) return 10;
    send("NOTIFY|room|", state("cpp", 100, 1, 0, 40));
    if (!waitUntil([&] { return badge(QStringLiteral("99+"), true); })) return 11;
    send("NOTIFY|room|", state("cpp", 2, 1, 0, 40));

    sidebar->roomSelected(QStringLiteral("cpp"));
    if (!saw("ROOM|switch|")) return 12;
    send("ROOM|result|", {{"action", "switch"}, {"ok", true}, {"room_id", "cpp"}});
    for (int i = 1; i <= 40; ++i) {
        send("HISTORY|", {{"room_id", "cpp"}, {"message_id", QString::number(i)},
            {"sequence_id", QString::number(i)}, {"sender", "用户A"}, {"sender_id", "11"},
            {"type", "user"}, {"text", QStringLiteral("历史消息 %1").arg(i)}});
    }
    auto *bar = scroll->verticalScrollBar();
    if (!waitUntil([&] { return bar->maximum() > 200; })) return 13;
    requests += peer->readAll();
    if (requests.contains("NOTIFY|read|") || !badge(QStringLiteral("2"), true)) return 14;
    peer->write("HISTORY_END\n");
    if (!saw(expectedRead(40))) return 15;
    send("NOTIFY|room|", state("cpp", 0, 0, 40, 40));
    if (!waitUntil([&] { return badge({}, false); })) return 16;

    bar->setValue(0);
    auto live = [&](int sequence, const QString &sender = QStringLiteral("11")) {
        send("CHAT2|", {{"room_id", "cpp"}, {"messageId", QString::number(sequence)},
            {"senderId", sender}, {"sender", "用户A"}, {"sequence_id", QString::number(sequence)},
            {"text", QStringLiteral("新消息 %1").arg(sequence)}});
    };
    live(41);
    notify(41, true);
    live(42);
    notify(42);
    notify(43, false, QStringLiteral("22"));
    if (!waitUntil([&] { return button->isVisible() && button->text().contains(QStringLiteral("2 条新消息")); }) ||
        bar->value() != 0 || !badge(QStringLiteral("2"), true) || button->parentWidget() != scroll->viewport()) return 17;
    if (!capturePath.isEmpty() && !window.grab().save(capturePath)) return 21;
    button->click();
    if (!saw(expectedRead(42)) || bar->value() != bar->maximum() || !button->isHidden()) return 18;
    send("NOTIFY|room|", state("cpp", 0, 0, 42, 42));
    live(44);
    notify(44);
    if (!saw(expectedRead(44)) || !button->isHidden()) return 19;

    QSettings(QSettings::IniFormat, QSettings::UserScope,
              QStringLiteral("VChat"), QStringLiteral("VChatClient")).sync();
    QProcess reopened;
    reopened.start(QCoreApplication::applicationFilePath(),
                   {QStringLiteral("--verify-notification-settings"), settingsDir.path()});
    return reopened.waitForFinished(5000) && reopened.exitStatus() == QProcess::NormalExit &&
        reopened.exitCode() == 0 ? 0 : 20;
}

static int testAllMention()
{
    QTemporaryDir settingsDir;
    if (!settingsDir.isValid()) return 1;
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) return 2;
    MainWindow window(QStringLiteral("甲"), QStringLiteral("127.0.0.1:%1").arg(server.serverPort()),
                      nullptr, 11, QStringLiteral("test-token"));
    window.resize(1050, 700);
    window.show();
    if (!waitUntil([&] { return server.hasPendingConnections(); })) return 3;
    QTcpSocket *peer = server.nextPendingConnection();
    QByteArray requests;
    auto saw = [&](const QByteArray &request) {
        return waitUntil([&] { requests += peer->readAll(); return requests.contains(request); });
    };
    auto send = [&](const QByteArray &prefix, const QJsonDocument &data) {
        peer->write(prefix + data.toJson(QJsonDocument::Compact).toBase64() + '\n');
    };
    if (!saw("JOIN|")) return 4;
    send("USERLIST2|", QJsonDocument(QJsonArray{QJsonObject{{"id", "11"}, {"name", "甲"}},
                                                QJsonObject{{"id", "22"}, {"name", "乙"}}}));
    peer->write("HISTORY_END\n");
    if (!saw("ROOM|list_mine|")) return 5;
    send("ROOM|result|", QJsonDocument(QJsonObject{{"action", "list_mine"}, {"ok", true},
        {"notifications_supported", true}, {"mention_all_supported", true},
        {"rooms", QJsonArray{QJsonObject{{"room_id", "class-room"}, {"room_name", "大厅"}}}}}));
    if (!saw("ROOM|members|")) return 6;
    auto members = [&](const QString &room, bool third) {
        QJsonArray roster{QJsonObject{{"user_id", "11"}, {"nickname", "甲"}},
                          QJsonObject{{"user_id", "22"}, {"nickname", "乙"}}};
        if (third) roster.append(QJsonObject{{"user_id", "33"}, {"nickname", "丙"}, {"online", false}});
        send("ROOM|result|", QJsonDocument(QJsonObject{{"action", "members"}, {"ok", true},
                                                       {"room_id", room}, {"members", roster}}));
    };
    auto changedRoster = [&](bool third) {
        requests.clear();
        peer->readAll();
        send("ROOM|notice|", QJsonDocument(QJsonObject{{"event", "members_changed"}, {"room_id", "class-room"}}));
        if (!saw("ROOM|members|")) return false;
        members(QStringLiteral("class-room"), third);
        return true;
    };
    members(QStringLiteral("class-room"), false);
    auto *input = window.findChild<QTextEdit *>(QStringLiteral("messageInput"));
    auto *list = window.findChild<QListWidget *>(QStringLiteral("mentionList"));
    auto *sendButton = window.findChild<QPushButton *>(QStringLiteral("sendButton"));
    if (!input || !list || !sendButton) return 7;
    auto type = [&](const QString &value) {
        input->setPlainText(value);
        input->moveCursor(QTextCursor::End);
    };
    auto first = [&](const QString &label) {
        if (!list->isVisible() || list->count() == 0) return false;
        QWidget *row = list->itemWidget(list->item(0));
        if (!row) return false;
        for (const QLabel *widget : row->findChildren<QLabel *>())
            if (widget->text() == label) return true;
        return false;
    };
    auto outgoing = [&]() {
        requests.clear();
        peer->readAll();
        sendButton->click();
        if (!saw("CHAT2|")) return QJsonObject{};
        const QByteArray line = requests.mid(requests.indexOf("CHAT2|") + 6).split('\n').first();
        return QJsonDocument::fromJson(QByteArray::fromBase64(line)).object();
    };
    const QString token = QStringLiteral("@全体成员");
    type(QStringLiteral("@"));
    if (!waitUntil([&] { return first(QStringLiteral("Vivi")) && list->count() == 2; })) return 8;
    members(QStringLiteral("another-room"), true);
    waitEvents();
    if (!first(QStringLiteral("Vivi"))) return 9;
    if (!changedRoster(true) || !waitUntil([&] { return first(token); })) return 10;
    list->itemClicked(list->item(0));
    if (input->toPlainText() != token + QLatin1Char(' ')) return 11;
    input->insertPlainText(QStringLiteral("请  <b>检查</b>"));
    const QJsonObject group = outgoing();
    const auto groupMentions = group.value("mentions").toArray();
    if (groupMentions.size() != 1) return 12;
    const QJsonObject marker = groupMentions.first().toObject();
    if (group.value("text").toString() != token + QStringLiteral(" 请  <b>检查</b>") ||
        marker.value("type").toString() != QStringLiteral("all") || marker.value("id").toString() != QStringLiteral("all") ||
        marker.value("start").toInt(-1) != 0 || marker.value("length").toInt() != token.size()) return 12;

    QJsonObject echo = group;
    echo.insert("room_id", "class-room");
    echo.insert("senderId", "11");
    echo.insert("sender", "甲");
    send("CHAT2|", QJsonDocument(echo));
    if (!waitUntil([&] {
        for (const QLabel *label : window.findChildren<QLabel *>()) {
            if (label->property("plainMessageText").toString() != group.value("text").toString() ||
                label->textFormat() != Qt::RichText) continue;
            QTextDocument document;
            document.setHtml(label->text());
            return document.toPlainText() == group.value("text").toString() &&
                document.firstBlock().begin().fragment().charFormat().background().color() == QColor("#694cc1");
        }
        return false;
    })) return 13;

    type(token + QStringLiteral("示例"));
    const QJsonObject similar = outgoing();
    if (similar.value("text").toString() != token + QStringLiteral("示例") ||
        !similar.value("mentions").toArray().isEmpty()) return 14;
    type(QStringLiteral("@乙"));
    if (!waitUntil([&] { return first(QStringLiteral("乙")); })) return 15;
    list->itemClicked(list->item(0));
    const auto user = outgoing().value("mentions").toArray();
    if (user.size() != 1 || user.first().toObject().value("id").toString() != QStringLiteral("22")) return 16;
    type(QStringLiteral("@Vivi"));
    if (!waitUntil([&] { return first(QStringLiteral("Vivi")); })) return 17;
    list->itemClicked(list->item(0));
    const auto ai = outgoing().value("mentions").toArray();
    if (ai.size() != 1 || ai.first().toObject().value("type").toString() != QStringLiteral("ai")) return 18;
    type(QStringLiteral("@"));
    if (!waitUntil([&] { return first(token); }) || !changedRoster(false) ||
        !waitUntil([&] { return first(QStringLiteral("Vivi")) && list->count() == 2; })) return 19;
    qInfo("All-member mention popup, roster updates, rendering and existing mention types passed");
    return 0;
}

static int testEmptyRoomState(const QString &capturePath)
{
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) return 1;
    const QString address = QStringLiteral("127.0.0.1:%1").arg(server.serverPort());
    MainWindow window(QStringLiteral("测试用户"), address, nullptr, 9026, QStringLiteral("empty-room-test"));
    window.resize(1050, 700);
    window.show();
    if (!waitUntil([&] { return server.hasPendingConnections(); })) return 2;
    auto *peer = server.nextPendingConnection();
    QByteArray requests;
    const auto saw = [&](const QByteArray &command) {
        return waitUntil([&] { requests += peer->readAll(); return requests.contains(command); });
    };
    const auto send = [&](const QByteArray &prefix, const QJsonObject &data) {
        peer->write(prefix + QJsonDocument(data).toJson(QJsonDocument::Compact).toBase64() + '\n');
    };
    const auto room = [](const QString &id, const QString &name) {
        return QJsonObject{{"room_id", id}, {"room_name", name}};
    };
    const auto message = [](const QString &id, const QString &text) {
        return QJsonObject{{"room_id", id}, {"message_id", QStringLiteral("history-") + id},
            {"sender_id", "11"}, {"sender", "用户A"}, {"type", "user"}, {"text", text}};
    };
    auto *scroll = window.findChild<QScrollArea *>(QStringLiteral("messageScroll"));
    auto *placeholder = window.findChild<QWidget *>(QStringLiteral("roomMessagePlaceholder"));
    auto *empty = window.findChild<QWidget *>(QStringLiteral("roomEmptyState"));
    auto *loading = window.findChild<QLabel *>(QStringLiteral("roomHistoryLoading"));
    auto *title = window.findChild<QLabel *>(QStringLiteral("roomEmptyTitle"));
    auto *description = window.findChild<QLabel *>(QStringLiteral("roomEmptyDescription"));
    auto *illustration = window.findChild<QWidget *>(QStringLiteral("roomEmptyIllustration"));
    auto *sidebar = window.findChild<RoomSidebarWidget *>();
    auto *composer = window.findChild<QWidget *>(QStringLiteral("composer"));
    auto *members = window.findChild<QWidget *>(QStringLiteral("rightPanel"));
    auto *toast = window.findChild<QWidget *>(QStringLiteral("roomWelcomeToast"));
    if (!scroll || !placeholder || !empty || !loading || !title || !description || !illustration ||
        !sidebar || !composer || !members || !toast || !saw("JOIN|")) return 3;
    if (title->text() != QStringLiteral("开启一场新的交流") ||
        description->text() != QStringLiteral("这里还没有消息，发送第一条消息，开始你们的讨论吧。") ||
        placeholder->parentWidget() != scroll->viewport() ||
        !placeholder->testAttribute(Qt::WA_TransparentForMouseEvents)) return 4;
    if (!loading->isVisible() || empty->isVisible()) return 5;

    send("HISTORY|", message("class-room", QStringLiteral("大厅历史消息")));
    waitEvents();
    if (!loading->isVisible() || empty->isVisible()) return 26;
    peer->write("HISTORY_END\n");
    if (!saw("ROOM|list_mine|")) return 6;
    send("ROOM|result|", {{"action", "list_mine"}, {"ok", true}, {"rooms", QJsonArray{
        room("class-room", "VChat 大厅"), room("empty-remote", "项目讨论室"), room("empty-ai", "Vivi 体验室"),
        room("load-failed", "设计协作室")}}});
    if (!saw("ROOM|members|") || !placeholder->isHidden()) return 7;

    sidebar->createRequested();
    auto *create = window.findChild<RoomCreateDialog *>();
    if (!create) return 8;
    create->createSubmitted(QStringLiteral("C++"), {}, QStringLiteral("invite_only"));
    if (!saw("ROOM|create|")) return 9;
    send("ROOM|result|", {{"action", "create"}, {"ok", true}, {"room", room("empty-new", "C++")}});
    if (!saw("ROOM|switch|")) return 10;
    const auto acceptSwitch = [&](const QString &id) {
        send("ROOM|result|", {{"action", "switch"}, {"ok", true}, {"room_id", id}});
    };
    acceptSwitch(QStringLiteral("empty-new"));
    if (!waitUntil([&] { return loading->isVisible(); }) || empty->isVisible()) return 11;
    const auto geometry = [&] {
        return QList<QRect>{scroll->geometry(), scroll->viewport()->geometry(), composer->geometry(),
                            sidebar->geometry(), members->geometry()};
    };
    const auto loadingGeometry = geometry();
    // A delayed history frame from the previous room cannot count against this room.
    send("HISTORY|", message("class-room", QStringLiteral("旧房间迟到消息")));
    peer->write("HISTORY_END\n");
    if (!waitUntil([&] { return empty->isVisible(); }) || loading->isVisible() ||
        geometry() != loadingGeometry || scroll->verticalScrollBar()->maximum() != 0) return 12;
    const auto centered = [&] {
        const QRect viewport = scroll->viewport()->rect();
        const QPoint delta = empty->geometry().center() - viewport.center();
        return placeholder->geometry() == viewport && qAbs(delta.x()) <= 1 && qAbs(delta.y()) <= 2 &&
            viewport.contains(empty->geometry()) && illustration->isVisible() && description->isVisible();
    };
    if (!centered()) return 13;
    window.resize(850, 570);
    if (!waitUntil(centered)) return 14;
    if (!capturePath.isEmpty()) {
        window.resize(1050, 700);
        if (!waitUntil([&] { return centered() && toast->isHidden(); }, 3500) ||
            !window.grab().save(capturePath)) return 15;
    }
    peer->write(QStringLiteral("[System] 某成员 joined the chat\n").toUtf8());
    peer->write("USERLIST2|W10=\n");
    waitEvents();
    if (!empty->isVisible()) return 27;
    auto *input = window.findChild<QTextEdit *>(QStringLiteral("messageInput"));
    auto *sendButton = window.findChild<QPushButton *>(QStringLiteral("sendButton"));
    if (!input || !sendButton) return 16;
    input->setPlainText(QStringLiteral("新房间第一条消息"));
    sendButton->click();
    if (!saw("CHAT2|") || !placeholder->isHidden()) return 17;

    const auto enterEmpty = [&](const QString &id) {
        sidebar->roomSelected(id);
        acceptSwitch(id);
        if (!waitUntil([&] { return loading->isVisible(); }) || empty->isVisible()) return false;
        peer->write("HISTORY_END\n");
        return waitUntil([&] { return empty->isVisible(); });
    };
    if (!enterEmpty(QStringLiteral("empty-remote"))) return 18;
    send("CHAT2|", {{"room_id", "empty-remote"}, {"messageId", "remote-first"},
        {"sender", "用户A"}, {"senderId", "11"}, {"text", "其他成员的首条消息"}});
    if (!waitUntil([&] { return placeholder->isHidden(); })) return 19;
    if (!enterEmpty(QStringLiteral("empty-ai"))) return 20;
    auto vivi = event("ai_start", 1, "first-vivi", 11, QStringLiteral("用户A"), 0);
    vivi.insert(QStringLiteral("room_id"), QStringLiteral("empty-ai"));
    peer->write(frame(vivi));
    if (!waitUntil([&] { return placeholder->isHidden(); })) return 21;
    const QString answer = QStringLiteral("Vivi 的首条回复");
    auto complete = event("ai_complete", 1, "first-vivi", 11, QStringLiteral("用户A"), 1, answer);
    complete.insert(QStringLiteral("room_id"), QStringLiteral("empty-ai"));
    peer->write(frame(complete));
    if (!waitUntil([&] {
        for (const auto *body : window.findChildren<AiMarkdownWidget *>())
            if (body->markdownText() == answer) return placeholder->isHidden();
        return false;
    })) return 28;

    sidebar->roomSelected(QStringLiteral("class-room"));
    acceptSwitch(QStringLiteral("class-room"));
    if (!waitUntil([&] { return loading->isVisible(); })) return 22;
    send("HISTORY|", message("class-room", QStringLiteral("大厅历史消息")));
    peer->write("HISTORY_END\n");
    if (!waitUntil([&] { return placeholder->isHidden(); })) return 23;
    sidebar->roomSelected(QStringLiteral("load-failed"));
    acceptSwitch(QStringLiteral("load-failed"));
    if (!waitUntil([&] { return loading->isVisible(); })) return 24;
    peer->write(QStringLiteral("[System] 历史消息读取失败\n").toUtf8());
    return waitUntil([&] { return placeholder->isHidden(); }) ? 0 : 25;
}

static int testScopedRoomHistory()
{
    QTemporaryDir settingsDir;
    if (!settingsDir.isValid()) return 1;
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) return 2;
    MainWindow window(QStringLiteral("甲"), QStringLiteral("127.0.0.1:%1").arg(server.serverPort()),
                      nullptr, 11, QStringLiteral("test-token"));
    window.resize(1030, 700);
    window.show();
    if (!waitUntil([&] { return server.hasPendingConnections(); })) return 3;
    auto *peer = server.nextPendingConnection();
    auto *sidebar = window.findChild<RoomSidebarWidget *>();
    auto *scroll = window.findChild<QScrollArea *>(QStringLiteral("messageScroll"));
    auto *loading = window.findChild<QLabel *>(QStringLiteral("roomHistoryLoading"));
    auto *input = window.findChild<QTextEdit *>(QStringLiteral("messageInput"));
    if (!sidebar || !scroll || !loading || !input) return 4;
    QByteArray incoming;
    const auto saw = [&](const QByteArray &command) {
        return waitUntil([&] { incoming += peer->readAll(); return incoming.contains(command); });
    };
    const auto send = [&](const QByteArray &prefix, const QJsonObject &data) {
        peer->write(prefix + QJsonDocument(data).toJson(QJsonDocument::Compact).toBase64() + '\n');
    };
    const QString a = QStringLiteral("class-room"), b = QStringLiteral("room-b"), c = QStringLiteral("room-c");
    const auto history = [&](const QString &room, const QString &request, const QString &message, int sequence) {
        send("HISTORY|", {{"room_id", room}, {"request_id", request}, {"message_id", message},
            {"sender", "乙"}, {"sender_id", "22"}, {"type", "user"}, {"text", message},
            {"sequence_id", QString::number(sequence)}});
    };
    if (!saw("JOIN|")) return 5;
    history(a, {}, QStringLiteral("initial-A"), 1);
    peer->write("HISTORY_END\n");
    if (!saw("ROOM|list_mine|")) return 6;
    QJsonArray rooms;
    for (const QString &room : {a, b, c}) rooms.append(QJsonObject{{"room_id", room}, {"room_name", room}});
    send("ROOM|result|", {{"action", "list_mine"}, {"ok", true}, {"scoped_history_supported", true},
        {"ai_regenerate_by_message", true}, {"rooms", rooms}});
    if (!saw("ROOM|members|")) return 7;
    const auto choose = [&](const QString &room) {
        incoming.clear();
        peer->readAll();
        sidebar->roomSelected(room);
        if (!saw("ROOM|switch|")) return QString();
        const QByteArray frame = incoming.mid(incoming.indexOf("ROOM|switch|") + 12).split('\n').first();
        const auto fields = QJsonDocument::fromJson(QByteArray::fromBase64(frame)).object();
        return fields.value("room_id") == room && fields.value("scoped_history").toBool()
            ? fields.value("request_id").toString() : QString();
    };
    const auto ack = [&](const QString &room, const QString &request) {
        send("ROOM|result|", {{"action", "switch"}, {"ok", true}, {"room_id", room},
            {"request_id", request}, {"scoped_history", true}});
    };
    const auto finish = [&](const QString &room, const QString &request, bool ok = true) {
        send("HISTORY_DONE|", {{"room_id", room}, {"request_id", request}, {"ok", ok},
            {"error", ok ? QString() : QStringLiteral("历史读取失败，请重试")}});
    };
    const auto visibleIds = [&] {
        QStringList ids;
        for (const QWidget *row : scroll->widget()->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
            const QString id = row->property("userMessageId").toString();
            if (!id.isEmpty()) ids.append(id);
        }
        return ids;
    };
    const auto latest = [&] {
        return scroll->verticalScrollBar()->maximum() > 0 &&
            scroll->verticalScrollBar()->value() == scroll->verticalScrollBar()->maximum();
    };

    const QString firstB = choose(b);
    if (firstB.isEmpty() || !loading->isVisible() || !visibleIds().isEmpty()) return 8;
    ack(b, firstB);
    history(b, firstB, QStringLiteral("incomplete-B"), 2);
    waitEvents();
    if (!loading->isVisible() || !visibleIds().isEmpty()) return 9;
    const QString firstC = choose(c);
    if (firstC.isEmpty() || firstB == firstC) return 10;
    ack(b, firstB);
    ack(c, firstC);
    finish(b, firstB);
    history(c, firstB, QStringLiteral("stale-id-for-C"), 3);
    finish(c, firstB);
    peer->write("HISTORY_END\n");
    waitEvents();
    if (!loading->isVisible() || !visibleIds().isEmpty()) return 11;
    QStringList expected;
    for (int i = 1; i <= 36; ++i) {
        const QString id = QStringLiteral("C-message-%1").arg(i);
        expected.append(id);
        history(c, firstC, id, i);
    }
    for (const QString &id : {expected.last(), QStringLiteral("C-live")})
        send("CHAT2|", {{"room_id", c}, {"messageId", id}, {"sender", "乙"}, {"senderId", "22"},
            {"text", id}, {"sequence_id", id == expected.last() ? "36" : "37"}});
    expected.append(QStringLiteral("C-live"));
    finish(c, firstC);
    if (!waitUntil([&] { return !loading->isVisible() && visibleIds() == expected && latest(); })) return 12;

    for (int pass = 0; pass < 2; ++pass) {
        const QString nextA = choose(a), nextB = choose(b), nextC = choose(c);
        if (nextA.isEmpty() || nextB.isEmpty() || nextC.isEmpty() || nextC == firstC) return 13;
        ack(a, nextA);
        history(a, nextA, QStringLiteral("outdated-A"), 1);
        finish(a, nextA);
        ack(b, nextB);
        finish(b, nextB, false);
        ack(c, nextC);
        history(c, firstC, QStringLiteral("outdated-C"), 1);
        history(c, nextC, QStringLiteral("final-C"), 1);
        finish(c, nextC);
        if (!waitUntil([&] { return !loading->isVisible() && visibleIds() == QStringList{QStringLiteral("final-C")}; }))
            return 14;
    }

    const QString failed = choose(b);
    if (failed.isEmpty()) return 15;
    ack(b, failed);
    history(b, failed, QStringLiteral("partial-B"), 1);
    finish(b, failed, false);
    if (!waitUntil([&] {
        const auto *error = window.findChild<QLabel *>(QStringLiteral("systemMessageText"));
        return !loading->isVisible() && !input->isEnabled() && visibleIds().isEmpty() && error &&
            error->text().contains(QStringLiteral("请重试"));
    })) return 16;
    const QString retryB = choose(b);
    if (retryB.isEmpty() || retryB == failed) return 17;
    ack(b, retryB);
    history(b, failed, QStringLiteral("failed-request-B"), 1);
    history(b, retryB, QStringLiteral("reloaded-B"), 2);
    const QString aiId = QStringLiteral("467762c9-9259-475c-af48-89507c29193d");
    send("HISTORY|", {{"room_id", b}, {"request_id", retryB}, {"message_id", aiId}, {"sender_id", "11"},
        {"sender", "Vivi"}, {"type", "ai"}, {"text", "persisted answer"}, {"ai_state", "completed"},
        {"original_request_id", "1"}, {"can_regenerate", true}});
    finish(b, retryB);
    if (!waitUntil([&] {
        return !loading->isVisible() && input->isEnabled() && visibleIds() == QStringList{QStringLiteral("reloaded-B")};
    })) return 18;
    auto *retry = window.findChild<QToolButton *>(QStringLiteral("viviActionRegenerate"));
    if (!retry || !retry->isVisible() || !retry->isEnabled()) return 19;
    retry->click();
    return saw("AI_REGENERATE|" + aiId.toLatin1()) ? 0 : 20;
}

static int testViviCodeLayout(const QString &capturePath)
{
    QTemporaryDir settingsDir;
    if (!settingsDir.isValid()) return 1;
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) return 2;
    MainWindow window(QStringLiteral("甲"), QStringLiteral("127.0.0.1:%1").arg(server.serverPort()),
                      nullptr, 11, QStringLiteral("test-token"));
    window.resize(1440, 860);
    window.show();
    if (!waitUntil([&] { return server.hasPendingConnections(); })) return 3;
    auto *peer = server.nextPendingConnection();
    auto *scroll = window.findChild<QScrollArea *>(QStringLiteral("messageScroll"));
    if (!scroll || !waitUntil([&] { return peer->bytesAvailable() > 0; })) return 4;

    const QString hello = QStringLiteral("#include <iostream>\n\nint main() {\n"
                                         "    std::cout << \"hello world\" << std::endl;\n"
                                         "    return 0;\n}");
    const QString five = QStringLiteral("int sum(int a, int b) {\n\tint result = a + b;\n\n\treturn result;\n}");
    const QString medium = QStringList(20, QStringLiteral("int value = 42;")).join(QLatin1Char('\n'));
    const QString wide = QStringLiteral("    // ") + QString(64, QLatin1Char('x'));
    const QString large = QStringLiteral("int value = 42;\n").repeated(59) + wide;
    const auto fenced = [](const QString &body) { return QStringLiteral("```cpp\n%1\n```").arg(body); };
    const auto sendHistory = [&](const QString &id, const QString &markdown) {
        const QJsonObject item{{"room_id", "class-room"}, {"message_id", id}, {"sender_id", "11"},
            {"sender", "Vivi"}, {"type", "ai"}, {"ai_state", "completed"}, {"text", markdown}};
        peer->write("HISTORY|" + QJsonDocument(item).toJson(QJsonDocument::Compact).toBase64() + '\n');
    };
    const auto bubble = [&](const QString &id) -> QFrame * {
        for (const QWidget *row : scroll->widget()->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
            if (row->property("aiMessageId").toString() == id)
                return row->findChild<QFrame *>(QStringLiteral("viviReadingCard"));
        return nullptr;
    };
    sendHistory(QStringLiteral("hello"), fenced(hello));
    peer->write("HISTORY_END\n");
    if (!waitUntil([&] { return bubble(QStringLiteral("hello")) && !bubble(QStringLiteral("hello"))
                        ->findChildren<QPlainTextEdit *>().isEmpty(); })) return 5;
    auto *helloBubble = bubble(QStringLiteral("hello"));
    auto *shortEditor = helloBubble->findChild<QPlainTextEdit *>();
    waitEvents();
    if (!capturePath.isEmpty() && !window.grab().save(capturePath)) return 6;

    sendHistory(QStringLiteral("multi-code"), fenced(five) + QStringLiteral("\n\n") + fenced(medium)
                + QStringLiteral("\n\n") + fenced(large) + QStringLiteral("\n\n") + fenced(wide));
    QFrame *multi = nullptr;
    if (!waitUntil([&] {
        multi = bubble(QStringLiteral("multi-code"));
        return multi && multi->findChildren<QPlainTextEdit *>().size() == 4;
    })) return 7;
    const auto editors = multi->findChildren<QPlainTextEdit *>();
    const auto noScroll = [](const QPlainTextEdit *editor) {
        return editor->horizontalScrollBar()->maximum() == 0 && editor->verticalScrollBar()->maximum() == 0 &&
            !editor->horizontalScrollBar()->isVisible() && !editor->verticalScrollBar()->isVisible();
    };
    const auto textHeight = [](const QPlainTextEdit *editor, int limit) {
        auto *doc = editor->document();
        qreal height = doc->documentMargin();
        for (QTextBlock block = doc->begin(); block.isValid() && block.blockNumber() < limit; block = block.next())
            height += doc->documentLayout()->blockBoundingRect(block).height();
        return qCeil(height);
    };
    const auto fits = [&](const QPlainTextEdit *editor) {
        const int expected = textHeight(editor, editor->blockCount());
        return editor->viewport()->height() >= expected && editor->viewport()->height() <= expected + 2;
    };
    waitEvents();
    if (!noScroll(shortEditor) || !fits(shortEditor) || shortEditor->toPlainText() != hello ||
        !noScroll(editors.at(0)) || !fits(editors.at(0)) || editors.at(0)->toPlainText() != five ||
        !noScroll(editors.at(1)) || !fits(editors.at(1)) || editors.at(1)->toPlainText() != medium) {
        qWarning("Short/medium code heights: hello=%d/%d, five=%d/%d, medium=%d/%d",
                 shortEditor->viewport()->height(), textHeight(shortEditor, shortEditor->blockCount()),
                 editors.at(0)->viewport()->height(), textHeight(editors.at(0), 5),
                 editors.at(1)->viewport()->height(), textHeight(editors.at(1), 20));
        for (const QPlainTextEdit *editor : {shortEditor, editors.at(0), editors.at(1)}) {
            const auto *body = editor->parentWidget()->parentWidget();
            qWarning() << "Code editor geometry" << editor->size() << editor->viewport()->size()
                       << editor->document()->documentLayout()->documentSize()
                       << editor->horizontalScrollBar()->maximum() << editor->verticalScrollBar()->maximum()
                       << editor->document()->defaultFont() << editor->parentWidget()->size()
                       << editor->parentWidget()->contentsMargins() << body->width();
        }
        return 8;
    }
    auto *longEditor = editors.at(2);
    auto *wideEditor = editors.at(3);
    if (longEditor->blockCount() != 60 || longEditor->toPlainText() != large ||
        !longEditor->verticalScrollBar()->isVisible() || longEditor->horizontalScrollBar()->isVisible() ||
        longEditor->viewport()->height() < textHeight(longEditor, 20) ||
        longEditor->viewport()->height() >= textHeight(longEditor, 21)) return 9;
    longEditor->verticalScrollBar()->setValue(longEditor->verticalScrollBar()->maximum());
    if (longEditor->verticalScrollBar()->value() <= 0) return 10;
    const auto copies = multi->findChildren<QPushButton *>();
    if (copies.size() != 4) return 11;
    copies.at(2)->click();
    if (QApplication::clipboard()->text() != large) return 12;
    copies.at(0)->click();
    if (QApplication::clipboard()->text() != five) return 13;
    const auto formats = shortEditor->document()->firstBlock().layout()->formats();
    if (formats.isEmpty() || formats.first().format.foreground().color() != QColor(QStringLiteral("#7956d5")))
        return 14;

    const int initialWidth = wideEditor->viewport()->width();
    window.showMaximized();
    waitEvents();
    if (!window.isMaximized() || !fits(shortEditor) || shortEditor->verticalScrollBar()->isVisible()) return 15;
    window.showNormal();
    window.resize(820, 630);
    if (!waitUntil([&] { return wideEditor->viewport()->width() < initialWidth &&
        wideEditor->horizontalScrollBar()->isVisible(); })) return 16;
    if (!longEditor->horizontalScrollBar()->isVisible() || !longEditor->verticalScrollBar()->isVisible())
        return 21;
    if (!fits(wideEditor) || wideEditor->verticalScrollBar()->maximum() != 0 ||
        wideEditor->toPlainText() != wide || wideEditor->document()->firstBlock().layout()->lineCount() != 1)
        return 17;
    wideEditor->horizontalScrollBar()->setValue(wideEditor->horizontalScrollBar()->maximum());
    if (wideEditor->horizontalScrollBar()->value() <= 0) return 18;
    const int narrowWidth = multi->width();
    window.resize(1440, 860);
    if (!waitUntil([&] { return multi->width() > narrowWidth && noScroll(wideEditor) &&
        fits(wideEditor) && noScroll(shortEditor) && fits(shortEditor); })) return 19;
    for (const auto *editor : editors)
        if (editor->mapTo(multi, QPoint()).x() + editor->width() > multi->width()) return 20;
    return 0;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       QDir::tempPath() + QStringLiteral("/VChatUiTest"));
    QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
    if (app.arguments().value(1) == QStringLiteral("--room-welcome-toast"))
        return testRoomWelcomeToast(app.arguments().value(2));
    if (app.arguments().value(1) == QStringLiteral("--message-notifications"))
        return testMessageNotifications(app.arguments().value(2));
    if (app.arguments().value(1) == QStringLiteral("--all-mention"))
        return testAllMention();
    if (app.arguments().value(1) == QStringLiteral("--empty-room-state"))
        return testEmptyRoomState(app.arguments().value(2));
    if (app.arguments().value(1) == QStringLiteral("--scoped-room-history"))
        return testScopedRoomHistory();
    if (app.arguments().value(1) == QStringLiteral("--vivi-code-layout"))
        return testViviCodeLayout(app.arguments().value(2));
    if (app.arguments().value(1) == QStringLiteral("--verify-notification-settings")) {
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, app.arguments().value(2));
        MainWindow window(QStringLiteral("用户B"));
        auto *sound = window.findChild<QAction *>(QStringLiteral("messageSoundSetting"));
        return sound && !sound->isChecked() ? 0 : 1;
    }
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
    QStringList chatCommands;
    QStringList roomCommands;
    QObject::connect(&server, &QTcpServer::newConnection, &app, [&]() {
        QTcpSocket *peer = server.nextPendingConnection();
        peers.append(peer);
        auto *buffer = new QByteArray;
        QObject::connect(peer, &QObject::destroyed, &app, [buffer]() { delete buffer; });
        QObject::connect(peer, &QTcpSocket::readyRead, peer, [peer, buffer, &clientCommands,
                                                               &chatCommands, &roomCommands]() {
            buffer->append(peer->readAll());
            int end = -1;
            while ((end = buffer->indexOf('\n')) >= 0) {
                const QByteArray line = buffer->left(end);
                buffer->remove(0, end + 1);
                if (line.startsWith("AI_CANCEL|") || line.startsWith("AI_REGENERATE|"))
                    clientCommands.append(QString::fromUtf8(line));
                if (line.startsWith("CHAT2|")) chatCommands.append(QString::fromUtf8(line));
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
    auto *readingCard = qobject_cast<QFrame *>(secondA->parentWidget());
    if (!readingCard || readingCard->objectName() != QStringLiteral("viviReadingCard")) return 150;
    auto *shortBar = readingCard->parentWidget()->findChild<QWidget *>(QStringLiteral("viviActionBar"));
    if (!shortBar || readingCard->isAncestorOf(shortBar) ||
        shortBar->parentWidget() != readingCard->parentWidget() ||
        readingCard->height() > secondA->height() + 45 ||
        shortBar->y() < readingCard->y() + readingCard->height() ||
        shortBar->y() > readingCard->y() + readingCard->height() + 16 ||
        !readingCard->findChildren<QToolButton *>().isEmpty() ||
        copyFull->toolButtonStyle() != Qt::ToolButtonTextBesideIcon) return 156;
    const QString layoutCapture = qEnvironmentVariable("VCHAT_UI_CAPTURE_DIR");
    if (!layoutCapture.isEmpty()) {
        const QSize originalSize = a.size();
        a.resize(1200, 780);
        waitEvents();
        a.grab().save(QDir(layoutCapture).filePath(QStringLiteral("vivi-short-v042.png")));
        a.resize(originalSize);
        waitEvents();
    }
    QToolButton *upVote = nullptr;
    QToolButton *downVote = nullptr;
    for (QToolButton *button : shortBar->findChildren<QToolButton *>()) {
        if (button->toolTip() == QStringLiteral("有帮助")) upVote = button;
        if (button->toolTip() == QStringLiteral("没有帮助")) downVote = button;
    }
    if (!upVote || !downVote) return 151;
    upVote->click();
    if (!upVote->isChecked() || downVote->isChecked()) return 152;
    downVote->click();
    if (!downVote->isChecked() || upVote->isChecked()) return 153;
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
    QTextBrowser *longProse = longBody->findChild<QTextBrowser *>();
    QWidget *longCard = longBody->parentWidget();
    QWidget *longBar = longCard->parentWidget()->findChild<QWidget *>(QStringLiteral("viviActionBar"));
    if (!waitUntil([&] { return longCard->height() >= longBody->y() + longBody->height(); }, 3000))
        return 157;
    if (longProse->verticalScrollBar()->maximum() != 0) return 154;
    if (longBody->y() + longBody->height() > longCard->height()) return 225;
    if (!waitUntil([&] { return longCard->height() <= longBody->height() + 45; }, 3000)) return 226;
    if (longCard->height() > longCard->sizeHint().height() + 2) return 229;
    if (!longBar || longBar->y() < longCard->y() + longCard->height() ||
        longBar->y() > longCard->y() + longCard->height() + 16) return 227;
    for (QToolButton *action : longBar->findChildren<QToolButton *>())
        if (action->isVisible() &&
            action->mapTo(longBar->parentWidget(), QPoint(0, 0)).y() <
                longCard->y() + longCard->height())
            return 155;
    if (!layoutCapture.isEmpty()) {
        const QSize originalSize = a.size();
        a.resize(1200, 780);
        waitEvents();
        scrollA->setValue(scrollA->maximum());
        waitEvents();
        a.grab().save(QDir(layoutCapture).filePath(QStringLiteral("vivi-long-v042.png")));
        a.resize(originalSize);
        waitEvents();
        scrollA->setValue(0);
    }
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
    auto retryFor = [](MainWindow &window, const QString &messageId) -> QToolButton * {
        for (QWidget *row : window.findChildren<QWidget *>())
            if (row->property("aiMessageId").toString() == messageId)
                return row->findChild<QToolButton *>(QStringLiteral("viviActionRegenerate"));
        return nullptr;
    };
    if (!retry->isVisible() || !retryFor(b, QStringLiteral("reply-1")) ||
        retryFor(b, QStringLiteral("reply-1"))->isVisible() ||
        !retryFor(b, QStringLiteral("reply-2"))->isVisible() ||
        retryFor(a, QStringLiteral("reply-2"))->isVisible()) return 214;
    QWidget *otherBar = retryFor(b, QStringLiteral("reply-1"))->parentWidget();
    auto *otherCopy = otherBar->findChild<QToolButton *>(QString(), Qt::FindDirectChildrenOnly);
    auto *otherLike = otherBar->findChild<QToolButton *>(QStringLiteral("viviActionLike"));
    if (!otherCopy || !otherLike || otherLike->x() -
        (otherCopy->x() + otherCopy->width()) > 18) return 218;
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
        {"reply_to_message_id", "saved-question"}, {"reply_to_name", QStringLiteral("甲")},
        {"original_request_id", "123"}, {"can_regenerate", true}};
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
    if (!retryFor(latest, QStringLiteral("saved-answer")) ||
        retryFor(latest, QStringLiteral("saved-answer"))->isVisible()) return 215;
    // A reconnect during generation receives the persisted pending row first,
    // then AiRoom's live snapshot. Both must update the same bubble.
    MainWindow resumed(QStringLiteral("甲"), address, nullptr, 11, QStringLiteral("test-e"));
    resumed.show();
    waitEvents(120);
    if (peers.size() != 5) return 41;
    peers.last()->write(historyFrame(question));
    peers.last()->write(historyFrame(answer));
    if (!waitUntil([&] { return retryFor(resumed, QStringLiteral("saved-answer")) &&
        retryFor(resumed, QStringLiteral("saved-answer"))->isVisible() &&
        retryFor(resumed, QStringLiteral("saved-answer"))->isEnabled(); })) return 216;
    retryFor(resumed, QStringLiteral("saved-answer"))->click();
    if (!waitUntil([&] { return clientCommands.contains(QStringLiteral("AI_REGENERATE|123")); })) return 217;
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
    const QSize shortReplyOriginalSize = a.size();
    a.resize(1200, 780);
    waitEvents();
    const QString shortSentence = QStringLiteral("你好，有什么可以帮你的吗？");
    broadcast(event("ai_start", 100, "short-reply-100", 22, QStringLiteral("乙"), 0));
    broadcast(event("ai_complete", 100, "short-reply-100", 22, QStringLiteral("乙"), 1,
                    shortSentence));
    QWidget *shortRow = nullptr;
    if (!waitUntil([&] {
        for (QWidget *widget : a.findChildren<QWidget *>())
            if (widget->property("aiMessageId").toString() == QStringLiteral("short-reply-100"))
                shortRow = widget;
        return shortRow && shortRow->findChild<AiMarkdownWidget *>()->plainText() == shortSentence;
    })) return 219;
    auto *shortBody = shortRow->findChild<AiMarkdownWidget *>();
    auto *shortProse = shortBody->findChild<QTextBrowser *>();
    auto *shortBubble = qobject_cast<QFrame *>(shortBody->parentWidget());
    if (!shortProse || !shortBubble) return 220;
    auto *shortActions = shortBubble->parentWidget()->findChild<QWidget *>(QStringLiteral("viviActionBar"));
    if (!shortActions) return 220;
    QToolButton *shortCopy = nullptr;
    for (QToolButton *button : shortActions->findChildren<QToolButton *>())
        if (button->text() == QStringLiteral("复制全文")) shortCopy = button;
    if (!shortCopy) return 220;
    if (!waitUntil([&] {
        return shortProse->document()->firstBlock().layout()->lineCount() == 1;
    })) return 221;
    if (shortBubble->width() >= 400) return 222;
    if (!waitUntil([&] { return shortBubble->height() <= shortBubble->sizeHint().height() + 2; })) return 228;
    const int visibleBottom = shortBubble->height() - shortBody->y() - shortBody->height()
        + shortBody->trailingTextSpace();
    if (qAbs(shortBody->y() - visibleBottom) > 2) return 248;
    auto *shortColumn = shortBubble->parentWidget();
    auto *shortAvatar = qobject_cast<QLabel *>(shortRow->layout()->itemAt(0)->widget());
    if (!shortAvatar || !shortRow->toolTip().isEmpty() ||
        !shortColumn->toolTip().isEmpty() || !shortAvatar->toolTip().isEmpty() ||
        !shortBody->toolTip().isEmpty() || !shortProse->toolTip().isEmpty() ||
        !shortActions->toolTip().isEmpty() ||
        !QDateTime::fromString(shortBubble->toolTip(),
            QStringLiteral("yyyy-MM-dd HH:mm:ss")).isValid()) return 249;
    a.raise();
    a.activateWindow();
    scrollA->setValue(scrollA->maximum());
    waitEvents(100);
    QToolTip::hideText();
    QCursor::setPos(shortBubble->mapToGlobal(shortBubble->rect().center()));
    QWidget *hovered = shortProse->viewport();
    QHelpEvent help(QEvent::ToolTip, hovered->mapFromGlobal(QCursor::pos()), QCursor::pos());
    QApplication::sendEvent(hovered, &help);
    if (!waitUntil([] { return QToolTip::isVisible(); }, 2500)) return 250;
    QCursor::setPos(shortAvatar->mapToGlobal(shortAvatar->rect().center()));
    QEvent leaveBubble(QEvent::Leave);
    QApplication::sendEvent(shortBubble, &leaveBubble);
    if (!waitUntil([] { return !QToolTip::isVisible(); })) return 251;
    QHelpEvent avatarHelp(QEvent::ToolTip,
        shortAvatar->rect().center(), shortAvatar->mapToGlobal(shortAvatar->rect().center()));
    QApplication::sendEvent(shortAvatar, &avatarHelp);
    waitEvents(200);
    if (QToolTip::isVisible()) return 259;
    const QPoint emptyPoint(shortRow->width() - 2, shortRow->height() / 2);
    QHelpEvent backgroundHelp(QEvent::ToolTip, emptyPoint, shortRow->mapToGlobal(emptyPoint));
    QApplication::sendEvent(shortRow, &backgroundHelp);
    waitEvents(200);
    if (QToolTip::isVisible()) return 260;
    if (qAbs(shortCopy->mapTo(shortBubble->parentWidget(), QPoint()).x() - shortBubble->x()) > 2)
        return 223;
    if (shortProse->verticalScrollBar()->maximum() != 0) return 224;
    a.resize(shortReplyOriginalSize);
    if (qEnvironmentVariableIsSet("VCHAT_AI_UI_ONLY")) {
        if (argc > 2) {
            a.resize(1200, 780);
            waitEvents();
            if (!waitUntil([&] { return shortBubble->height() <= shortBubble->sizeHint().height() + 2; }))
                return 230;
            if (!waitUntil([&] {
                return shortBubble->height() <= shortBody->y() + shortProse->height() + 15;
            })) return 231;
            if (shortProse->document()->firstBlock().layout()->lineCount() != 1) return 232;
            if (!waitUntil([&] {
                const int chrome = shortProse->height() - shortProse->viewport()->height();
                return shortProse->height() <= qCeil(shortProse->document()->documentLayout()
                    ->documentSize().height()) + chrome + 4;
            })) return 233;
            scrollA->setValue(scrollA->maximum());
            waitEvents();
            a.grab().save(QString::fromLocal8Bit(argv[2]));
        }
        a.resize(1200, 780);
        broadcast(event("ai_start", 101, "stream-width-101", 22, QStringLiteral("乙"), 0));
        QWidget *streamRow = nullptr;
        if (!waitUntil([&] {
            for (QWidget *widget : a.findChildren<QWidget *>())
                if (widget->property("aiMessageId").toString() == QStringLiteral("stream-width-101"))
                    streamRow = widget;
            return streamRow != nullptr;
        })) return 234;
        auto *streamBody = streamRow->findChild<AiMarkdownWidget *>();
        auto *streamBubble = qobject_cast<QFrame *>(streamBody->parentWidget());
        const int initialWidth = streamBubble->width();
        const QString streamedText(80, QChar(0x4e2d));
        broadcast(event("ai_chunk", 101, "stream-width-101", 22, QStringLiteral("乙"), 1,
                        streamedText));
        if (!waitUntil([&] { return streamBody->plainText().size() >= 20; }, 10000) ||
            streamBubble->width() <= initialWidth + 30) return 235;
        broadcast(event("ai_complete", 101, "stream-width-101", 22, QStringLiteral("乙"), 1,
                        streamedText));
        if (!waitUntil([&] { return streamBody->plainText() == streamedText &&
            streamBody->findChild<QTextBrowser *>() != nullptr; }, 10000)) return 236;
        MainWindow timeline(QStringLiteral("甲"), address, nullptr, 11,
                            QStringLiteral("timeline-test"));
        timeline.show();
        if (!waitUntil([&] { return peers.size() == 6; })) return 242;
        QTcpSocket *timelinePeer = peers.last();
        const QDate today = QDate::currentDate();
        const auto stamp = [](const QDate &date, const QTime &clock) {
            return QDateTime(date, clock, QTimeZone::systemTimeZone())
                .toString(Qt::ISODateWithMs);
        };
        const QVector<QPair<QDate, QTime>> moments{
            {today.addDays(-1), QTime(20, 30)},
            {today.addDays(-1), QTime(23, 59)},
            {today, QTime(0, 1)},
            {today, QTime(9, 15)},
            {today, QTime(9, 17)},
            {today, QTime(9, 18)},
            {today, QTime(9, 24)}};
        for (int index = 0; index < moments.size(); ++index) {
            const QJsonObject item{{"room_id", "class-room"},
                {"message_id", QStringLiteral("timeline-%1").arg(index)},
                {"sender_id", "22"}, {"sender", QStringLiteral("乙")},
                {"type", "user"}, {"text", QStringLiteral("时间测试消息")},
                {"time", stamp(moments.at(index).first, moments.at(index).second)}};
            timelinePeer->write(historyFrame(item));
        }
        if (!waitUntil([&] {
            return timeline.findChildren<QLabel *>(QStringLiteral("chatTimeLabel")).size() == 5;
        })) return 243;
        QStringList timeTexts;
        for (QLabel *label : timeline.findChildren<QLabel *>(QStringLiteral("chatTimeLabel")))
            timeTexts.append(label->text());
        if (timeTexts != QStringList{QStringLiteral("%1月%2日 20:30")
                                         .arg(today.addDays(-1).month(), 2, 10, QLatin1Char('0'))
                                         .arg(today.addDays(-1).day(), 2, 10, QLatin1Char('0')),
                                     QStringLiteral("%1月%2日 23:59")
                                         .arg(today.addDays(-1).month(), 2, 10, QLatin1Char('0'))
                                         .arg(today.addDays(-1).day(), 2, 10, QLatin1Char('0')),
                                     QStringLiteral("00:01"),
                                     QStringLiteral("09:15"),
                                     QStringLiteral("09:24")}) return 244;
        QWidget *lastMessage = nullptr;
        for (QWidget *widget : timeline.findChildren<QWidget *>())
            if (widget->property("userMessageId").toString() == QStringLiteral("timeline-6"))
                lastMessage = widget;
        QFrame *lastBubble = nullptr;
        if (lastMessage)
            for (QFrame *candidate : lastMessage->findChildren<QFrame *>())
                if (candidate->property("messageTimeBubble").toBool()) lastBubble = candidate;
        if (!lastMessage || !lastMessage->toolTip().isEmpty() || !lastBubble ||
            !lastBubble->toolTip().endsWith(QStringLiteral("09:24:00"))) return 245;
        QLabel *userText = nullptr;
        for (QLabel *label : lastBubble->findChildren<QLabel *>())
            if (label->text() == QStringLiteral("时间测试消息")) userText = label;
        if (!userText || !userText->toolTip().isEmpty()) return 261;
        for (QLabel *label : timeline.findChildren<QLabel *>(QStringLiteral("chatTimeLabel")))
            if (!label->toolTip().isEmpty()) return 252;
        const QString timeCapture = qEnvironmentVariable("VCHAT_TIME_CAPTURE");
        if (!timeCapture.isEmpty()) {
            timeline.resize(1200, 780);
            waitEvents();
            timeline.grab().save(timeCapture);
        }
        timelinePeer->write("USERLIST2|W10=\n");
        waitEvents(100);
        auto *timelineInput = timeline.findChild<QTextEdit *>(QStringLiteral("messageInput"));
        auto *timelineSend = timeline.findChild<QPushButton *>(QStringLiteral("sendButton"));
        if (!timelineInput || !timelineSend) return 253;
        timelineInput->setPlainText(QStringLiteral("发送时间确认测试"));
        timelineSend->click();
        if (!waitUntil([&] { return !chatCommands.isEmpty(); })) return 254;
        const QString sentCommand = chatCommands.last();
        const QJsonObject sentPayload = QJsonDocument::fromJson(
            QByteArray::fromBase64(sentCommand.mid(6).toLatin1())).object();
        const QString sentId = sentPayload.value(QStringLiteral("messageId")).toString();
        QWidget *optimistic = nullptr;
        for (QWidget *widget : timeline.findChildren<QWidget *>())
            if (widget->property("userMessageId").toString() == sentId) optimistic = widget;
        if (sentId.isEmpty() || !optimistic ||
            !optimistic->property("pendingTimeConfirmation").toBool()) return 255;
        const QString confirmedTime = stamp(today, QTime(6, 38));
        const QJsonObject acknowledgement{{"room_id", "class-room"},
            {"sender", QStringLiteral("甲")}, {"senderId", "11"},
            {"messageId", sentId}, {"text", QStringLiteral("发送时间确认测试")},
            {"time", confirmedTime}};
        timelinePeer->write("CHAT2|" + QJsonDocument(acknowledgement)
            .toJson(QJsonDocument::Compact).toBase64() + "\n");
        if (!waitUntil([&] {
            return !optimistic->property("pendingTimeConfirmation").toBool() &&
                optimistic->property("originalTimestamp").toString() == confirmedTime;
        })) return 256;
        int confirmedRows = 0;
        QFrame *confirmedBubble = nullptr;
        for (QWidget *widget : timeline.findChildren<QWidget *>()) {
            if (widget->property("userMessageId").toString() == sentId) {
                ++confirmedRows;
                for (QFrame *candidate : widget->findChildren<QFrame *>())
                    if (candidate->property("messageTimeBubble").toBool()) confirmedBubble = candidate;
            }
        }
        if (confirmedRows != 1 || !confirmedBubble ||
            !confirmedBubble->toolTip().endsWith(QStringLiteral("06:38:00")) ||
            timeline.findChildren<QLabel *>(QStringLiteral("chatTimeLabel")).last()->text()
                != QStringLiteral("06:38")) return 257;
        const auto roomFrame = [](const QJsonObject &result) {
            return "ROOM|result|" + QJsonDocument(result).toJson(QJsonDocument::Compact).toBase64() + "\n";
        };
        timelinePeer->write("HISTORY_END\n");
        timelinePeer->write(roomFrame(QJsonObject{{"action", "list_mine"}, {"ok", true},
            {"rooms", QJsonArray{QJsonObject{{"room_id", "class-room"},
                                               {"room_name", QStringLiteral("VChat 大厅")}},
                                  QJsonObject{{"room_id", "room-two"},
                                              {"room_name", QStringLiteral("任务讨论")}}}}}));
        auto *timelineSidebar = timeline.findChild<RoomSidebarWidget *>();
        if (!waitUntil([&] {
            auto *rooms = timelineSidebar ? timelineSidebar->findChild<QListWidget *>(
                QStringLiteral("roomList")) : nullptr;
            return rooms && rooms->count() == 2;
        })) return 246;
        timelineSidebar->roomSelected(QStringLiteral("room-two"));
        timelinePeer->write(roomFrame(QJsonObject{{"action", "switch"}, {"ok", true},
                                            {"room_id", "room-two"}}));
        const QJsonObject nextRoomMessage{{"room_id", "room-two"},
            {"message_id", "timeline-next-room"}, {"sender_id", "22"},
            {"sender", QStringLiteral("乙")}, {"type", "user"},
            {"text", QStringLiteral("新聊天室消息")}, {"time", stamp(today, QTime(8, 0))}};
        timelinePeer->write(historyFrame(nextRoomMessage));
        if (!waitUntil([&] {
            const auto labels = timeline.findChildren<QLabel *>(QStringLiteral("chatTimeLabel"));
            return labels.size() == 1 && labels.first()->text() == QStringLiteral("08:00");
        })) return 247;
        return 0;
    }

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
