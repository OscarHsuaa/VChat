#include "roomstore.h"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSet>
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
    q.prepare(QStringLiteral("SELECT 1 FROM room_members m JOIN rooms r ON r.room_id=m.room_id "
                             "WHERE m.room_id=? AND m.user_id=? AND r.dissolved_at IS NULL LIMIT 1"));
    q.addBindValue(roomId); q.addBindValue(userId);
    return q.exec() && q.next();
}

QJsonObject RoomStore::details(qint64 userId, const QString &roomId, QString *error)
{
    if (!isMember(roomId, userId)) {
        *error = QStringLiteral("无权查看聊天室或聊天室已解散"); return {};
    }
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT r.room_id,r.room_name,r.description,r.room_type,r.owner_id,"
        "r.created_at,u.nickname,(SELECT COUNT(*) FROM room_members m WHERE m.room_id=r.room_id) "
        "FROM rooms r LEFT JOIN users u ON u.id=r.owner_id WHERE r.room_id=? AND r.dissolved_at IS NULL"));
    q.addBindValue(roomId);
    if (!q.exec() || !q.next()) { *error = QStringLiteral("聊天室详情读取失败"); return {}; }
    return {{QStringLiteral("room_id"), q.value(0).toString()},
            {QStringLiteral("room_name"), q.value(1).toString()},
            {QStringLiteral("description"), q.value(2).toString()},
            {QStringLiteral("room_type"), q.value(3).toString()},
            {QStringLiteral("owner_id"), q.value(4).toString()},
            {QStringLiteral("created_at"), q.value(5).toDateTime().toString(Qt::ISODateWithMs)},
            {QStringLiteral("owner_name"), q.value(6).toString()},
            {QStringLiteral("member_count"), q.value(7).toInt()}};
}

QJsonObject RoomStore::updateDetails(qint64 actorId, const QString &roomId,
                                      const QString &name, const QString &description, QString *error)
{
    const QString cleanName = name.trimmed();
    const QString cleanDescription = description.trimmed();
    if (cleanName.toUcs4().size() < 2 || cleanName.toUcs4().size() > 30 ||
        cleanDescription.toUcs4().size() > 200) {
        *error = QStringLiteral("聊天室名称应为 2～30 字，简介不能超过 200 字"); return {};
    }
    QSqlQuery owner;
    owner.prepare(QStringLiteral("SELECT 1 FROM rooms WHERE room_id=? AND owner_id=? "
                                 "AND room_id<>'class-room' AND dissolved_at IS NULL"));
    owner.addBindValue(roomId); owner.addBindValue(actorId);
    if (!owner.exec() || !owner.next()) {
        *error = QStringLiteral("只有房主可以修改聊天室信息"); return {};
    }
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE rooms SET room_name=?,description=? "
        "WHERE room_id=? AND owner_id=? AND room_id<>'class-room' AND dissolved_at IS NULL"));
    q.addBindValue(cleanName); q.addBindValue(cleanDescription); q.addBindValue(roomId); q.addBindValue(actorId);
    if (!q.exec()) { *error = QStringLiteral("修改聊天室信息失败"); return {}; }
    return details(actorId, roomId, error);
}

bool RoomStore::removeMember(qint64 actorId, const QString &roomId, qint64 targetId, QString *error)
{
    if (roomId == QString::fromLatin1(LobbyId) || targetId <= 0 || targetId == actorId) {
        *error = QStringLiteral("不能移除房主或大厅成员"); return false;
    }
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) { *error = QStringLiteral("数据库事务无法启动"); return false; }
    QSqlQuery room;
    room.prepare(QStringLiteral("SELECT owner_id FROM rooms WHERE room_id=? AND dissolved_at IS NULL FOR UPDATE"));
    room.addBindValue(roomId);
    if (!room.exec() || !room.next() || room.value(0).toLongLong() != actorId) {
        db.rollback(); *error = QStringLiteral("只有房主可以移除成员"); return false;
    }
    QSqlQuery member;
    member.prepare(QStringLiteral("DELETE FROM room_members WHERE room_id=? AND user_id=? AND role='member'"));
    member.addBindValue(roomId); member.addBindValue(targetId);
    if (!member.exec() || member.numRowsAffected() != 1 || !db.commit()) {
        db.rollback(); *error = QStringLiteral("该用户已不是聊天室普通成员"); return false;
    }
    return true;
}

