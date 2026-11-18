#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QUuid>
#include <cstdio>
#include <functional>

namespace {
bool send(QTcpSocket &socket, const QByteArray &line)
{
    return socket.write(line + '\n') >= 0 && socket.waitForBytesWritten(3000);
}

QByteArray readLine(QTcpSocket &socket, QByteArray &buffer, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        const int end = buffer.indexOf('\n');
        if (end >= 0) {
            const QByteArray line = buffer.left(end).trimmed();
            buffer.remove(0, end + 1);
            return line;
        }
        if (socket.waitForReadyRead(qMax(1, timeoutMs - int(timer.elapsed()))))
            buffer += socket.readAll();
    }
    return {};
}

QByteArray until(QTcpSocket &socket, QByteArray &buffer,
                 const std::function<bool(const QByteArray &)> &matches, int timeoutMs = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        const QByteArray line = readLine(socket, buffer, qMax(1, timeoutMs - int(timer.elapsed())));
        if (line.isEmpty()) break;
        if (matches(line)) return line;
    }
    return {};
}

QJsonObject decoded(const QByteArray &line, int prefixLength)
{
    return QJsonDocument::fromJson(QByteArray::fromBase64(line.mid(prefixLength))).object();
}

bool connectTo(QTcpSocket &socket, quint16 port)
{
    for (int attempt = 0; attempt < 30; ++attempt) {
        socket.connectToHost(QHostAddress::LocalHost, port);
        if (socket.waitForConnected(500)) return true;
        socket.abort();
        QThread::msleep(100);
    }
    return false;
}

