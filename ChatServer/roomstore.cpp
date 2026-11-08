#include "roomstore.h"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

namespace {
QString tokenHash(const QString &token)
{
    return QString::fromLatin1(QCryptographicHash::hash(token.toUtf8(), QCryptographicHash::Sha256).toHex());
}
QJsonObject roomRow(const QSqlQuery &q)
{
    return {{QStringLiteral("room_id"),q.value(0).toString()},
            {QStringLiteral("room_name"),q.value(1).toString()},
            {QStringLiteral("description"),q.value(2).toString()},
            {QStringLiteral("room_type"),q.value(3).toString()},
            {QStringLiteral("owner_id"),q.value(4).toString()}};
}
}

bool RoomStore::ensureLobbyMember(qint64 userId)
{
    if (userId <= 0) return false;
    QSqlQuery q;
    q.prepare(QStringLiteral("INSERT INTO room_members(room_id,user_id,role) "
                             "VALUES('class-room',?,'member') ON DUPLICATE KEY UPDATE user_id=user_id"));
    q.addBindValue(userId);
    return q.exec();
}

bool RoomStore::isMember(const QString &roomId, qint64 userId)
{
    if (userId <= 0 || roomId.isEmpty()) return false;
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT 1 FROM room_members WHERE room_id=? AND user_id=? LIMIT 1"));
    q.addBindValue(roomId); q.addBindValue(userId);
    return q.exec() && q.next();
}

QJsonArray RoomStore::myRooms(qint64 userId)
{
    QJsonArray rooms;
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT r.room_id,r.room_name,r.description,r.room_type,r.owner_id "
        "FROM room_members m JOIN rooms r ON r.room_id=m.room_id WHERE m.user_id=? "
        "ORDER BY (r.room_id='class-room') DESC,m.joined_at,r.room_id"));
    q.addBindValue(userId);
    if (q.exec()) while (q.next()) rooms.append(roomRow(q));
    return rooms;
}

QJsonArray RoomStore::publicRooms(qint64 userId)
{
    QJsonArray rooms;
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT r.room_id,r.room_name,r.description,r.room_type,r.owner_id "
        "FROM rooms r LEFT JOIN room_members m ON m.room_id=r.room_id AND m.user_id=? "
        "WHERE r.room_type='public' AND m.user_id IS NULL ORDER BY r.created_at,r.room_id"));
    q.addBindValue(userId);
    if (q.exec()) while (q.next()) rooms.append(roomRow(q));
    return rooms;
}

QJsonArray RoomStore::members(const QString &roomId)
{
    QJsonArray result;
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT m.user_id,u.nickname,m.role,m.joined_at FROM room_members m "
                             "JOIN users u ON u.id=m.user_id WHERE m.room_id=? "
                             "ORDER BY (m.role='owner') DESC,m.joined_at,m.user_id"));
    q.addBindValue(roomId);
    if (q.exec()) while (q.next()) result.append(QJsonObject{
        {QStringLiteral("user_id"),q.value(0).toString()},
        {QStringLiteral("nickname"),q.value(1).toString()},
        {QStringLiteral("role"),q.value(2).toString()},
        {QStringLiteral("joined_at"),q.value(3).toDateTime().toString(Qt::ISODateWithMs)}});
    return result;
}

QJsonObject RoomStore::create(qint64 ownerId, const QString &name, const QString &description,
                              const QString &type, QString *error)
{
    const QString cleanName=name.trimmed();
    QString cleanDescription=description.trimmed();
    if (cleanDescription.isNull()) cleanDescription=QStringLiteral("");
    if (ownerId<=0 || cleanName.isEmpty() || cleanName.size()>100 || cleanDescription.size()>500 ||
        (type!=QStringLiteral("invite_only") && type!=QStringLiteral("public"))) {
        *error=QStringLiteral("聊天室名称、简介或类型无效"); return {};
    }
    QSqlDatabase db=QSqlDatabase::database();
    if (!db.transaction()) { *error=QStringLiteral("数据库事务无法启动"); return {}; }
    const QString id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlQuery room;
    room.prepare(QStringLiteral("INSERT INTO rooms(room_id,room_name,description,owner_id,room_type) VALUES(?,?,?,?,?)"));
    room.addBindValue(id); room.addBindValue(cleanName); room.addBindValue(cleanDescription);
    room.addBindValue(ownerId); room.addBindValue(type);
    QSqlQuery member;
    member.prepare(QStringLiteral("INSERT INTO room_members(room_id,user_id,role) VALUES(?,?,'owner')"));
    member.addBindValue(id); member.addBindValue(ownerId);
    if (!room.exec() || !member.exec() || !db.commit()) {
        db.rollback(); *error=QStringLiteral("创建聊天室失败"); return {};
    }
    return {{QStringLiteral("room_id"),id},{QStringLiteral("room_name"),cleanName},
            {QStringLiteral("description"),cleanDescription},{QStringLiteral("room_type"),type},
            {QStringLiteral("owner_id"),QString::number(ownerId)}};
}