bool RoomStore::dissolve(qint64 actorId, const QString &roomId, QString *error)
{
    if (roomId == QString::fromLatin1(LobbyId)) {
        *error = QStringLiteral("不能解散 VChat 大厅"); return false;
    }
    QSqlQuery q;
    q.prepare(QStringLiteral("UPDATE rooms SET dissolved_at=NOW(3) "
                             "WHERE room_id=? AND owner_id=? AND dissolved_at IS NULL"));
    q.addBindValue(roomId); q.addBindValue(actorId);
    if (!q.exec() || q.numRowsAffected() != 1) {
        *error = QStringLiteral("只有房主可以解散未解散的聊天室"); return false;
    }
    return true;
}

QJsonArray RoomStore::myRooms(qint64 userId)
{
    QJsonArray rooms;
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT r.room_id,r.room_name,r.description,r.room_type,r.owner_id "
        "FROM room_members m JOIN rooms r ON r.room_id=m.room_id WHERE m.user_id=? AND r.dissolved_at IS NULL "
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
        "WHERE r.room_type='public' AND r.dissolved_at IS NULL AND m.user_id IS NULL ORDER BY r.created_at,r.room_id"));
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
    if (!canInvite(actorId, roomId, error)) return {};
    const QString token=QString::number(QRandomGenerator::system()->generate64(),16).rightJustified(16,QLatin1Char('0'))+
        QString::number(QRandomGenerator::system()->generate64(),16).rightJustified(16,QLatin1Char('0'));
    QSqlQuery q;
    q.prepare(QStringLiteral("INSERT INTO room_invites(token_hash,room_id,created_by,expires_at) "
                             "SELECT ?,room_id,?,DATE_ADD(NOW(3),INTERVAL ? MINUTE) "
                             "FROM rooms WHERE room_id=? AND dissolved_at IS NULL"));
    q.addBindValue(tokenHash(token)); q.addBindValue(actorId); q.addBindValue(validMinutes); q.addBindValue(roomId);
    if (!q.exec() || q.numRowsAffected() != 1) { *error=QStringLiteral("创建邀请失败"); return {}; }
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
    QSqlQuery active;
    active.prepare(QStringLiteral("SELECT 1 FROM rooms WHERE room_id=? AND dissolved_at IS NULL FOR UPDATE"));
    active.addBindValue(roomId);
    if (!active.exec() || !active.next()) {
        db.rollback(); *error=QStringLiteral("聊天室已解散"); return {};
    }
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

bool RoomStore::canInvite(qint64 actorId, const QString &roomId, QString *error)
{
    if (actorId <= 0 || roomId.isEmpty()) {
        if (error) *error = QStringLiteral("账号或聊天室标识无效");
        return false;
    }
    QSqlQuery room;
    room.prepare(QStringLiteral("SELECT 1 FROM rooms WHERE room_id=? AND dissolved_at IS NULL LIMIT 1"));
    room.addBindValue(roomId);
    if (!room.exec()) {
        if (error) *error = QStringLiteral("聊天室权限检查失败");
        return false;
    }
    if (!room.next()) {
        if (error) *error = QStringLiteral("聊天室不存在");
        return false;
    }
    if (roomId == QString::fromLatin1(LobbyId)) {
        if (error) *error = QStringLiteral("VChat 大厅不支持定向邀请");
        return false;
    }
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT 1 FROM room_members m JOIN rooms r ON r.room_id=m.room_id "
                             "WHERE m.room_id=? AND m.user_id=? AND m.role='owner' "
                             "AND r.owner_id=? AND r.dissolved_at IS NULL LIMIT 1"));
    q.addBindValue(roomId);
    q.addBindValue(actorId);
    q.addBindValue(actorId);
    if (!q.exec()) {
        if (error) *error = QStringLiteral("聊天室权限检查失败");
        return false;
    }
    if (!q.next()) {
        if (error) *error = QStringLiteral("只有聊天室房主可以邀请成员");
        return false;
    }
    return true;
}

