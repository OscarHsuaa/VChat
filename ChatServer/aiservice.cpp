#include "aiservice.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QElapsedTimer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslSocket>
#include <QTimer>

#include <memory>

namespace {
// SSE frames are buffered as bytes until a blank line. This keeps split UTF-8
// characters and JSON tokens intact across arbitrary QNetworkReply reads.
struct SseParser {
    QByteArray pending;
    QByteArray data;
    QString answer;
    QString protocolError;
    bool done = false;
    QElapsedTimer elapsed;
    int chunks = 0;
    qint64 firstChunkMs = -1;
    qint64 lastChunkMs = -1;
    qint64 maxGapMs = 0;

    void event(const AiService::Chunk &onChunk) {
        if (data.isEmpty() || done || !protocolError.isEmpty()) { data.clear(); return; }
        const QByteArray payload = data.trimmed();
        data.clear();
        if (payload == "[DONE]") { done = true; return; }
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            protocolError = QStringLiteral("AI 流式响应格式异常，请稍后重试。");
            return;
        }
        const QJsonObject object = document.object();
        if (object.contains(QStringLiteral("error"))) {
            protocolError = QStringLiteral("AI 服务暂时不可用，请稍后重试。");
            return;
        }
        const QJsonArray choices = object.value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty()) return; // Usage-only event.
        const QString delta = choices.first().toObject().value(QStringLiteral("delta"))
                                  .toObject().value(QStringLiteral("content")).toString();
        if (!delta.isEmpty()) {
            const qint64 now = elapsed.elapsed();
            if (chunks == 0) firstChunkMs = now;
            else maxGapMs = qMax(maxGapMs, now - lastChunkMs);
            lastChunkMs = now;
            ++chunks;
            answer += delta;
            onChunk(delta);
        }
    }

    void feed(const QByteArray &bytes, const AiService::Chunk &onChunk, bool flush = false) {
        pending += bytes;
        if (pending.size() > 1024 * 1024) {
            protocolError = QStringLiteral("AI 流式响应过大，请稍后重试。");
            return;
        }
        int newline = -1;
        while ((newline = pending.indexOf('\n')) >= 0) {
            QByteArray line = pending.left(newline);
            pending.remove(0, newline + 1);
            if (line.endsWith('\r')) line.chop(1);
            if (line.isEmpty()) event(onChunk);
            else if (line.startsWith("data:")) {
                QByteArray value = line.mid(5);
                if (value.startsWith(' ')) value.remove(0, 1);
                if (!data.isEmpty()) data += '\n';
                data += value;
            }
        }
        if (flush) {
            if (!pending.isEmpty()) {
                QByteArray line = pending;
                if (line.endsWith('\r')) line.chop(1);
                if (line.startsWith("data:")) {
                    if (!data.isEmpty()) data += '\n';
                    data += line.mid(5).trimmed();
                }
                pending.clear();
            }
            event(onChunk);
        }
    }
};
}

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

QNetworkReply *AiService::stream(const QJsonArray &messages, Chunk onChunk, Completion callback)
{
    const QByteArray key = qgetenv("DEEPSEEK_API_KEY").trimmed();
    if (key.isEmpty()) {
        QTimer::singleShot(0, this, [callback]() {
            callback({}, QStringLiteral("AI 服务尚未配置，请联系管理员。"));
        });
        return nullptr;
    }
    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", "Bearer " + key);
    request.setTransferTimeout(timeoutMs);
    const QJsonObject body{
        {QStringLiteral("model"), QStringLiteral("deepseek-flash")},
        {QStringLiteral("thinking"), QJsonObject{{QStringLiteral("type"), QStringLiteral("disabled")}}},
        {QStringLiteral("stream"), true},
        {QStringLiteral("max_tokens"), 1024},
        {QStringLiteral("messages"), messages}
    };
    QNetworkReply *reply = network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    const auto parser = std::make_shared<SseParser>();
    parser->elapsed.start();
    QObject::connect(reply, &QNetworkReply::readyRead, this, [reply, parser, onChunk]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 200 && status < 300) parser->feed(reply->readAll(), onChunk);
        else reply->readAll(); // Never log an upstream body or credential echo.
    });
    QObject::connect(reply, &QNetworkReply::finished, this, [reply, parser, onChunk, callback]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto networkError = reply->error();
        if (status >= 200 && status < 300) parser->feed(reply->readAll(), onChunk, true);
        else reply->readAll();
        reply->deleteLater();
        qInfo() << "DeepSeek SSE: HTTP" << status << "chunks=" << parser->chunks
                << "first_ms=" << parser->firstChunkMs << "last_ms=" << parser->lastChunkMs
                << "max_gap_ms=" << parser->maxGapMs << "complete_ms=" << parser->elapsed.elapsed();
        if (networkError == QNetworkReply::OperationCanceledError) {
            callback(parser->answer, QStringLiteral("已取消 Vivi 生成。"));
        } else if (networkError == QNetworkReply::TimeoutError) {
            qWarning() << "DeepSeek stream failed: HTTP" << status << "category=timeout";
            callback(parser->answer, QStringLiteral("AI 响应超时，请稍后重试。"));
        } else if (networkError != QNetworkReply::NoError || status < 200 || status >= 300) {
            const char *category = status == 401 || status == 403 ? "authentication"
                : status == 429 ? "rate_limit" : status >= 500 ? "upstream"
                : networkError != QNetworkReply::NoError ? "transport" : "request";
            qWarning() << "DeepSeek stream failed: HTTP" << status << "category=" << category
                       << "network_code=" << int(networkError);
            callback(parser->answer, QStringLiteral("AI 服务暂时不可用，请稍后重试。"));
        } else if (!parser->protocolError.isEmpty() || !parser->done || parser->answer.trimmed().isEmpty()) {
            qWarning() << "DeepSeek stream failed: HTTP" << status << "category=invalid_sse";
            callback(parser->answer, parser->protocolError.isEmpty()
                ? QStringLiteral("AI 回复中断，请稍后重试。") : parser->protocolError);
        } else {
            callback(parser->answer, {});
        }
    });
    return reply;
}
