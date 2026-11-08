#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// Database-backed room permissions. Caller identity always comes from the
// validated JOIN session, never from fields supplied in a ROOM command.
class RoomStore
{
public:
    static constexpr const char *LobbyId = "class-room";
    static bool ensureLobbyMember(qint64 userId);
    static bool isMember(const QString &roomId, qint64 userId);
    static QJsonArray myRooms(qint64 userId);
    static QJsonArray publicRooms(qint64 userId);
    static QJsonArray members(const QString &roomId);
    static QJsonObject create(qint64 ownerId, const QString &name, const QString &description,
                              const QString &type, QString *error);
    static QString invite(qint64 actorId, const QString &roomId, int validMinutes, QString *error);
    static QString redeem(qint64 userId, const QString &token, QString *error);
    static bool joinPublic(qint64 userId, const QString &roomId, QString *error);
    static bool leave(qint64 userId, const QString &roomId, QString *error);
};