bool loginAndJoin(QTcpSocket &socket, QByteArray &buffer, quint16 port,
                  const QByteArray &username, const QByteArray &password,
                  const std::function<bool(const QJsonObject &)> &onHistory = {})
{
    if (!connectTo(socket, port) || !send(socket, "LOGIN|" + username + '|' + password)) return false;
    QByteArray token, nickname;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 5000 && (token.isEmpty() || nickname.isEmpty())) {
        const QByteArray line = readLine(socket, buffer, 5000 - int(timer.elapsed()));
        if (line.startsWith("LOGIN_META|")) token = line.split('|').value(2);
        if (line.startsWith("LOGIN_OK|")) nickname = line.split('|').value(1);
        if (line.startsWith("LOGIN_FAIL|")) return false;
    }
    if (token.isEmpty() || nickname.isEmpty() ||
        !send(socket, "JOIN|" + nickname + '|' + token)) return false;
    bool foundHistory = false;
    const QByteArray end = until(socket, buffer, [&](const QByteArray &line) {
        if (line.startsWith("HISTORY|") && onHistory)
            foundHistory |= onHistory(decoded(line, 8));
        return line == "HISTORY_END";
    });
    return end == "HISTORY_END" && (!onHistory || foundHistory);
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QString password = qEnvironmentVariable("VCHAT_TEST_DB_PASSWORD");
    const QString serverExe = qEnvironmentVariable("VCHAT_SERVER_EXE");
    if (password.isEmpty() || serverExe.isEmpty()) return 1;

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QMYSQL"));
    db.setHostName(QStringLiteral("127.0.0.1"));
    db.setPort(3306);
    db.setDatabaseName(QStringLiteral("vchat_demo"));
    db.setUserName(QStringLiteral("root"));
    db.setPassword(password);
    if (!db.open()) return 2;
    const QString dbName = QStringLiteral("vchat_time_verify_")
        + QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QLatin1Char('-')).left(12);
    if (!QRegularExpression(QStringLiteral("^vchat_time_verify_[a-f0-9]{12}$"))
             .match(dbName).hasMatch()) return 3;

    QProcess server;
    server.setProgram(serverExe);
    server.setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
    server.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif
    auto stopServer = [&] {
        if (server.state() == QProcess::NotRunning) return;
        server.terminate();
        if (!server.waitForFinished(3000)) {
            server.kill();
            server.waitForFinished(3000);
        }
    };
    const auto run = [&]() -> int {
        QSqlQuery query(db);
        if (!query.exec(QStringLiteral("CREATE DATABASE `%1`").arg(dbName))) return 4;
        if (!query.exec(QStringLiteral("SELECT TABLE_NAME FROM information_schema.TABLES "
            "WHERE TABLE_SCHEMA='vchat_demo' AND TABLE_TYPE='BASE TABLE'"))) return 5;
        QStringList tables;
        while (query.next()) tables.append(query.value(0).toString());
        for (const QString &table : tables) {
            if (!QRegularExpression(QStringLiteral("^[A-Za-z0-9_]+$"))
                     .match(table).hasMatch()) return 6;
            if (!query.exec(QStringLiteral("CREATE TABLE `%1`.`%2` LIKE `vchat_demo`.`%2`")
                .arg(dbName, table))) return 7;
        }
        if (!query.exec(QStringLiteral("INSERT INTO `%1`.rooms(room_id,room_name,description,room_type) "
            "VALUES('class-room','Time test lobby','','public')").arg(dbName))) return 8;

        QTcpServer portProbe;
        if (!portProbe.listen(QHostAddress::LocalHost, 0)) return 9;
        const quint16 port = portProbe.serverPort();
        portProbe.close();
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.remove(QStringLiteral("VCHAT_TEST_DB_PASSWORD"));
        environment.insert(QStringLiteral("VCHAT_TEST_DB"), dbName);
        environment.insert(QStringLiteral("VCHAT_TEST_PORT"), QString::number(port));
        server.setProcessEnvironment(environment);
        const auto start = [&]() {
            server.start();
            return server.waitForStarted(5000);
        };
        if (!start()) return 10;

        QTcpSocket registration;
        QByteArray registrationBuffer;
        if (!connectTo(registration, port)) return 11;
        const QByteArray username = "time_" + QUuid::createUuid().toString(
            QUuid::WithoutBraces).left(8).toLatin1();
        const QByteArray userPassword = "timepass123";
        const QJsonObject registrationData{{QStringLiteral("username"), QString::fromLatin1(username)},
            {QStringLiteral("password"), QString::fromLatin1(userPassword)}};
        if (!send(registration, "REGISTER|" + QJsonDocument(registrationData)
            .toJson(QJsonDocument::Compact).toBase64()) ||
            until(registration, registrationBuffer, [](const QByteArray &line) {
                return line.startsWith("REGISTER_OK") || line.startsWith("REGISTER_FAIL");
            }) != "REGISTER_OK") return 12;
        registration.disconnectFromHost();

        QTcpSocket first;
        QByteArray firstBuffer;
        if (!loginAndJoin(first, firstBuffer, port, username, userPassword)) return 13;
        const QString messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QJsonObject outgoing{{QStringLiteral("text"), QStringLiteral("时间持久化验证")},
            {QStringLiteral("messageId"), messageId},
            {QStringLiteral("mentions"), QJsonArray{}}};
        if (!send(first, "CHAT2|" + QJsonDocument(outgoing)
            .toJson(QJsonDocument::Compact).toBase64())) return 14;
        const QByteArray echo = until(first, firstBuffer, [](const QByteArray &line) {
            return line.startsWith("CHAT2|");
        });
        const QJsonObject echoed = decoded(echo, 6);
        const QString liveTime = echoed.value(QStringLiteral("time")).toString();
        const QDateTime live = QDateTime::fromString(liveTime, Qt::ISODateWithMs);
        if (echoed.value(QStringLiteral("messageId")).toString() != messageId ||
            !live.isValid() || live.timeSpec() != Qt::UTC) return 15;

        QSqlQuery stored(db);
        stored.prepare(QStringLiteral("SELECT created_at FROM `%1`.chat_history WHERE message_id=?")
            .arg(dbName));
        stored.addBindValue(messageId);
        if (!stored.exec() || !stored.next()) return 16;
        const QString rawBefore = stored.value(0).toDateTime().toString(
            QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
        if (rawBefore != live.toLocalTime().toString(
            QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))) return 17;
        first.disconnectFromHost();
        stopServer();

        if (!start()) return 18;
        QTcpSocket restarted;
        QByteArray restartedBuffer;
        QString historyTime;
        if (!loginAndJoin(restarted, restartedBuffer, port, username, userPassword,
            [&](const QJsonObject &item) {
                if (item.value(QStringLiteral("message_id")).toString() != messageId) return false;
                historyTime = item.value(QStringLiteral("time")).toString();
                return true;
            })) return 19;
        if (historyTime != liveTime) return 20;
        if (!stored.exec() || !stored.next()) return 21;
        const QString rawAfter = stored.value(0).toDateTime().toString(
            QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
        if (rawAfter != rawBefore) return 22;
        std::printf("MESSAGE_TIME_E2E stored_local=%s live_utc=%s restart_same=true\n",
                    rawBefore.toUtf8().constData(), liveTime.toUtf8().constData());
        return 0;
    };
    const int result = run();
    stopServer();
    QSqlQuery cleanup(db);
    if (!cleanup.exec(QStringLiteral("DROP DATABASE `%1`").arg(dbName)))
        std::fprintf(stderr, "Temporary test database cleanup failed\n");
    return result;
}
