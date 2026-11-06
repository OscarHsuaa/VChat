#include "airoom.h"
#include <QJsonObject>

AiRoom::AiRoom(AiService *aiService, Broadcast send, QObject *parent)
    : QObject(parent), service(aiService), broadcast(std::move(send)) {}

void AiRoom::appendHistory(const QString &role, const QString &content)
{
    history.append(QJsonObject{{QStringLiteral("role"), role}, {QStringLiteral("content"), content}});
    while (history.size() > 20) history.removeAt(0);
    if (historySink) historySink(role, content);
}

void AiRoom::addUserMessage(qint64 accountId, const QString &nickname, const QString &content, bool askVivi)
{
    appendHistory(QStringLiteral("user"), QStringLiteral("[%1] %2").arg(nickname, content));
    if (!askVivi) return;
    const Pending request{nextId++, {accountId, nickname}, content, {}};
    requestTargets.insert(request.id, request.target);
    pending.enqueue(request);
    broadcast(QStringLiteral("thinking"), request.id, QStringLiteral("Vivi 正在思考…"));
    startNext();
}

bool AiRoom::regenerate(quint64 originalId)
{
    if (!requestContexts.contains(originalId)) return false;
    const Pending request{nextId++, requestTargets.value(originalId), {}, requestContexts.value(originalId)};
    requestTargets.insert(request.id, request.target);
    pending.enqueue(request);
    broadcast(QStringLiteral("thinking"), request.id, QStringLiteral("Vivi 正在思考…"));
    startNext();
    return true;
}

void AiRoom::startNext()
{
    if (busy || pending.isEmpty()) return;
    busy = true;
    const Pending request = pending.dequeue();
    broadcast(QStringLiteral("busy"), request.id, {});
    QJsonArray messages = request.context;
    if (messages.isEmpty()) {
        messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
            {QStringLiteral("content"), QStringLiteral(
                "你是 VChat 聊天室的 Vivi 助手。以下是共享聊天室消息，方括号中是成员昵称。"
                "请区分成员身份，简洁回答当前指定成员的问题。回复正文直接开始回答，不要在开头重复成员昵称或方括号标记。")}});
        for (const QJsonValue &value : history) messages.append(value);
        messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
            {QStringLiteral("content"), QStringLiteral("请回答 [%1] 的这次提问：%2").arg(request.target.nickname, request.question)}});
    }
    requestContexts.insert(request.id, messages);
    requestOrder.enqueue(request.id);
    while (requestOrder.size() > 30) {
        const quint64 oldId = requestOrder.dequeue();
        requestContexts.remove(oldId);
        requestTargets.remove(oldId);
    }
    service->complete(messages, [this, request](const QString &answer, const QString &error) {
        if (error.isEmpty()) {
            QString body = answer;
            const QString prefix = QStringLiteral("[%1]").arg(request.target.nickname);
            if (!request.target.nickname.isEmpty() && body.startsWith(prefix)) {
                const QString remainder = body.mid(prefix.size());
                if (!remainder.isEmpty() && (remainder.front().isSpace() || remainder.front() == QLatin1Char(':') || remainder.front() == QChar(0xff1a)))
                    body = remainder.trimmed();
            }
            appendHistory(QStringLiteral("assistant"), body);
            broadcast(QStringLiteral("reply"), request.id, body);
        } else {
            broadcast(QStringLiteral("error"), request.id, error);
        }
        busy = false;
        startNext();
    });
}
