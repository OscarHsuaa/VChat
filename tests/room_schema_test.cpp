#include "roomstore.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(QStringLiteral(":memory:"));
    if (!db.open()) return 1;
    const auto run = [](const QString &sql) {
        QSqlQuery q;
        if (q.exec(sql)) return true;
        qWarning() << q.lastError().text();
        return false;
    };
    const QStringList setup{
        QStringLiteral("CREATE TABLE users(id INTEGER PRIMARY KEY,nickname TEXT)"),
        QStringLiteral("CREATE TABLE rooms(room_id TEXT PRIMARY KEY,room_name TEXT,description TEXT,"
                       "room_type TEXT,owner_id INTEGER,created_at TEXT,dissolved_at TEXT)"),
        QStringLiteral("CREATE TABLE room_members(room_id TEXT,user_id INTEGER,role TEXT,joined_at TEXT)"),
        QStringLiteral("INSERT INTO users VALUES(11,'owner')"),
        QStringLiteral("INSERT INTO users VALUES(22,'member')"),
        QStringLiteral("INSERT INTO rooms VALUES('class-room','VChat lobby','','public',11,'2026-01-01',NULL)"),
        QStringLiteral("INSERT INTO rooms VALUES('private-room','Private','old room','invite_only',11,'2026-01-02',NULL)"),
        QStringLiteral("INSERT INTO rooms VALUES('public-room','Public','','public',11,'2026-01-03',NULL)"),
        QStringLiteral("INSERT INTO room_members VALUES('class-room',11,'member','2026-01-01')"),
        QStringLiteral("INSERT INTO room_members VALUES('private-room',11,'owner','2026-01-02')"),
        QStringLiteral("INSERT INTO room_members VALUES('private-room',22,'member','2026-01-03')")};
    for (const QString &sql : setup) if (!run(sql)) return 2;

    QString error;
    QJsonArray mine = RoomStore::myRooms(11, &error);
    if (!error.isEmpty() || mine.size() != 2 ||
        mine.at(1).toObject().value(QStringLiteral("room_id")) != QStringLiteral("private-room") ||
        mine.at(1).toObject().value(QStringLiteral("join_policy")) != QStringLiteral("direct")) return 3;
    if (RoomStore::details(11, QStringLiteral("private-room"), &error)
            .value(QStringLiteral("room_name")) != QStringLiteral("Private")) return 4;
    error.clear();
    if (!RoomStore::permissions(11, QStringLiteral("private-room"), &error).isEmpty() ||
        !error.contains(QStringLiteral("006_room_permissions.sql"))) return 5;
    if (!RoomStore::canManage(11, QStringLiteral("private-room"), QStringLiteral("invite")) ||
        RoomStore::canManage(22, QStringLiteral("private-room"), QStringLiteral("invite"))) return 12;
    error.clear();
    if (RoomStore::publicRooms(22, &error).size() != 2 || !error.isEmpty()) return 6;
    if (!run(QStringLiteral("ALTER TABLE rooms ADD COLUMN join_policy TEXT NOT NULL DEFAULT 'direct'")) ||
        !run(QStringLiteral("UPDATE rooms SET join_policy='approval' WHERE room_id='public-room'"))) return 14;
    error.clear();
    if (RoomStore::joinPublic(44, QStringLiteral("public-room"), &error)) return 15;
    if (!run(QStringLiteral("UPDATE rooms SET join_policy='direct' WHERE room_id='public-room'"))) return 16;

    const QStringList migration{
        QStringLiteral("ALTER TABLE rooms ADD COLUMN allow_member_invite INTEGER NOT NULL DEFAULT 0"),
        QStringLiteral("ALTER TABLE rooms ADD COLUMN allow_member_edit INTEGER NOT NULL DEFAULT 0"),
        QStringLiteral("ALTER TABLE rooms ADD COLUMN allow_member_kick INTEGER NOT NULL DEFAULT 0"),
        QStringLiteral("ALTER TABLE rooms ADD COLUMN allow_member_manage_permissions INTEGER NOT NULL DEFAULT 0"),
        QStringLiteral("UPDATE rooms SET allow_member_invite=1 WHERE room_id='private-room'")};
    for (const QString &sql : migration) if (!run(sql)) return 7;
    error.clear();
    mine = RoomStore::myRooms(11, &error);
    if (!error.isEmpty() || mine.size() != 2 ||
        !mine.at(1).toObject().value(QStringLiteral("allow_member_invite")).toBool()) return 8;
    if (!RoomStore::permissions(11, QStringLiteral("private-room"), &error)
            .value(QStringLiteral("allow_member_invite")).toBool()) return 9;
    if (!RoomStore::canManage(22, QStringLiteral("private-room"), QStringLiteral("invite")) ||
        RoomStore::canManage(22, QStringLiteral("private-room"), QStringLiteral("kick"))) return 13;
    if (!run(QStringLiteral("DROP TABLE room_members"))) return 10;
    error.clear();
    mine = RoomStore::myRooms(11, &error);
    if (!mine.isEmpty() || error.isEmpty()) return 11; // Query failure must not look like an empty membership.
    return 0;
}