QString RoomStore::invite(qint64 actorId, const QString &roomId, int validMinutes, QString *error)
{
    if (validMinutes<1 || validMinutes>10080) { *error=QStringLiteral("邀请有效期应为 1～10080 分钟"); return {}; }
    QSqlQuery role;
    role.prepare(QStringLiteral("SELECT role FROM room_members WHERE room_id=? AND user_id=?"));
    role.addBindValue(roomId); role.addBindValue(actorId);
    if (!role.exec() || !role.next() || role.value(0).toString()!=QStringLiteral("owner")) {
        *error=QStringLiteral("只有聊天室创建者可以邀请成员"); return {};
    }
    const QString token=QString::number(QRandomGenerator::system()->generate64(),16).rightJustified(16,QLatin1Char('0'))+
        QString::number(QRandomGenerator::system()->generate64(),16).rightJustified(16,QLatin1Char('0'));
    QSqlQuery q;
    q.prepare(QStringLiteral("INSERT INTO room_invites(token_hash,room_id,created_by,expires_at) "
                             "VALUES(?,?,?,DATE_ADD(NOW(3),INTERVAL ? MINUTE))"));
    q.addBindValue(tokenHash(token)); q.addBindValue(roomId); q.addBindValue(actorId); q.addBindValue(validMinutes);
    if (!q.exec()) { *error=QStringLiteral("创建邀请失败"); return {}; }
    return token;
}

QString RoomStore::redeem(qint64 userId, const QString &token, QString *error)
{
    if (userId<=0 || token.size()!=32) { *error=QStringLiteral("邀请令牌无效"); return {}; }
    QSqlDatabase db=QSqlDatabase::database();
    if (!db.transaction()) { *error=QStringLiteral("数据库事务无法启动"); return {}; }
    QSqlQuery lookup;
    lookup.prepare(QStringLiteral("SELECT room_id FROM room_invites WHERE token_hash=? "
        "AND used_at IS NULL AND expires_at>NOW(3) FOR UPDATE"));
    lookup.addBindValue(tokenHash(token));
    if (!lookup.exec() || !lookup.next()) {
        db.rollback(); *error=QStringLiteral("邀请不存在、已使用或已过期"); return {};
    }
    const QString roomId=lookup.value(0).toString();
    if (isMember(roomId,userId)) { db.rollback(); *error=QStringLiteral("已经是该聊天室成员"); return {}; }
    QSqlQuery member;
    member.prepare(QStringLiteral("INSERT INTO room_members(room_id,user_id,role) VALUES(?,?,'member')"));
    member.addBindValue(roomId); member.addBindValue(userId);
    QSqlQuery consume;
    consume.prepare(QStringLiteral("UPDATE room_invites SET used_by=?,used_at=NOW(3) WHERE token_hash=? AND used_at IS NULL"));
    consume.addBindValue(userId); consume.addBindValue(tokenHash(token));
    if (!member.exec() || !consume.exec() || consume.numRowsAffected()!=1 || !db.commit()) {
        db.rollback(); *error=QStringLiteral("加入聊天室失败"); return {};
    }
    return roomId;
}

bool RoomStore::joinPublic(qint64 userId, const QString &roomId, QString *error)
{
    if (isMember(roomId,userId)) { *error=QStringLiteral("已经是该聊天室成员"); return false; }
    QSqlQuery q;
    q.prepare(QStringLiteral("INSERT INTO room_members(room_id,user_id,role) "
        "SELECT room_id,?,'member' FROM rooms WHERE room_id=? AND room_type='public'"));
    q.addBindValue(userId); q.addBindValue(roomId);
    if (!q.exec() || q.numRowsAffected()!=1) { *error=QStringLiteral("公开聊天室不存在或无法加入"); return false; }
    return true;
}

bool RoomStore::leave(qint64 userId, const QString &roomId, QString *error)
{
    if (roomId==QString::fromLatin1(LobbyId)) { *error=QStringLiteral("不能退出默认大厅"); return false; }
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM room_members WHERE room_id=? AND user_id=? AND role='member'"));
    q.addBindValue(roomId); q.addBindValue(userId);
    if (!q.exec() || q.numRowsAffected()!=1) { *error=QStringLiteral("未加入聊天室，或创建者不能直接退出"); return false; }
    return true;
}
