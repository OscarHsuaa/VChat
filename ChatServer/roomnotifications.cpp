#include "roomnotifications.h"

#include <QDebug>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>

bool RoomNotifications::available()
{
    QSqlQuery query;
    return query.exec(QStringLiteral(
        "SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() "
        "AND TABLE_NAME='room_members' AND COLUMN_NAME='last_read_sequence_id' AND IS_NULLABLE='NO' "
        "AND EXISTS (SELECT 1 FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() "
        "AND TABLE_NAME='chat_message_mentions')")) && query.next();
}

bool RoomNotifications::saveMentions(qint64 sequence, const QSet<qint64> &targets)
{
    if (sequence <= 0) return false;
    QSqlQuery query;
    query.prepare(QStringLiteral("INSERT INTO chat_message_mentions(sequence_id,user_id) VALUES(?,?)"));
    for (qint64 account : targets) {
        query.bindValue(0, sequence);
        query.bindValue(1, account);
        if (!query.exec()) {
            qWarning() << "Message mention insert failed:" << query.lastError().text();
            return false;
        }
    }
    return true;
}

QSet<qint64> RoomNotifications::recipients(const QString &roomId)
{
    QSqlQuery query;
    query.prepare(QStringLiteral("SELECT m.user_id FROM room_members m JOIN rooms r ON r.room_id=m.room_id "
                                 "WHERE m.room_id=? AND r.dissolved_at IS NULL"));
    query.addBindValue(roomId);
    QSet<qint64> accounts;
    if (!query.exec()) qWarning() << "Message notification membership lookup failed:" << query.lastError().text();
    else while (query.next()) accounts.insert(query.value(0).toLongLong());
    return accounts;
}

bool RoomNotifications::snapshot(qint64 accountId, QJsonArray *rooms, const QString &roomId)
{
    QSqlQuery query;
    query.prepare(QStringLiteral(
        "SELECT m.room_id,m.last_read_sequence_id,COUNT(h.sequence_id),COUNT(n.user_id),MAX(h.sequence_id) "
        "FROM room_members m JOIN rooms r ON r.room_id=m.room_id AND r.dissolved_at IS NULL "
        "LEFT JOIN chat_history h ON h.room_id=m.room_id AND h.sequence_id>m.last_read_sequence_id "
        "AND h.created_at>=m.joined_at AND h.message_type='user' AND h.visibility='team' "
        "AND NOT(h.sender_id <=> m.user_id) "
        "LEFT JOIN chat_message_mentions n ON n.sequence_id=h.sequence_id AND n.user_id=m.user_id "
        "WHERE m.user_id=? %1 GROUP BY m.room_id,m.last_read_sequence_id")
        .arg(roomId.isEmpty() ? QString() : QStringLiteral("AND m.room_id=?")));
    query.addBindValue(accountId);
    if (!roomId.isEmpty()) query.addBindValue(roomId);
    if (!query.exec()) {
        qWarning() << "Unread totals lookup failed:" << query.lastError().text();
        return false;
    }
    while (query.next()) {
        const qint64 read = query.value(1).toLongLong();
        rooms->append(QJsonObject{{QStringLiteral("room_id"), query.value(0).toString()},
            {QStringLiteral("read_sequence_id"), QString::number(read)},
            {QStringLiteral("through_sequence_id"), QString::number(qMax(read, query.value(4).toLongLong()))},
            {QStringLiteral("unread"), query.value(2).toLongLong()},
            {QStringLiteral("mentions"), query.value(3).toLongLong()}});
    }
    return true;
}

bool RoomNotifications::markRead(qint64 accountId, const QString &roomId, qint64 sequence)
{
    QSqlQuery query;
    query.prepare(QStringLiteral(
        "UPDATE room_members m JOIN rooms r ON r.room_id=m.room_id AND r.dissolved_at IS NULL "
        "JOIN chat_history h ON h.room_id=m.room_id AND h.sequence_id=? "
        "AND h.message_type='user' AND h.visibility='team' "
        "SET m.last_read_sequence_id=GREATEST(m.last_read_sequence_id,h.sequence_id) "
        "WHERE m.room_id=? AND m.user_id=?"));
    query.addBindValue(sequence);
    query.addBindValue(roomId);
    query.addBindValue(accountId);
    if (!query.exec()) {
        qWarning() << "Read position update failed:" << query.lastError().text();
        return false;
    }
    return true;
}
