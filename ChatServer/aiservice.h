#pragma once

#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QUrl>
#include <functional>

class QNetworkReply;

class AiService : public QObject
{
public:
    using Completion = std::function<void(const QString &answer, const QString &error)>;
    using Chunk = std::function<void(const QString &chunk)>;
    explicit AiService(QObject *parent = nullptr,
                       const QUrl &endpoint = QUrl(QStringLiteral("https://api.deepseek.com/chat/completions")),
                       int timeoutMs = 30000);
    void complete(const QJsonArray &messages, Completion callback);
    QNetworkReply *stream(const QJsonArray &messages, Chunk onChunk, Completion callback);
    bool isConfigured() const;

private:
    QNetworkAccessManager network;
    QUrl endpoint;
    int timeoutMs;
};
