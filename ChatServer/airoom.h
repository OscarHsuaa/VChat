#pragma once

#include "aiservice.h"
#include <QJsonArray>
#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QQueue>
#include <QSet>
#include <QTimer>
#include <functional>
#include <utility>

class AiRoom : public QObject
{
public:
    enum class State { Pending, Streaming, Completed, Failed, Cancelled, TimedOut };
    struct ReplyTarget { qint64 accountId = 0; QString nickname; };
    using Broadcast = std::function<void(const QString &state, quint64 id, const QString &text)>;
    using HistorySink = std::function<void(const QString &role, const QString &content)>;
    explicit AiRoom(AiService *service, Broadcast broadcast, QObject *parent = nullptr,
                    int maxConcurrent = 3, int maxQueued = 12, int timeoutMs = 90000,
                    quint64 firstRequestId = 1);
    bool addUserMessage(qint64 accountId, const QString &nickname, const QString &content,
                        bool askVivi, const QString &replyToMessageId = {});
    bool regenerate(quint64 originalId, qint64 accountId);
    bool cancel(quint64 requestId, qint64 accountId);
    ReplyTarget replyTarget(quint64 requestId) const { return requestTargets.value(requestId); }
    QJsonObject streamEvent(quint64 requestId, const QString &state, const QString &text = {}) const;
    QList<QJsonObject> snapshots() const;
    bool hasWork() const { return !activeReplies.isEmpty() || !pending.isEmpty(); }
    void setHistorySink(HistorySink sink) { historySink = std::move(sink); }

private:
    struct Pending {
        quint64 id;
        ReplyTarget target;
        QString question;
        QJsonArray context;
        QString replyToMessageId;
        quint64 originalId = 0;
    };
    struct StreamInfo {
        ReplyTarget target;
        QString messageId;
        QString generationId;
        QString replyToMessageId;
        QString content;
        QString error;
        QString previousContent;
        quint64 originalId = 0;
        quint64 previousRequestId = 0;
        int sequence = 0;
        bool finished = false;
        State state = State::Pending;
    };
    void startNext();
    static QString stateName(State state);
    void appendHistory(const QString &role, const QString &content, const QString &messageId = {});
    AiService *service;
    Broadcast broadcast;
    HistorySink historySink;
    QJsonArray history;
    QStringList historyMessageIds;
    QQueue<Pending> pending;
    QHash<quint64, QJsonArray> requestContexts;
    QHash<quint64, ReplyTarget> requestTargets;
    QHash<quint64, StreamInfo> streams;
    QQueue<quint64> streamOrder;
    QHash<quint64, quint64> latestRequestIds;
    QSet<quint64> regenerating;
    QHash<quint64, QPointer<QNetworkReply>> activeReplies;
    QHash<quint64, QPointer<QTimer>> activeTimers;
    QSet<quint64> canceledRequests;
    QSet<quint64> timedOutRequests;
    quint64 nextId = 1;
    const int maxActive;
    const int maxPending;
    const int requestTimeoutMs;
};