QJsonArray RoomStore::searchInvitees(qint64 actorId, const QString &roomId,
                                    const QString &query, QString *error)
{
    QJsonArray results;
    if (!canInvite(actorId, roomId, error)) {
        return results;
    }
    if (query.size() > 50) {
        *error = QStringLiteral("搜索关键词不能超过 50 字");
        return results;
    }
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT u.id,u.username,u.nickname,m.user_id,"
        "(i.status='pending' AND i.expires_at>NOW(3)) "
        "FROM users u LEFT JOIN room_members m ON m.room_id=? AND m.user_id=u.id "
        "LEFT JOIN room_direct_invites i ON i.room_id=? AND i.recipient_user_id=u.id "
        "WHERE u.id<>? AND (u.username LIKE ? OR u.nickname LIKE ?) "
        "ORDER BY u.nickname,u.id LIMIT 40"));
    const QString pattern = QStringLiteral("%") + query.trimmed() + QStringLiteral("%");
    q.addBindValue(roomId); q.addBindValue(roomId); q.addBindValue(actorId);
    q.addBindValue(pattern); q.addBindValue(pattern);
    if (!q.exec()) {
        *error = QStringLiteral("用户搜索失败；请确认已执行 004_direct_room_invites.sql");
        return {};
    }
    while (q.next()) {
        const bool pending = q.value(4).toBool();
        results.append(QJsonObject{{QStringLiteral("user_id"), q.value(0).toString()},
            {QStringLiteral("username"), q.value(1).toString()},
            {QStringLiteral("nickname"), q.value(2).toString()},
            {QStringLiteral("member"), !q.value(3).isNull()},
            {QStringLiteral("pending"), pending}});
    }
    return results;
}

QJsonArray RoomStore::pendingInvites(qint64 recipientId, QString *error)
{
    QJsonArray results;
    QSqlQuery q;
    q.prepare(QStringLiteral(
        "SELECT i.invite_id,i.room_id,r.room_name,r.description,u.nickname,i.created_at,i.expires_at "
        "FROM room_direct_invites i JOIN rooms r ON r.room_id=i.room_id "
        "JOIN users u ON u.id=i.invited_by "
        "WHERE i.recipient_user_id=? AND i.status='pending' AND i.expires_at>NOW(3) "
        "AND r.dissolved_at IS NULL "
        "ORDER BY i.created_at DESC,i.invite_id"));
    q.addBindValue(recipientId);
    if (!q.exec()) {
        *error = QStringLiteral("邀请列表读取失败；请确认已执行 004_direct_room_invites.sql");
        return {};
    }
    while (q.next()) results.append(QJsonObject{
        {QStringLiteral("invite_id"), q.value(0).toString()},
        {QStringLiteral("room_id"), q.value(1).toString()},
        {QStringLiteral("room_name"), q.value(2).toString()},
        {QStringLiteral("description"), q.value(3).toString()},
        {QStringLiteral("inviter"), q.value(4).toString()},
        {QStringLiteral("created_at"), q.value(5).toDateTime().toString(Qt::ISODateWithMs)},
        {QStringLiteral("expires_at"), q.value(6).toDateTime().toString(Qt::ISODateWithMs)}});
    return results;
}

