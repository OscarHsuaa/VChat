#include <QCoreApplication>
#include "airoom.h"
#include "aiservice.h"
#include <QDateTime>
#include <QDebug>
#include <QHostAddress>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QRegularExpression>
#include <QSettings>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSslSocket>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUuid>

namespace {
// 服务器监听端口。客户端连接服务器时填写：服务器IP:8080。
const quint16 kServerPort = 8080;

// MySQL 数据库连接配置。这里对应 Navicat 里的 vchat_demo 数据库。
const QString kDbHost = QStringLiteral("127.0.0.1");
const int kDbPort = 3306;
const QString kDbName = QStringLiteral("vchat_demo");
const QString kDbUser = QStringLiteral("root");
const QString kDbPassword = QStringLiteral("123456");

struct LoginSession { qint64 accountId; QString nickname; QDateTime issuedAt; };

// Qt Creator can keep an old process environment after the Windows user
// environment changes. Read the existing user-scoped variable only when the
// process did not inherit it; the secret is never stored in project settings.
void restoreDeepSeekKeyFromUserEnvironment()
{
#ifdef Q_OS_WIN
    if (!qEnvironmentVariableIsEmpty("DEEPSEEK_API_KEY")) return;
    QSettings userEnvironment(QStringLiteral("HKEY_CURRENT_USER\\Environment"), QSettings::NativeFormat);
    QByteArray key = userEnvironment.value(QStringLiteral("DEEPSEEK_API_KEY")).toString().trimmed().toUtf8();
    if (!key.isEmpty()) qputenv("DEEPSEEK_API_KEY", key);
    key.fill('\0');
#endif
}

// 把一个 socket 转成便于日志显示的名字，例如 192.168.1.8:52341。
QString socketName(QTcpSocket *socket)
{
    return QStringLiteral("%1:%2")
        .arg(socket->peerAddress().toString())
        .arg(socket->peerPort());
}

// 给客户端发送一行文本。客户端和服务器约定：每条消息都用换行符结尾。
void sendLine(QTcpSocket *socket, const QString &message)
{
    socket->write(message.toUtf8());
    socket->write("\n");
    socket->flush();
}

// 启动服务器时先连接 MySQL。连接失败就不继续启动聊天服务。
bool openDatabase()
{
    qDebug() << "Available SQL drivers:" << QSqlDatabase::drivers();

    // Qt 连接 MySQL 需要 QMYSQL 驱动，如果这里没有 QMYSQL，说明驱动没有配置好。
    if (!QSqlDatabase::drivers().contains(QStringLiteral("QMYSQL"))) {
        qDebug() << "QMYSQL driver not found. Please install/copy the Qt MySQL SQL driver first.";
        return false;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QMYSQL"));
    db.setHostName(kDbHost);
    db.setPort(kDbPort);
    db.setDatabaseName(kDbName);
    db.setUserName(kDbUser);
    db.setPassword(kDbPassword);

    if (!db.open()) {
        qDebug() << "Database connection failed:" << db.lastError().text();
        return false;
    }

    qDebug() << "Database connected:" << kDbName;
    return true;
}

// 把聊天内容保存到 messages 表，方便之后在 Navicat 里查看聊天记录。
void saveMessageToDatabase(const QString &sender, const QString &content)
{
    if (!QSqlDatabase::database().isOpen()) {
        qDebug() << "Database is not open. Message was not saved.";
        return;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral(
        "INSERT INTO messages(sender, group_name, content, message_type) "
        "VALUES(?, ?, ?, ?)"));
    query.addBindValue(sender);
    query.addBindValue(QStringLiteral("班级群"));
    query.addBindValue(content);
    query.addBindValue(QStringLiteral("text"));

    if (!query.exec()) {
        qDebug() << "Save message failed:" << query.lastError().text();
    }
}

// 查询 users 表，检查账号密码是否正确。
bool authenticateUser(const QString &username, const QString &password, QString *nickname, qint64 *accountId)
{
    if (!QSqlDatabase::database().isOpen()) {
        qDebug() << "Database is not open. Login failed.";
        return false;
    }

    // 使用 prepare + addBindValue，可以避免直接拼接 SQL 字符串造成安全问题。
    QSqlQuery query;
    query.prepare(QStringLiteral(
        "SELECT id, nickname FROM users "
        "WHERE username = ? AND password = ?"));
    query.addBindValue(username);
    query.addBindValue(password);

    if (!query.exec()) {
        qDebug() << "Login query failed:" << query.lastError().text();
        return false;
    }

    // 没查到记录，说明账号或密码不正确。
    if (!query.next()) {
        return false;
    }

    // 查到记录后，把数据库里的 nickname 返回给客户端。
    *accountId = query.value(0).toLongLong();
    *nickname = query.value(1).toString();
    return true;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    restoreDeepSeekKeyFromUserEnvironment();

    qInfo() << "VChatServer Vivi AI support: enabled";
    qInfo() << "DeepSeek API key configured:"
            << !qgetenv("DEEPSEEK_API_KEY").trimmed().isEmpty();
    qInfo() << "Qt TLS available:" << QSslSocket::supportsSsl();

    // 数据库连接失败时直接退出，因为登录验证和聊天记录保存都依赖数据库。
    if (!openDatabase()) {
        qDebug() << "Server stopped because database connection failed.";
        return -1;
    }

    // QTcpServer 负责监听端口，有客户端连接进来时会触发 newConnection。
    QTcpServer server;

    // 保存所有已经建立 TCP 连接的客户端 socket。登录窗口的临时连接也会短暂出现在这里。
    QList<QTcpSocket *> clients;

    // 每个客户端都有自己的接收缓存，防止 TCP 半包导致一条消息被拆成几次收到。
    QMap<QTcpSocket *, QByteArray> buffers;

    // 只保存真正进入聊天窗口的客户端。
    // 登录窗口使用的是临时连接，不会加入这个在线列表。
    QMap<QTcpSocket *, QString> onlineUsers;
    QMap<QTcpSocket *, qint64> onlineUserIds;
    QSet<QTcpSocket *> modernPeers;
    QHash<QString, LoginSession> loginSessions;
    qint64 nextLegacySessionId = -1;

    AiService aiService(&app);
    QString aiState = aiService.isConfigured() ? QStringLiteral("online") : QStringLiteral("unavailable");
    AiRoom *roomForBroadcast = nullptr;
    auto broadcastAi = [&onlineUsers, &aiState, &roomForBroadcast, &aiService](const QString &state, quint64 id, const QString &text) {
        const bool streamEvent = state.startsWith(QStringLiteral("stream_"));
        if (state == QStringLiteral("thinking") || state == QStringLiteral("busy") ||
            state == QStringLiteral("stream_start")) aiState = QStringLiteral("thinking");
        else if (state == QStringLiteral("reply") || state == QStringLiteral("stream_complete")) aiState = QStringLiteral("online");
        else if (state == QStringLiteral("error") || state == QStringLiteral("stream_error"))
            aiState = text.startsWith(QStringLiteral("已取消")) && aiService.isConfigured()
                ? QStringLiteral("online") : QStringLiteral("unavailable");
        const QString eventName = state == QStringLiteral("stream_start") ? QStringLiteral("ai_start")
            : state == QStringLiteral("stream_chunk") ? QStringLiteral("ai_chunk")
            : state == QStringLiteral("stream_complete") ? QStringLiteral("ai_complete")
            : QStringLiteral("ai_error");
        const QString streamLine = streamEvent && roomForBroadcast
            ? QStringLiteral("AI|stream|%1|%2").arg(id).arg(QString::fromLatin1(
                QJsonDocument(roomForBroadcast->streamEvent(id, eventName, text))
                    .toJson(QJsonDocument::Compact).toBase64())) : QString();
        const QString line = QStringLiteral("AI|%1|%2|%3")
            .arg(state).arg(id).arg(QString::fromLatin1(text.toUtf8().toBase64()));
        const QString statusLine = QStringLiteral("AI|status|0|%1")
            .arg(QString::fromLatin1(aiState.toUtf8().toBase64()));
        for (QTcpSocket *member : onlineUsers.keys()) {
            if (member->state() == QAbstractSocket::ConnectedState) {
                if (streamEvent) {
                    sendLine(member, streamLine); // Older clients ignore the unknown AI state.
                    if (state == QStringLiteral("stream_chunk")) continue;
                    sendLine(member, statusLine);
                    continue;
                }
                if (state == QStringLiteral("reply") && roomForBroadcast) {
                    const AiRoom::ReplyTarget target = roomForBroadcast->replyTarget(id);
                    const QJsonObject meta{{QStringLiteral("accountId"), QString::number(target.accountId)},
                                           {QStringLiteral("nickname"), target.nickname}};
                    sendLine(member, QStringLiteral("AI|meta|%1|%2").arg(id).arg(
                        QString::fromLatin1(QJsonDocument(meta).toJson(QJsonDocument::Compact).toBase64())));
                }
                if (state != QStringLiteral("busy")) sendLine(member, line);
                sendLine(member, statusLine);
            }
        }
    };
    AiRoom aiRoom(&aiService, broadcastAi, &app); // Existing server has one room: 班级群.
    roomForBroadcast = &aiRoom;

    // 把当前在线用户列表广播给所有聊天窗口客户端。
    // 数据格式：USERLIST|张三,李四,王五
    auto broadcastUserList = [&]() {
        QStringList users;
        QJsonArray members;
        for (auto it = onlineUsers.cbegin(); it != onlineUsers.cend(); ++it) {
            if (!it.value().trimmed().isEmpty()) {
                users << it.value().trimmed();
                members.append(QJsonObject{{QStringLiteral("id"), QString::number(onlineUserIds.value(it.key()))},
                    {QStringLiteral("name"), it.value().trimmed()}});
            }
        }

        const QString payload = QStringLiteral("USERLIST|%1").arg(users.join(','));
        const QString modernPayload = QStringLiteral("USERLIST2|%1")
            .arg(QString::fromLatin1(QJsonDocument(members).toJson(QJsonDocument::Compact).toBase64()));
        for (QTcpSocket *client : clients) {
            if (client->state() == QAbstractSocket::ConnectedState) {
                sendLine(client, payload);
                if (modernPeers.contains(client)) sendLine(client, modernPayload);
            }
        }
    };

    // 有新的客户端连接到服务器时执行。
    QObject::connect(&server, &QTcpServer::newConnection, [&]() {
        while (server.hasPendingConnections()) {
            QTcpSocket *client = server.nextPendingConnection();
            clients.append(client);
            buffers.insert(client, QByteArray());

            const QString clientName = socketName(client);
            qDebug() << "Client connected:" << clientName;
            sendLine(client, QStringLiteral("[System] Connected to chat server"));

            // 某个客户端发来数据时执行。这里统一处理登录、上线和聊天消息。
            QObject::connect(client, &QTcpSocket::readyRead, [client, &clients, &buffers, &onlineUsers,
                &onlineUserIds, &modernPeers, &loginSessions, &nextLegacySessionId,
                &broadcastUserList, &aiRoom, &aiState]() {
                // TCP 数据不一定一次收到完整一行，所以先放进该客户端自己的缓存。
                QByteArray &buffer = buffers[client];
                buffer.append(client->readAll());

                int lineEnd = -1;

                // 只要缓存中存在换行符，就说明至少有一条完整消息可以处理。
                while ((lineEnd = buffer.indexOf('\n')) != -1) {
                    QByteArray line = buffer.left(lineEnd).trimmed();
                    buffer.remove(0, lineEnd + 1);

                    if (line.isEmpty()) {
                        continue;
                    }

                    const QString text = QString::fromUtf8(line);

                    // 登录请求格式：LOGIN|username|password。
                    // 登录请求只查 users 表，不加入在线列表，也不广播给其他客户端。
                    if (text.startsWith(QStringLiteral("LOGIN|"))) {
                        const QStringList parts = text.split('|');
                        if (parts.size() != 3) {
                            sendLine(client, QStringLiteral("LOGIN_FAIL|登录请求格式错误"));
                            continue;
                        }

                        QString nickname;
                        qint64 accountId = 0;
                        if (authenticateUser(parts.at(1), parts.at(2), &nickname, &accountId)) {
                            for (auto it = loginSessions.begin(); it != loginSessions.end();) {
                                if (it.value().issuedAt.secsTo(QDateTime::currentDateTimeUtc()) > 86400)
                                    it = loginSessions.erase(it);
                                else ++it;
                            }
                            const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
                            loginSessions.insert(token, {accountId, nickname, QDateTime::currentDateTimeUtc()});
                            sendLine(client, QStringLiteral("LOGIN_META|%1|%2").arg(accountId).arg(token));
                            sendLine(client, QStringLiteral("LOGIN_OK|%1").arg(nickname));
                            qDebug() << "Login success:" << parts.at(1) << nickname;
                        } else {
                            sendLine(client, QStringLiteral("LOGIN_FAIL|账号或密码错误"));
                            qDebug() << "Login failed:" << parts.at(1);
                        }
                        continue;
                    }

                    // 聊天窗口连接成功后会发送 JOIN|nickname。
                    // 服务器收到 JOIN 后，才把这个 socket 算作在线用户。
                    if (text.startsWith(QStringLiteral("JOIN|"))) {
                        const QStringList parts = text.split(QLatin1Char('|'));
                        if (parts.size() != 2 && parts.size() != 3) continue;
                        QString nickname = parts.at(1).trimmed();
                        qint64 accountId = nextLegacySessionId--;
                        if (parts.size() == 3) {
                            auto session = loginSessions.find(parts.at(2));
                            const bool valid = session != loginSessions.end() &&
                                session.value().issuedAt.secsTo(QDateTime::currentDateTimeUtc()) <= 86400 &&
                                (session.value().nickname == nickname ||
                                 (onlineUsers.contains(client) && onlineUserIds.value(client) == session.value().accountId));
                            if (valid) {
                                accountId = session.value().accountId;
                                session.value().nickname = nickname;
                                session.value().issuedAt = QDateTime::currentDateTimeUtc();
                                modernPeers.insert(client);
                            } else {
                                // A reconnect after a server restart retains the old chat behavior.
                                // It does not receive ID-bearing mention metadata until a fresh login.
                                modernPeers.remove(client);
                                sendLine(client, QStringLiteral("[System] 身份会话已失效，建议重新登录以使用成员提及"));
                            }
                        }
                        if (nickname.isEmpty()) {
                            nickname = socketName(client);
                        }

                        onlineUsers[client] = nickname;
                        onlineUserIds[client] = accountId;
                        sendLine(client, QStringLiteral("AI|status|0|%1")
                            .arg(QString::fromLatin1(aiState.toUtf8().toBase64())));
                        for (const QJsonObject &event : aiRoom.snapshots())
                            sendLine(client, QStringLiteral("AI|stream|%1|%2")
                                .arg(event.value(QStringLiteral("request_id")).toString())
                                .arg(QString::fromLatin1(QJsonDocument(event)
                                    .toJson(QJsonDocument::Compact).toBase64())));
                        qDebug() << "User joined:" << nickname;
                        sendLine(client, QStringLiteral("[System] Welcome %1").arg(nickname));

                        // 通知其他已经在线的用户：有新用户加入。
                        for (QTcpSocket *other : clients) {
                            if (other != client && other->state() == QAbstractSocket::ConnectedState && onlineUsers.contains(other)) {
                                sendLine(other, QStringLiteral("[System] %1 joined the chat").arg(nickname));
                            }
                        }

                        broadcastUserList();
                        continue;
                    }

                    if (text.startsWith(QStringLiteral("AI_CANCEL|"))) {
                        bool validId = false;
                        const quint64 requestId = text.mid(10).toULongLong(&validId);
                        if (!onlineUsers.contains(client) || !validId ||
                            !aiRoom.cancel(requestId, onlineUserIds.value(client)))
                            sendLine(client, QStringLiteral("[System] 无法取消：请求不存在或不属于当前账号"));
                        continue;
                    }

                    if (text.startsWith(QStringLiteral("AI_REGENERATE|"))) {
                        bool validId = false;
                        const quint64 originalId = text.mid(14).toULongLong(&validId);
                        // Only the account that asked the original question may replace its shared answer.
                        if (!onlineUsers.contains(client) || !validId ||
                            !aiRoom.regenerate(originalId, onlineUserIds.value(client)))
                            sendLine(client, QStringLiteral("AI|error|0|%1")
                                .arg(QString::fromLatin1(QStringLiteral("无法重新生成：请求不可用、正在生成或不属于当前账号。")
                                    .toUtf8().toBase64())));
                        continue;
                    }

                    if (text.startsWith(QStringLiteral("CHAT2|"))) {
                        if (!modernPeers.contains(client) || !onlineUsers.contains(client)) continue;
                        const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromBase64(text.mid(6).toLatin1()));
                        if (!document.isObject()) continue;
                        const QJsonObject request = document.object();
                        const QString content = request.value(QStringLiteral("text")).toString();
                        if (content.trimmed().isEmpty() || content.contains(QLatin1Char('\n')) || content.size() > 4000) continue;
                        QString userMessageId = request.value(QStringLiteral("messageId")).toString();
                        if (QUuid(userMessageId).isNull())
                            userMessageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
                        QJsonArray validated;
                        bool invalid = false;
                        for (const QJsonValue &value : request.value(QStringLiteral("mentions")).toArray()) {
                            const QJsonObject entry = value.toObject();
                            const QString type = entry.value(QStringLiteral("type")).toString();
                            const QString id = entry.value(QStringLiteral("id")).toString();
                            const QString name = entry.value(QStringLiteral("name")).toString();
                            const int start = entry.value(QStringLiteral("start")).toInt(-1);
                            const int length = entry.value(QStringLiteral("length")).toInt(-1);
                            if (start < 0 || length < 2 || start + length > content.size() ||
                                content.mid(start, length) != QStringLiteral("@") + name) { invalid = true; break; }
                            if (type == QStringLiteral("ai")) {
                                if (id != QStringLiteral("vivi") || name != QStringLiteral("Vivi")) { invalid = true; break; }
                            } else if (type == QStringLiteral("user")) {
                                bool ok = false;
                                const qint64 targetId = id.toLongLong(&ok);
                                bool found = false;
                                for (auto it = onlineUserIds.cbegin(); it != onlineUserIds.cend(); ++it)
                                    if (ok && targetId != onlineUserIds.value(client) && it.value() == targetId &&
                                        it.key() != client && onlineUsers.value(it.key()) == name)
                                        found = true;
                                if (!found) { invalid = true; break; }
                            } else { invalid = true; break; }
                            validated.append(entry);
                        }
                        if (invalid) {
                            sendLine(client, QStringLiteral("[System] 提及对象已离线或发生变化，请重新选择"));
                            continue;
                        }
                        const QString sender = onlineUsers.value(client);
                        const QString time = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));
                        const QString legacyLine = QStringLiteral("[%1] %2: %3").arg(time, sender, content);
                        QJsonObject outgoing{{QStringLiteral("sender"), sender},
                            {QStringLiteral("senderId"), QString::number(onlineUserIds.value(client))},
                            {QStringLiteral("messageId"), userMessageId},
                            {QStringLiteral("text"), content}, {QStringLiteral("time"), time},
                            {QStringLiteral("mentions"), validated}};
                        const QString modernLine = QStringLiteral("CHAT2|%1")
                            .arg(QString::fromLatin1(QJsonDocument(outgoing).toJson(QJsonDocument::Compact).toBase64()));
                        saveMessageToDatabase(sender, content);
                        for (QTcpSocket *other : clients)
                            if (other != client && other->state() == QAbstractSocket::ConnectedState && onlineUsers.contains(other))
                                sendLine(other, modernPeers.contains(other) ? modernLine : legacyLine);
                        bool askVivi = false;
                        for (const QJsonValue &value : validated)
                            if (value.toObject().value(QStringLiteral("type")).toString() == QStringLiteral("ai")) askVivi = true;
                        static const QRegularExpression mentionVivi(
                            QStringLiteral(R"((?:^|\s)(@Vivi)(?=$|\s|[,，:：]))"),
                            QRegularExpression::CaseInsensitiveOption);
                        auto matches = mentionVivi.globalMatch(content);
                        while (matches.hasNext()) {
                            const auto match = matches.next();
                            bool ordinaryVivi = false;
                            for (const QJsonValue &value : validated) {
                                const QJsonObject entry = value.toObject();
                                if (entry.value(QStringLiteral("type")).toString() == QStringLiteral("user") &&
                                    entry.value(QStringLiteral("start")).toInt() == match.capturedStart(1)) ordinaryVivi = true;
                            }
                            if (!ordinaryVivi) askVivi = true;
                        }
                        aiRoom.addUserMessage(onlineUserIds.value(client), sender, content, askVivi, userMessageId);
                        continue;
                    }

                    // 走到这里说明既不是 LOGIN，也不是 JOIN，而是一条普通聊天消息。
                    const QString time = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));
                    const QString sender = onlineUsers.value(client, socketName(client));
                    const QString serverMessage = QStringLiteral("[%1] %2").arg(time, text);

                    qDebug() << "Chat message received: characters=" << text.size();
                    saveMessageToDatabase(sender, text);

                    // 转发给其他已经进入聊天窗口的用户，不发回给发送者本人。
                    for (QTcpSocket *other : clients) {
                        if (other != client && other->state() == QAbstractSocket::ConnectedState && onlineUsers.contains(other)) {
                            sendLine(other, serverMessage);
                        }
                    }
                    if (onlineUsers.contains(client)) {
                        const QString prefix = sender + QStringLiteral(": ");
                        const QString content = text.startsWith(prefix) ? text.mid(prefix.size()) : text;
                        static const QRegularExpression mention(
                            QStringLiteral(R"((?:^|\s)@Vivi(?=$|\s|[,，:：]))"),
                            QRegularExpression::CaseInsensitiveOption);
                        aiRoom.addUserMessage(onlineUserIds.value(client), sender, content,
                                              mention.match(content).hasMatch(),
                                              QUuid::createUuid().toString(QUuid::WithoutBraces));
                    }
                }
            });

