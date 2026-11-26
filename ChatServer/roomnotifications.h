#ifndef ROOMNOTIFICATIONS_H
#define ROOMNOTIFICATIONS_H

#include <QJsonArray>
#include <QSet>
#include <QString>

namespace RoomNotifications {
bool available();
bool saveMentions(qint64 sequence, const QSet<qint64> &targets);
QSet<qint64> recipients(const QString &roomId);
bool snapshot(qint64 accountId, QJsonArray *rooms, const QString &roomId = {});
bool markRead(qint64 accountId, const QString &roomId, qint64 sequence);
}

#endif
