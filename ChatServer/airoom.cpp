#include "airoom.h"
#include <QJsonObject>
#include <QNetworkReply>
#include <QUuid>

AiRoom::AiRoom(AiService *aiService, Broadcast send, QObject *parent,
               int maxConcurrent, int maxQueued, int timeoutMs, quint64 firstRequestId)
    : QObject(parent), service(aiService), broadcast(std::move(send)),
      nextId(qMax<quint64>(1, firstRequestId)),
      maxActive(qMax(1, maxConcurrent)), maxPending(qMax(0, maxQueued)),
      requestTimeoutMs(qMax(100, timeoutMs)) {}

QString AiRoom::stateName(State state)
{
    switch (state) {
    case State::Pending: return QStringLiteral("pending");
    case State::Streaming: return QStringLiteral("streaming");
    case State::Completed: return QStringLiteral("completed");
    case State::Failed: return QStringLiteral("failed");
    case State::Cancelled: return QStringLiteral("cancelled");
    case State::TimedOut: return QStringLiteral("timed_out");
    }
    return {};
}

void AiRoom::appendHistory(const QString &role, const QString &content, const QString &messageId)
{
    history.append(QJsonObject{{QStringLiteral("role"), role}, {QStringLiteral("content"), content}});
    historyMessageIds.append(messageId);
    while (history.size() > 20) { history.removeAt(0); historyMessageIds.removeFirst(); }
    if (historySink) historySink(role, content);
}

bool AiRoom::addUserMessage(qint64 accountId, const QString &nickname, const QString &content,
                            bool askVivi, const QString &replyToMessageId)
{
    appendHistory(QStringLiteral("user"), QStringLiteral("[%1] %2").arg(nickname, content));
    if (!askVivi) return true;
    if (activeReplies.size() >= maxActive && pending.size() >= maxPending) return false;
    QJsonArray context;
    context.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
        {QStringLiteral("content"), QStringLiteral(
            "你是 VChat 聊天室的 Vivi 助手。以下是共享聊天室消息，方括号中是成员昵称。"
            "请区分成员身份，简洁回答当前指定成员的问题。回复正文直接开始回答，不要在开头重复成员昵称或方括号标记。")}});
    for (const QJsonValue &value : history) context.append(value);
    context.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
        {QStringLiteral("content"), QStringLiteral("请回答 [%1] 的这次提问：%2").arg(nickname, content)}});
    const Pending request{nextId++, {accountId, nickname}, content, context, replyToMessageId};
    requestTargets.insert(request.id, request.target);
    requestContexts.insert(request.id, context);
    streams.insert(request.id, {request.target,
        QUuid::createUuid().toString(QUuid::WithoutBraces),
        QUuid::createUuid().toString(QUuid::WithoutBraces), replyToMessageId,
        {}, {}, {}, request.id});
    streamOrder.enqueue(request.id);
    latestRequestIds.insert(request.id, request.id);
    pending.enqueue(request);
    broadcast(QStringLiteral("stream_start"), request.id, {});
    broadcast(QStringLiteral("thinking"), request.id, QStringLiteral("Vivi 正在思考…"));
    startNext();
    return true;
}

bool AiRoom::regenerate(quint64 originalId, qint64 accountId)
{
    if ((activeReplies.size() >= maxActive && pending.size() >= maxPending) ||
        !requestContexts.contains(originalId) || !latestRequestIds.contains(originalId) ||
        regenerating.contains(originalId) ||
        requestTargets.value(originalId).accountId != accountId) return false;
    const quint64 previousId = latestRequestIds.value(originalId);
    const StreamInfo previous = streams.value(previousId);
    if (!previous.finished || !previous.error.isEmpty() || previous.content.isEmpty()) return false;
    const Pending request{nextId++, requestTargets.value(originalId), {}, requestContexts.value(originalId),
                          previous.replyToMessageId, originalId};
    requestTargets.insert(request.id, request.target);
    streams.insert(request.id, {request.target,
        previous.messageId, QUuid::createUuid().toString(QUuid::WithoutBraces),
        request.replyToMessageId, {}, {}, previous.content, originalId, previousId});
    latestRequestIds.insert(originalId, request.id);
    regenerating.insert(originalId);
    pending.enqueue(request);
    broadcast(QStringLiteral("stream_start"), request.id, {});
    startNext();
    return true;
}