QJsonArray RoomStore::sendDirectInvites(qint64 actorId, const QString &roomId,
                                       const QJsonArray &recipients, QString *error)
{
    QJsonArray sent;
    if (!canInvite(actorId, roomId, error)) {
        return sent;
    }
    if (recipients.isEmpty() || recipients.size() > 20) {
        *error = QStringLiteral("每次请选择 1～20 名用户");
        return sent;
    }
    QSet<qint64> ids;
    for (const QJsonValue &value : recipients) {
        bool valid = false;
        const qint64 id = value.toString().toLongLong(&valid);
        if (!valid || id <= 0 || id == actorId || ids.contains(id)) {
            *error = QStringLiteral("受邀用户列表包含无效或重复的账号");
            return {};
        }
        ids.insert(id);
    }
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) { *error = QStringLiteral("数据库事务无法启动"); return {}; }
    QSqlQuery roomLock;
    roomLock.prepare(QStringLiteral("SELECT owner_id FROM rooms WHERE room_id=? AND dissolved_at IS NULL FOR UPDATE"));
    roomLock.addBindValue(roomId);
    if (!roomLock.exec() || !roomLock.next() || roomLock.value(0).toLongLong() != actorId) {
        db.rollback(); *error = QStringLiteral("聊天室已解散或无邀请权限"); return {};
    }
    for (qint64 id : ids) {
        QSqlQuery user;
        user.prepare(QStringLiteral("SELECT 1 FROM users WHERE id=? LIMIT 1"));
        user.addBindValue(id);
        if (!user.exec() || !user.next() || isMember(roomId, id)) {
            db.rollback(); *error = QStringLiteral("用户不存在或已加入聊天室"); return {};
        }
        QSqlQuery previous;
        previous.prepare(QStringLiteral("SELECT status,expires_at>NOW(3) FROM room_direct_invites "
                                        "WHERE room_id=? AND recipient_user_id=? FOR UPDATE"));
        previous.addBindValue(roomId); previous.addBindValue(id);
        if (!previous.exec()) {
            db.rollback(); *error = QStringLiteral("邀请记录读取失败；请确认已执行 004_direct_room_invites.sql"); return {};
        }
        const bool existed = previous.next();
        if (existed && previous.value(0).toString() == QStringLiteral("pending") &&
            previous.value(1).toBool()) {
            db.rollback(); *error = QStringLiteral("所选用户已有待处理邀请"); return {};
        }
        const QString inviteId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QSqlQuery save;
        if (existed) {
            save.prepare(QStringLiteral("UPDATE room_direct_invites SET invite_id=?,invited_by=?,"
                "status='pending',created_at=NOW(3),expires_at=DATE_ADD(NOW(3),INTERVAL 7 DAY),"
                "responded_at=NULL WHERE room_id=? AND recipient_user_id=?"));
            save.addBindValue(inviteId); save.addBindValue(actorId);
            save.addBindValue(roomId); save.addBindValue(id);
        } else {
            save.prepare(QStringLiteral("INSERT INTO room_direct_invites "
                "(invite_id,room_id,recipient_user_id,invited_by,expires_at) "
                "VALUES(?,?,?,?,DATE_ADD(NOW(3),INTERVAL 7 DAY))"));
            save.addBindValue(inviteId); save.addBindValue(roomId);
            save.addBindValue(id); save.addBindValue(actorId);
        }
        if (!save.exec()) { db.rollback(); *error = QStringLiteral("邀请保存失败，请稍后重试"); return {}; }
        sent.append(QJsonObject{{QStringLiteral("invite_id"), inviteId},
            {QStringLiteral("user_id"), QString::number(id)}});
    }
    if (!db.commit()) { db.rollback(); *error = QStringLiteral("邀请提交失败"); return {}; }
    return sent;
}