            // 客户端断开连接时，清理列表和缓存，并刷新在线用户列表。
            QObject::connect(client, &QTcpSocket::disconnected, [client, &clients, &buffers, &onlineUsers,
                &onlineUserIds, &modernPeers, &broadcastUserList]() {
                const QString clientName = socketName(client);
                const bool wasOnline = onlineUsers.contains(client);
                const QString nickname = onlineUsers.value(client);

                qDebug() << "Client disconnected:" << clientName;

                clients.removeOne(client);
                buffers.remove(client);
                onlineUsers.remove(client);
                onlineUserIds.remove(client);
                modernPeers.remove(client);

                if (wasOnline) {
                    for (QTcpSocket *other : clients) {
                        if (other->state() == QAbstractSocket::ConnectedState && onlineUsers.contains(other)) {
                            sendLine(other, QStringLiteral("[System] %1 left the chat").arg(nickname));
                        }
                    }
                    broadcastUserList();
                }

                client->deleteLater();
            });
        }
    });

    // QHostAddress::Any 表示监听本机所有网卡地址。
    // 局域网其他电脑可以用这台电脑的 IPv4:8080 连接。
    if (!server.listen(QHostAddress::Any, kServerPort)) {
        qDebug() << "Server start failed:" << server.errorString();
        return -1;
    }

    qDebug() << "Chat server started";
    qDebug() << "Port:" << kServerPort;
    qDebug() << "Local test: 127.0.0.1:8080";
    qDebug() << "LAN test: <server IPv4>:8080";

    return app.exec();
}
