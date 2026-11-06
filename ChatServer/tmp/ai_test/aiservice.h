#pragma once

#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QUrl>
#include <functional>

class AiService : public QObject
{
public:
    using Completion = std::function<void(const QString &answer, const QString &error)>;
    explicit AiService(QObject *parent = nullptr,
                       const QUrl &endpoint = QUrl(QStringLiteral("https://api.deepseek.com/chat/completions")),
                       int timeoutMs = 30000);
    void complete(const QJsonArray &messages, Completion callback);
    bool isConfigured() const;

private:
    QNetworkAccessManager network;
    QUrl endpoint;
    int timeoutMs;
};
