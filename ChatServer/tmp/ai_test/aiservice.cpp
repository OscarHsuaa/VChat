#include "aiservice.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslSocket>
#include <QTimer>

AiService::AiService(QObject *parent, const QUrl &url, int timeout)
    : QObject(parent), endpoint(url), timeoutMs(timeout) {}

bool AiService::isConfigured() const
{
    return !qgetenv("DEEPSEEK_API_KEY").trimmed().isEmpty() && QSslSocket::supportsSsl();
}

void AiService::complete(const QJsonArray &messages, Completion callback)
{
    const QByteArray key = qgetenv("DEEPSEEK_API_KEY").trimmed();
    if (key.isEmpty()) {
        qWarning() << "DeepSeek request skipped: API key not configured";
        QTimer::singleShot(0, this, [callback]() {
            callback({}, QStringLiteral("AI 服务尚未配置，请联系管理员。"));
        });
        return;
    }

    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", "Bearer " + key);
    request.setTransferTimeout(timeoutMs);
    QJsonObject body{
        {QStringLiteral("model"), QStringLiteral("deepseek-flash")},
        {QStringLiteral("thinking"), QJsonObject{{QStringLiteral("type"), QStringLiteral("disabled")}}},
        {QStringLiteral("stream"), false},
        {QStringLiteral("max_tokens"), 1024},
        {QStringLiteral("messages"), messages}
    };
    QNetworkReply *reply = network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QByteArray bytes = reply->readAll();
        reply->deleteLater();
        if (networkError == QNetworkReply::TimeoutError) {
            qWarning() << "DeepSeek request failed: HTTP" << status << "category=timeout";
            callback({}, QStringLiteral("AI 响应超时，请稍后重试。"));
            return;
        }
        if (networkError != QNetworkReply::NoError || status < 200 || status >= 300) {
            const char *category = status == 401 || status == 403 ? "authentication"
                : status == 429 ? "rate_limit"
                : status >= 500 ? "upstream"
                : networkError != QNetworkReply::NoError ? "transport" : "request";
            // Do not log the response body, URL, Authorization header, or raw error text.
            qWarning() << "DeepSeek request failed: HTTP" << status << "category=" << category
                       << "network_code=" << int(networkError);
            callback({}, QStringLiteral("AI 服务暂时不可用，请稍后重试。"));
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
        const QJsonArray choices = document.object().value(QStringLiteral("choices")).toArray();
        const QString answer = choices.isEmpty() ? QString()
            : choices.first().toObject().value(QStringLiteral("message")).toObject()
                  .value(QStringLiteral("content")).toString().trimmed();
        if (parseError.error != QJsonParseError::NoError || answer.isEmpty()) {
            qWarning() << "DeepSeek request failed: HTTP" << status << "category=invalid_response";
            callback({}, QStringLiteral("AI 返回了无法读取的回复，请稍后重试。"));
            return;
        }
        callback(answer, {});
    });
}