bool AiRoom::cancel(quint64 requestId, qint64 accountId)
{
    if (!streams.contains(requestId) || streams.value(requestId).target.accountId != accountId ||
        streams.value(requestId).finished) return false;
    if (activeReplies.contains(requestId)) {
        canceledRequests.insert(requestId);
        if (activeReplies.value(requestId)) activeReplies.value(requestId)->abort();
        return true;
    }
    for (auto it = pending.begin(); it != pending.end(); ++it) {
        if (it->id != requestId) continue;
        pending.erase(it);
        StreamInfo &info = streams[requestId];
        info.finished = true;
        info.state = State::Cancelled;
        info.error = QStringLiteral("已取消 Vivi 生成。");
        broadcast(QStringLiteral("stream_error"), requestId, info.error);
        if (info.originalId != requestId) {
            latestRequestIds.insert(info.originalId, info.previousRequestId);
            regenerating.remove(info.originalId);
        } else broadcast(QStringLiteral("error"), requestId, info.error);
        startNext();
        return true;
    }
    return false;
}

void AiRoom::cancelAll()
{
    QList<quint64> active;
    for (auto it = streams.cbegin(); it != streams.cend(); ++it)
        if (!it.value().finished) active.append(it.key());
    for (quint64 id : active) cancel(id, streams.value(id).target.accountId);
}

QJsonObject AiRoom::streamEvent(quint64 requestId, const QString &state, const QString &text) const
{
    const StreamInfo info = streams.value(requestId);
    QJsonObject event{{QStringLiteral("event"), state},
        {QStringLiteral("request_id"), QString::number(requestId)},
        {QStringLiteral("message_id"), info.messageId},
        {QStringLiteral("generation_id"), info.generationId},
        {QStringLiteral("original_request_id"), QString::number(info.originalId)},
        {QStringLiteral("regeneration"), info.originalId != requestId},
        {QStringLiteral("reply_to_message_id"), info.replyToMessageId},
        {QStringLiteral("user_id"), QString::number(info.target.accountId)},
        {QStringLiteral("nickname"), info.target.nickname},
        {QStringLiteral("sequence"), info.sequence}};
    event.insert(QStringLiteral("request_state"), stateName(info.state));
    if (state == QStringLiteral("ai_chunk")) event.insert(QStringLiteral("text"), text);
    if (state == QStringLiteral("ai_start") || state == QStringLiteral("ai_complete"))
        event.insert(QStringLiteral("full_text"), info.content);
    if (info.originalId != requestId && (state == QStringLiteral("ai_start") || state == QStringLiteral("ai_error")))
        event.insert(QStringLiteral("previous_text"), info.previousContent);
    if (state == QStringLiteral("ai_error")) event.insert(QStringLiteral("error"), info.error);
    return event;
}

QList<QJsonObject> AiRoom::snapshots() const
{
    QList<QJsonObject> result;
    for (quint64 id : streamOrder) {
        const quint64 latestId = latestRequestIds.value(id, id);
        const StreamInfo info = streams.value(latestId);
        result.append(streamEvent(latestId, info.finished
            ? info.error.isEmpty() ? QStringLiteral("ai_complete") : QStringLiteral("ai_error")
            : QStringLiteral("ai_start")));
    }
    return result;
}

