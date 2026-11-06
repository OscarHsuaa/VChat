#pragma once

#include "aiservice.h"
#include <QJsonArray>
#include <QHash>
#include <QQueue>
#include <functional>
#include <utility>

class AiRoom : public QObject
{
public:
    struct ReplyTarget { qint64 accountId = 0; QString nickname; };
    using Broadcast = std::function<void(const QString &state, quint64 id, const QString &text)>;
    using HistorySink = std::function<void(const QString &role, const QString &content)>;
    explicit AiRoom(AiService *service, Broadcast broadcast, QObject *parent = nullptr);
    void addUserMessage(qint64 accountId, const QString &nickname, const QString &content, bool askVivi);
    bool regenerate(quint64 originalId);
    ReplyTarget replyTarget(quint64 requestId) const { return requestTargets.value(requestId); }
    void setHistorySink(HistorySink sink) { historySink = std::move(sink); }

private:
    struct Pending { quint64 id; ReplyTarget target; QString question; QJsonArray context; };
    void startNext();
    void appendHistory(const QString &role, const QString &content);
    AiService *service;
    Broadcast broadcast;
    HistorySink historySink;
    QJsonArray history;
    QQueue<Pending> pending;
    QHash<quint64, QJsonArray> requestContexts;
    QHash<quint64, ReplyTarget> requestTargets;
    QQueue<quint64> requestOrder;
    quint64 nextId = 1;
    bool busy = false;
};
