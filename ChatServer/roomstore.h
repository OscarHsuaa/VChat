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
    static QJsonObject details(qint64 userId, const QString &roomId, QString *error);
    static QJsonObject permissions(qint64 userId, const QString &roomId, QString *error);
    static QJsonObject updatePermissions(qint64 actorId, const QString &roomId,
                                         const QJsonObject &settings, QString *error);
    static bool canManage(qint64 actorId, const QString &roomId, const QString &permission,
                          QString *error = nullptr);
    static QJsonObject updateDetails(qint64 actorId, const QString &roomId,
                                     const QString &name, const QString &description, QString *error);
    static bool removeMember(qint64 actorId, const QString &roomId, qint64 targetId, QString *error);
    static bool dissolve(qint64 actorId, const QString &roomId, QString *error);
    static QJsonArray myRooms(qint64 userId, QString *error = nullptr);
    static QJsonArray publicRooms(qint64 userId, QString *error = nullptr);
    static QJsonArray members(const QString &roomId);
    static QJsonObject create(qint64 ownerId, const QString &name, const QString &description,
                              const QString &type, QString *error);
    static QString invite(qint64 actorId, const QString &roomId, int validMinutes, QString *error);
    static QString redeem(qint64 userId, const QString &token, QString *error);
    static bool canInvite(qint64 actorId, const QString &roomId, QString *error = nullptr);
    static QJsonArray searchInvitees(qint64 actorId, const QString &roomId,
                                    const QString &query, QString *error);
    static QJsonArray pendingInvites(qint64 recipientId, QString *error);
    static QJsonArray sendDirectInvites(qint64 actorId, const QString &roomId,
                                       const QJsonArray &recipients, QString *error);
    static bool respondDirectInvite(qint64 recipientId, const QString &inviteId,
                                    bool accept, QString *roomId, QString *error);
    static bool joinPublic(qint64 userId, const QString &roomId, QString *error);
    static bool leave(qint64 userId, const QString &roomId, QString *error);
};