void AiRoom::startNext()
{
    while (activeReplies.size() < maxActive && !pending.isEmpty()) {
    const Pending request = pending.dequeue();
    StreamInfo &starting = streams[request.id];
    starting.state = State::Streaming;
    activeReplies.insert(request.id, nullptr);
    broadcast(QStringLiteral("busy"), request.id, {});
    QNetworkReply *reply = service->stream(request.context, [this, id = request.id](const QString &chunk) {
        StreamInfo &info = streams[id];
        if (info.finished || canceledRequests.contains(id) || timedOutRequests.contains(id)) return;
        info.content += chunk;
        ++info.sequence;
        broadcast(QStringLiteral("stream_chunk"), id, chunk);
    }, [this, request](const QString &answer, const QString &error) {
        StreamInfo &info = streams[request.id];
        if (info.finished) return;
        const bool cancelled = canceledRequests.remove(request.id);
        const bool timedOut = timedOutRequests.remove(request.id);
        const QString finalError = cancelled ? QStringLiteral("已取消 Vivi 生成。")
            : timedOut ? QStringLiteral("AI 响应超时，请稍后重试。") : error;
        if (QTimer *timer = activeTimers.take(request.id)) { timer->stop(); timer->deleteLater(); }
        activeReplies.remove(request.id);
        if (finalError.isEmpty()) {
            QString body = answer;
            const QString prefix = QStringLiteral("[%1]").arg(request.target.nickname);
            if (!request.target.nickname.isEmpty() && body.startsWith(prefix)) {
                const QString remainder = body.mid(prefix.size());
                if (!remainder.isEmpty() && (remainder.front().isSpace() || remainder.front() == QLatin1Char(':') || remainder.front() == QChar(0xff1a)))
                    body = remainder.trimmed();
            }
            info.content = body;
            info.finished = true;
            info.state = State::Completed;
            if (request.originalId == 0) {
                appendHistory(QStringLiteral("assistant"), body, info.messageId);
            } else {
                const int index = historyMessageIds.indexOf(info.messageId);
                if (index >= 0) history.replace(index, QJsonObject{
                    {QStringLiteral("role"), QStringLiteral("assistant")},
                    {QStringLiteral("content"), body}});
                regenerating.remove(request.originalId);
            }
            broadcast(QStringLiteral("stream_complete"), request.id, body);
            if (request.originalId == 0) broadcast(QStringLiteral("reply"), request.id, body);
        } else {
            info.finished = true;
            info.state = cancelled ? State::Cancelled : timedOut ||
                finalError.contains(QStringLiteral("超时")) ? State::TimedOut : State::Failed;
            info.error = finalError;
            broadcast(QStringLiteral("stream_error"), request.id, finalError);
            if (request.originalId == 0) broadcast(QStringLiteral("error"), request.id, finalError);
            else {
                // A failed generation never becomes the room's current answer.
                latestRequestIds.insert(request.originalId, info.previousRequestId);
                regenerating.remove(request.originalId);
            }
        }
        while (streamOrder.size() > 30 && streams.value(latestRequestIds.value(streamOrder.head())).finished) {
            const quint64 oldId = streamOrder.dequeue();
            const QString oldMessageId = streams.value(oldId).messageId;
            for (auto it = streams.begin(); it != streams.end();) {
                if (it.value().messageId == oldMessageId) {
                    requestTargets.remove(it.key());
                    it = streams.erase(it);
                } else ++it;
            }
            latestRequestIds.remove(oldId);
            regenerating.remove(oldId);
            requestContexts.remove(oldId);
            requestTargets.remove(oldId);
        }
        startNext();
    });
    activeReplies[request.id] = reply;
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    activeTimers.insert(request.id, timer);
    connect(timer, &QTimer::timeout, this, [this, id = request.id]() {
        if (!activeReplies.contains(id) || streams.value(id).finished) return;
        timedOutRequests.insert(id);
        if (activeReplies.value(id)) activeReplies.value(id)->abort();
    });
    timer->start(requestTimeoutMs);
    }
}