bool RoomStore::respondDirectInvite(qint64 recipientId, const QString &inviteId,
                                    bool accept, QString *roomId, QString *error)
{
    if (recipientId <= 0 || inviteId.isEmpty()) {
        *error = QStringLiteral("邀请标识无效"); return false;
    }
    QSqlQuery preflight;
    preflight.prepare(QStringLiteral("SELECT room_id FROM room_direct_invites WHERE invite_id=?"));
    preflight.addBindValue(inviteId);
    if (!preflight.exec() || !preflight.next()) {
        *error = QStringLiteral("邀请不存在或无权处理"); return false;
    }
    const QString expectedRoom = preflight.value(0).toString();
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) { *error = QStringLiteral("数据库事务无法启动"); return false; }
    QSqlQuery roomLock;
    roomLock.prepare(QStringLiteral("SELECT 1 FROM rooms WHERE room_id=? AND dissolved_at IS NULL FOR UPDATE"));
    roomLock.addBindValue(expectedRoom);
    if (!roomLock.exec() || !roomLock.next()) {
        db.rollback(); *error = QStringLiteral("聊天室已解散"); return false;
    }
    QSqlQuery lookup;
    lookup.prepare(QStringLiteral("SELECT room_id,recipient_user_id,status,expires_at>NOW(3) "
                                  "FROM room_direct_invites WHERE invite_id=? FOR UPDATE"));
    lookup.addBindValue(inviteId);
    if (!lookup.exec() || !lookup.next() || lookup.value(0).toString() != expectedRoom ||
        lookup.value(1).toLongLong() != recipientId) {
        db.rollback(); *error = QStringLiteral("邀请不存在或无权处理"); return false;
    }
    const QString targetRoom = lookup.value(0).toString();
    if (lookup.value(2).toString() != QStringLiteral("pending") || !lookup.value(3).toBool()) {
        db.rollback(); *error = QStringLiteral("邀请已处理或已过期"); return false;
    }
    if (accept) {
        if (isMember(targetRoom, recipientId)) {
            db.rollback(); *error = QStringLiteral("已经是该聊天室成员"); return false;
        }
        QSqlQuery member;
        member.prepare(QStringLiteral("INSERT INTO room_members(room_id,user_id,role) VALUES(?,?,'member')"));
        member.addBindValue(targetRoom); member.addBindValue(recipientId);
        if (!member.exec()) { db.rollback(); *error = QStringLiteral("加入聊天室失败"); return false; }
    }
    QSqlQuery update;
    update.prepare(QStringLiteral("UPDATE room_direct_invites SET status=?,responded_at=NOW(3) "
                                  "WHERE invite_id=? AND status='pending'"));
    update.addBindValue(accept ? QStringLiteral("accepted") : QStringLiteral("declined"));
    update.addBindValue(inviteId);
    if (!update.exec() || update.numRowsAffected() != 1 || !db.commit()) {
        db.rollback(); *error = QStringLiteral("邀请处理失败"); return false;
    }
    *roomId = targetRoom;
    return true;
}

bool RoomStore::joinPublic(qint64 userId, const QString &roomId, QString *error)
{
    if (isMember(roomId,userId)) { *error=QStringLiteral("已经是该聊天室成员"); return false; }
    QSqlQuery q;
    q.prepare(QStringLiteral("INSERT INTO room_members(room_id,user_id,role) "
        "SELECT room_id,?,'member' FROM rooms WHERE room_id=? AND room_type='public' AND dissolved_at IS NULL"));
    q.addBindValue(userId); q.addBindValue(roomId);
    if (!q.exec() || q.numRowsAffected()!=1) { *error=QStringLiteral("公开聊天室不存在或无法加入"); return false; }
    return true;
}

bool RoomStore::leave(qint64 userId, const QString &roomId, QString *error)
{
    if (roomId==QString::fromLatin1(LobbyId)) { *error=QStringLiteral("不能退出默认大厅"); return false; }
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) { *error=QStringLiteral("数据库事务无法启动"); return false; }
    QSqlQuery room;
    room.prepare(QStringLiteral("SELECT 1 FROM rooms WHERE room_id=? AND dissolved_at IS NULL FOR UPDATE"));
    room.addBindValue(roomId);
    if (!room.exec() || !room.next()) {
        db.rollback(); *error=QStringLiteral("聊天室不存在或已解散"); return false;
    }
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM room_members WHERE room_id=? AND user_id=? AND role='member'"));
    q.addBindValue(roomId); q.addBindValue(userId);
    if (!q.exec() || q.numRowsAffected()!=1 || !db.commit()) {
        db.rollback(); *error=QStringLiteral("未加入聊天室，或房主不能直接退出"); return false;
    }
    return true;
}
