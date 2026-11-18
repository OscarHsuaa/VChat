#include "mainwindow.h"
#include "aimarkdownwidget.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFrame>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QToolButton>
#include <functional>

static bool waitFor(const std::function<bool()> &ready)
{
    QElapsedTimer clock;
    clock.start();
    while (!ready() && clock.elapsed() < 2500) {
        QEventLoop loop;
        QTimer::singleShot(20, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return ready();
}

static QByteArray packet(const QJsonObject &object)
{
    return "AI_SUMMARY|" + QJsonDocument(object).toJson(QJsonDocument::Compact).toBase64() + '\n';
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) return 1;
    QTcpSocket *peer = nullptr;
    QByteArray pending;
    QList<QJsonObject> requests;
    QList<QJsonObject> shareRequests;
    QObject::connect(&server, &QTcpServer::newConnection, &app, [&] {
        peer = server.nextPendingConnection();
        QObject::connect(peer, &QTcpSocket::readyRead, &app, [&] {
            pending += peer->readAll();
            int end;
            while ((end = pending.indexOf('\n')) >= 0) {
                const QByteArray line = pending.left(end).trimmed();
                pending.remove(0, end + 1);
                if (line.startsWith("AI_SUMMARY|")) {
                    const auto document = QJsonDocument::fromJson(QByteArray::fromBase64(line.mid(11)));
                    if (document.isObject()) requests.append(document.object());
                } else if (line.startsWith("SUMMARY_SHARE|")) {
                    const auto document = QJsonDocument::fromJson(QByteArray::fromBase64(line.mid(14)));
                    if (document.isObject()) shareRequests.append(document.object());
                }
            }
        });
    });
    MainWindow window(QStringLiteral("测试者"),
        QStringLiteral("127.0.0.1:%1").arg(server.serverPort()), nullptr, 1,
        QStringLiteral("test-session"));
    window.resize(1200, 780);
    window.show();
    if (!waitFor([&] { return peer != nullptr; })) return 2;
    auto *toggle = window.findChild<QToolButton *>(QStringLiteral("viviComposerButton"));
    auto *menu = window.findChild<QFrame *>(QStringLiteral("viviFeatureMenu"));
    auto *summary = window.findChild<QFrame *>(QStringLiteral("viviSummaryPanel"));
    auto *scroll = window.findChild<QScrollArea *>(QStringLiteral("viviSummaryScroll"));
    auto *members = window.findChild<QListWidget *>(QStringLiteral("userList"));
    auto *close = window.findChild<QToolButton *>(QStringLiteral("viviSummaryClose"));
    auto *chat = window.findChild<QFrame *>(QStringLiteral("chatPanel"));
    if (!toggle || !menu || !summary || !scroll || !members || !close || !chat) return 4;
    const int chatWidth = chat->width();
    toggle->click();
    if (!waitFor([&] { return menu->isVisible(); })) return 5;
    const auto cards = menu->findChildren<QPushButton *>(QStringLiteral("viviActionCard"));
    if (cards.size() != 3) return 6;
    cards.first()->click();
    if (!waitFor([&] { return summary->isVisible(); }) || !requests.isEmpty()) return 16;
    close->click();
    peer->write(packet({{QStringLiteral("event"), QStringLiteral("capability")}}));
    if (!waitFor([&] { return peer->bytesToWrite() == 0; })) return 3;
    toggle->click();
    if (!waitFor([&] { return menu->isVisible(); })) return 17;
    cards.first()->click();
    if (!waitFor([&] { return requests.size() == 1; })) return 7;
    const auto first = requests.first();
    if (first.value(QStringLiteral("action")) != QStringLiteral("start") ||
        first.value(QStringLiteral("room_id")) != QStringLiteral("class-room") ||
        !summary->isVisible() || members->isVisible() || chat->width() != chatWidth) return 8;
    const QString id = first.value(QStringLiteral("request_id")).toString();
    auto send = [&](const QString &event, const QJsonObject &extra = {}) {
        QJsonObject response = extra;
        response.insert(QStringLiteral("event"), event);
        response.insert(QStringLiteral("request_id"), id);
        response.insert(QStringLiteral("room_id"), QStringLiteral("class-room"));
        peer->write(packet(response));
    };
    send(QStringLiteral("meta"), {{QStringLiteral("message_count"), 3},
        {QStringLiteral("first_sequence"), QStringLiteral("9")},
        {QStringLiteral("last_sequence"), QStringLiteral("11")}});
    send(QStringLiteral("chunk"), {{QStringLiteral("text"), QStringLiteral("### 讨论概览\n多人讨论了 Qt。")}});
    if (!waitFor([&] {
        auto *first = summary->findChild<AiMarkdownWidget *>(QStringLiteral("viviSummarySectionBody"));
        return first && first->plainText().contains(QStringLiteral("多人讨论了 Qt"));
    })) return 9;
    const QString text = QStringLiteral("### 讨论概览\n多人讨论了 Qt。\n### 已达成共识\n使用 Qt Widgets。"
        "\n### 待解决问题\n无。\n### 下一步行动\n继续验证。");
    send(QStringLiteral("complete"), {{QStringLiteral("text"), text}});
    auto actions = summary->findChildren<QPushButton *>(QStringLiteral("viviSummaryAction"));
    if (actions.size() != 2 || !waitFor([&] { return actions.at(0)->isEnabled(); })) return 10;
    const auto bodies = summary->findChildren<AiMarkdownWidget *>(QStringLiteral("viviSummarySectionBody"));
    if (bodies.size() != 4) return 21;
    for (int i = 0; i < bodies.size(); ++i)
        if (bodies.at(i)->plainText().isEmpty()) return 18;
    if (scroll->horizontalScrollBar()->maximum() != 0) return 19;
    auto *share = summary->findChild<QPushButton *>(QStringLiteral("viviSummaryShare"));
    if (!share || !share->isEnabled()) return 45;
    share->click();
    if (!waitFor([&] { return shareRequests.size() == 1; }) || share->isEnabled()) return 46;
    const QJsonObject shareRequest = shareRequests.first();
    if (shareRequest.value(QStringLiteral("summary_request_id")).toString() != id ||
        shareRequest.value(QStringLiteral("room_id")).toString() != QStringLiteral("class-room") ||
        shareRequest.contains(QStringLiteral("markdown"))) return 47;
    const QString shareId = shareRequest.value(QStringLiteral("request_id")).toString();
    peer->write(packet({{QStringLiteral("event"), QStringLiteral("result")},
        {QStringLiteral("ok"), true}, {QStringLiteral("request_id"), shareId}})
        .replace("AI_SUMMARY|", "SUMMARY_SHARE|"));
    const QString fiveConsensus = QStringLiteral("### 讨论概览\n多人讨论了 Qt。\n### 已达成共识\n"
        "- 使用 Qt Widgets。\n- 服务端使用 QTcpServer。\n- 历史保存到 MySQL。"
        "\n- 保留权限验证。\n- 明确后续测试。"
        "\n### 待解决问题\n无。\n### 下一步行动\n继续验证。");
    const QJsonObject sharedMessage{{QStringLiteral("event"), QStringLiteral("message")},
        {QStringLiteral("room_id"), QStringLiteral("class-room")},
        {QStringLiteral("message_id"), QStringLiteral("shared-message-1")},
        {QStringLiteral("room_name"), QStringLiteral("VChat 大厅")},
        {QStringLiteral("shared_by_id"), QStringLiteral("1")},
        {QStringLiteral("shared_by_name"), QStringLiteral("测试者")},
        {QStringLiteral("markdown"), fiveConsensus}, {QStringLiteral("message_count"), 3},
        {QStringLiteral("first_sequence"), QStringLiteral("9")},
        {QStringLiteral("last_sequence"), QStringLiteral("11")},
        {QStringLiteral("time"), QStringLiteral("2026-11-16T12:00:00.000Z")}};
    const QByteArray sharedLine = packet(sharedMessage).replace("AI_SUMMARY|", "SUMMARY_SHARE|");
    peer->write(sharedLine);
    peer->write(sharedLine);
    if (!waitFor([&] { return window.findChildren<QFrame *>(
        QStringLiteral("discussionSummaryCard")).size() == 1; }) || share->isEnabled()) return 48;
    window.resize(900, 660);
    if (!waitFor([&] { return scroll->widget()->width() <= scroll->viewport()->width() + 1; })) return 26;
    window.resize(1200, 780);
    const QString capture = qEnvironmentVariable("VCHAT_UI_CAPTURE_DIR");
    if (!capture.isEmpty()) window.grab().save(QDir(capture).filePath(QStringLiteral("vivi-summary-generated.png")));
    actions.at(0)->click();
    if (QApplication::clipboard()->text() != text) return 11;
    actions.at(1)->click();
    if (!waitFor([&] { return requests.size() == 2; })) return 12;
    if (requests.at(1).value(QStringLiteral("request_id")) == id ||
        requests.at(1).value(QStringLiteral("action")) != QStringLiteral("start")) return 13;
    const QString longSummary = QStringLiteral("### 讨论概览\n")
        + QStringLiteral("这是一次多人参与的技术讨论，客户端使用 Qt Widgets。 ").repeated(35);
    peer->write(packet({{QStringLiteral("event"), QStringLiteral("chunk")},
        {QStringLiteral("request_id"), requests.at(1).value(QStringLiteral("request_id"))},
        {QStringLiteral("room_id"), QStringLiteral("class-room")},
        {QStringLiteral("text"), longSummary}}));
    window.resize(900, 660);
    if (!waitFor([&] { return bodies.at(0)->plainText().size() > 300 &&
                             scroll->verticalScrollBar()->maximum() > 0; }) ||
        scroll->horizontalScrollBar()->maximum() != 0 ||
        !actions.at(0)->isVisible() || !actions.at(1)->isVisible()) return 20;
    if (!capture.isEmpty())
        window.grab().save(QDir(capture).filePath(QStringLiteral("vivi-summary-long.png")));
    window.resize(1200, 780);
    const QJsonObject oldReply{{QStringLiteral("event"), QStringLiteral("complete")},
        {QStringLiteral("request_id"), id}, {QStringLiteral("room_id"), QStringLiteral("class-room")},
        {QStringLiteral("text"), QStringLiteral("过期结果")}};
    peer->write(packet(oldReply));
    close->click();
    if (!waitFor([&] { return requests.size() == 3; })) return 14;
    if (requests.at(2).value(QStringLiteral("action")) != QStringLiteral("cancel") ||
        requests.at(2).value(QStringLiteral("request_id")) !=
            requests.at(1).value(QStringLiteral("request_id")) ||
        summary->isVisible() || !members->isVisible() || chat->width() != chatWidth) return 15;
    toggle->click();
    if (!waitFor([&] { return menu->isVisible(); })) return 22;
    cards.first()->click();
    if (!waitFor([&] { return requests.size() == 4; })) return 23;
    const QString codeSummary = QStringLiteral("### 讨论概览\n")
        + QStringLiteral("多人讨论了客户端与服务端方案。 ").repeated(30)
        + QStringLiteral("\n```cpp\n### 待解决问题\n```")
        + QStringLiteral("\n### 已达成共识\n")
        + QStringLiteral("- 继续使用 Qt Widgets。\n").repeated(11)
        + QStringLiteral("- ") + QStringLiteral("这条共识需要在窄面板中自然换行，仍然完整显示。 ").repeated(5)
        + QStringLiteral("\n### 下一步行动\n继续测试。");
    peer->write(packet({{QStringLiteral("event"), QStringLiteral("complete")},
        {QStringLiteral("request_id"), requests.at(3).value(QStringLiteral("request_id"))},
        {QStringLiteral("room_id"), QStringLiteral("class-room")},
        {QStringLiteral("text"), codeSummary}}));
    auto *missing = summary->findChild<QFrame *>(QStringLiteral("viviSummarySection待解决问题"));
    if (!waitFor([&] { return missing && missing->findChild<QLabel *>(
        QStringLiteral("viviSectionEmpty"))->text() == QStringLiteral("暂无明确内容"); })) return 24;
    if (!bodies.at(0)->plainText().contains(QStringLiteral("### 待解决问题")) ||
        !missing->findChild<QLabel *>(QStringLiteral("viviSectionEmpty"))->isVisible()) return 25;
    window.resize(900, 660);
    if (!waitFor([&] { return scroll->verticalScrollBar()->maximum() > 0; })) return 27;
    if (summary->findChildren<QScrollArea *>().size() != 1 ||
        scroll->horizontalScrollBar()->maximum() != 0 ||
        !actions.at(0)->isVisible() || !actions.at(1)->isVisible()) return 28;
    auto *consensus = summary->findChild<QFrame *>(QStringLiteral("viviSummarySection已达成共识"));
    if (!consensus || consensus->findChildren<QWidget *>(QStringLiteral("viviSummaryRow")).size() != 12) return 37;
    const auto rowTexts = consensus->findChildren<QLabel *>(QStringLiteral("viviSummaryRowText"));
    if (rowTexts.size() != 12 || rowTexts.last()->height() <=
        rowTexts.last()->fontMetrics().lineSpacing() * 2) return 44;
    for (AiMarkdownWidget *body : bodies) {
        if (body->isVisible() && body->y() + body->height() > body->parentWidget()->height() + 2)
            return 31;
        for (QTextBrowser *prose : body->findChildren<QTextBrowser *>())
            if (prose->verticalScrollBar()->maximum() != 0 ||
                prose->viewport()->height() + 2 < qCeil(prose->document()->size().height())) return 29;
        for (QPlainTextEdit *code : body->findChildren<QPlainTextEdit *>())
            if (code->verticalScrollBar()->maximum() != 0) return 30;
    }
    if (!capture.isEmpty())
        window.grab().save(QDir(capture).filePath(QStringLiteral("vivi-summary-layout-fixed.png")));
    scroll->verticalScrollBar()->setValue(consensus->y());
    QApplication::processEvents();
    if (!capture.isEmpty())
        window.grab().save(QDir(capture).filePath(QStringLiteral("vivi-summary-consensus-v2.png")));
    actions.at(1)->click();
    if (!waitFor([&] { return requests.size() == 5; })) return 32;
    const QString compactSummary = QStringLiteral(
        "### 讨论概览\n已确定客户端与服务端的技术方案。\n第二行概览。"
        "\n### 已达成共识\n- 客户端使用 Qt Widgets。"
        "\n### 待解决问题\n暂无。"
        "\n### 下一步行动\n继续验证。");
    peer->write(packet({{QStringLiteral("event"), QStringLiteral("complete")},
        {QStringLiteral("request_id"), requests.at(4).value(QStringLiteral("request_id"))},
        {QStringLiteral("room_id"), QStringLiteral("class-room")},
        {QStringLiteral("text"), compactSummary}}));
    if (!waitFor([&] { return bodies.at(0)->plainText().contains(QStringLiteral("第二行概览")); })) return 33;
    if (consensus->findChildren<QWidget *>(QStringLiteral("viviSummaryRow")).size() != 1) return 38;
    if (bodies.at(0)->height() > 170 || bodies.at(0)->parentWidget()->height() > 235 ||
        bodies.at(1)->height() > 170 || bodies.at(1)->parentWidget()->height() > 235) return 34;
    window.resize(1200, 780);
    if (!waitFor([&] { return scroll->viewport()->height() >= scroll->widget()->height(); }) ||
        scroll->verticalScrollBar()->maximum() != 0 ||
        scroll->horizontalScrollBar()->maximum() != 0) return 35;
    if (!waitFor([&] { return !scroll->verticalScrollBar()->isVisible(); })) return 36;
    if (!capture.isEmpty())
        window.grab().save(QDir(capture).filePath(QStringLiteral("vivi-summary-compact-fixed.png")));
    actions.at(1)->click();
    if (!waitFor([&] { return requests.size() == 6; })) return 39;
    const QString emptySummary = QStringLiteral(
        "### 讨论概览\n只有一行概览。"
        "\n### 已达成共识\n- 一个已确认的决定。"
        "\n### 待解决问题\n"
        "### 下一步行动\n");
    peer->write(packet({{QStringLiteral("event"), QStringLiteral("complete")},
        {QStringLiteral("request_id"), requests.at(5).value(QStringLiteral("request_id"))},
        {QStringLiteral("room_id"), QStringLiteral("class-room")},
        {QStringLiteral("text"), emptySummary}}));
    auto *nextSteps = summary->findChild<QFrame *>(QStringLiteral("viviSummarySection下一步行动"));
    if (!waitFor([&] { return nextSteps &&
        missing->findChild<QLabel *>(QStringLiteral("viviSectionEmpty"))->text() ==
            QStringLiteral("暂无明确内容") &&
        nextSteps->findChild<QLabel *>(QStringLiteral("viviSectionEmpty"))->text() ==
            QStringLiteral("暂无明确内容"); })) return 40;
    if (scroll->verticalScrollBar()->maximum() != 0 ||
        summary->findChildren<QScrollArea *>().size() != 1) return 41;
    actions.at(1)->click();
    if (!waitFor([&] { return requests.size() == 7; })) return 42;
    const QString mixedSummary = QStringLiteral(
        "### 讨论概览\n这是混合 Markdown 示例。"
        "\n### 已达成共识\n- **保留粗体** 与 `代码`。"
        "\n### 待解决问题\n- 第一层\n  - 第二层"
        "\n### 下一步行动\n1. 先验证\n2. 再发布");
    peer->write(packet({{QStringLiteral("event"), QStringLiteral("meta")},
        {QStringLiteral("request_id"), requests.at(6).value(QStringLiteral("request_id"))},
        {QStringLiteral("room_id"), QStringLiteral("class-room")},
        {QStringLiteral("message_count"), 4},
        {QStringLiteral("first_sequence"), QStringLiteral("12")},
        {QStringLiteral("last_sequence"), QStringLiteral("15")}}));
    peer->write(packet({{QStringLiteral("event"), QStringLiteral("complete")},
        {QStringLiteral("request_id"), requests.at(6).value(QStringLiteral("request_id"))},
        {QStringLiteral("room_id"), QStringLiteral("class-room")},
        {QStringLiteral("text"), mixedSummary}}));
    if (!waitFor([&] { return bodies.at(1)->plainText().contains(QStringLiteral("保留粗体")); }) ||
        !bodies.at(1)->isVisible() ||
        !bodies.at(2)->plainText().contains(QStringLiteral("第二层")) ||
        !bodies.at(3)->plainText().contains(QStringLiteral("再发布"))) return 43;
    share->click();
    if (!waitFor([&] { return shareRequests.size() == 2; }) || share->isEnabled()) return 55;
    const QString retryId = shareRequests.last().value(QStringLiteral("request_id")).toString();
    peer->write(packet({{QStringLiteral("event"), QStringLiteral("result")},
        {QStringLiteral("ok"), false}, {QStringLiteral("request_id"), retryId},
        {QStringLiteral("error"), QStringLiteral("数据库暂时不可用")}})
        .replace("AI_SUMMARY|", "SUMMARY_SHARE|"));
    if (!waitFor([&] { return share->isEnabled(); })) return 56;
    share->click();
    if (!waitFor([&] { return shareRequests.size() == 3; }) ||
        shareRequests.last().value(QStringLiteral("request_id")).toString() != retryId) return 57;
    peer->write(packet({{QStringLiteral("event"), QStringLiteral("result")},
        {QStringLiteral("ok"), true}, {QStringLiteral("request_id"), retryId}})
        .replace("AI_SUMMARY|", "SUMMARY_SHARE|"));
    auto *sharedCard = window.findChild<QFrame *>(QStringLiteral("discussionSummaryCard"));
    auto *kind = sharedCard ? sharedCard->findChild<QLabel *>(
        QStringLiteral("discussionSummaryKind")) : nullptr;
    auto *sharedView = sharedCard ? sharedCard->findChild<QPushButton *>(
        QStringLiteral("discussionSummaryView")) : nullptr;
    auto *sharedCopy = sharedCard ? sharedCard->findChild<QPushButton *>(
        QStringLiteral("discussionSummaryCopy")) : nullptr;
    if (!sharedView || !sharedCopy || !kind || kind->text() != QStringLiteral("讨论总结")) return 49;
    if (sharedCard->findChildren<QWidget *>(QStringLiteral("discussionSummaryEntry1")).size() != 3 ||
        sharedCard->findChild<QLabel *>(QStringLiteral("discussionSummaryRemaining1"))->text()
            != QStringLiteral("还有 2 项，查看完整总结")) return 59;
    if (sharedCard->height() > 580) return 60;
    if (sharedView->y() + sharedView->height() > sharedCard->height() ||
        sharedCopy->y() + sharedCopy->height() > sharedCard->height()) return 72;
    auto *messageScroll = window.findChild<QScrollArea *>(QStringLiteral("messageScroll"));
    if (!messageScroll || !sharedCard->findChildren<QScrollArea *>().isEmpty()) return 66;
    window.resize(900, 660);
    if (!waitFor([&] { return sharedCard->width() <= messageScroll->viewport()->width() - 50; })) return 67;
    window.resize(1200, 780);
    sharedCopy->click();
    if (QApplication::clipboard()->text() != fiveConsensus) return 50;
    const int beforeView = requests.size();
    sharedView->click();
    if (!waitFor([&] { return summary->isVisible() && !share->isVisible() &&
        !actions.at(1)->isVisible(); }) || requests.size() != beforeView) return 51;
    const QString expectedLocal = QDateTime::fromString(
        QStringLiteral("2026-11-16T12:00:00.000Z"), Qt::ISODateWithMs)
        .toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    auto *status = summary->findChild<QLabel *>(QStringLiteral("viviSummaryStatus"));
    if (!status || !status->text().contains(expectedLocal) || status->text().contains(QLatin1Char('T')))
        return 61;
    if (!waitFor([&] {
        const auto rows = consensus->findChildren<QWidget *>(QStringLiteral("viviSummaryRow"));
        if (rows.size() != 5) return false;
        auto *heading = consensus->findChild<QLabel *>(QStringLiteral("viviSectionTitle"));
        if (!heading) return false;
        for (int i = 0; i < rows.size(); ++i) {
            QWidget *row = rows.at(i);
            QLabel *text = row->findChild<QLabel *>(QStringLiteral("viviSummaryRowText"));
            if (!text || row->height() < text->heightForWidth(text->width())) return false;
            if (row->findChildren<QLabel *>(QString(), Qt::FindDirectChildrenOnly).size() != 1)
                return false;
            if (text->mapTo(consensus, QPoint(0, 0)).x() != heading->x()) return false;
            if (i > 0 && row->y() < rows.at(i - 1)->y() + rows.at(i - 1)->height() + 8)
                return false;
        }
        auto *container = consensus->findChild<QWidget *>(QStringLiteral("viviSummaryRows"));
        return container && consensus->height() >= consensus->sizeHint().height() &&
            rows.last()->y() + rows.last()->height() <= container->height();
    })) return 65;
    if (!capture.isEmpty())
        window.grab().save(QDir(capture).filePath(QStringLiteral("vivi-summary-shared-card.png")));
    actions.at(0)->click();
    if (QApplication::clipboard()->text() != fiveConsensus) return 52;
    close->click();
    toggle->click();
    if (!waitFor([&] { return menu->isVisible(); })) return 53;
    cards.first()->click();
    if (!waitFor([&] { return summary->isVisible() && share->isVisible() &&
        actions.at(1)->isVisible(); }) || requests.size() != beforeView ||
        !bodies.at(1)->plainText().contains(QStringLiteral("保留粗体"))) return 54;
    const QString manyConsensus = QStringLiteral("### 讨论概览\n")
        + QStringLiteral("这段概览超过三百字，仍应在完整面板中自然换行。 ").repeated(22)
        + QStringLiteral("\n### 已达成共识\n")
        + QStringLiteral("- 第 %1 项已确认的团队决定。\n").arg(1);
    QString historyMarkdown = manyConsensus;
    for (int i = 2; i <= 11; ++i)
        historyMarkdown += QStringLiteral("- 第 %1 项已确认的团队决定。\n").arg(i);
    historyMarkdown += QStringLiteral("### 待解决问题\n暂无。\n### 下一步行动\n继续验证。");
    const QJsonObject historyItem{{QStringLiteral("room_id"), QStringLiteral("class-room")},
        {QStringLiteral("message_id"), QStringLiteral("history-shared-message")},
        {QStringLiteral("type"), QStringLiteral("discussion_summary")},
        {QStringLiteral("sender_id"), QStringLiteral("2")},
        {QStringLiteral("sender"), QStringLiteral("另一成员")},
        {QStringLiteral("time"), QStringLiteral("2026-11-16T12:01:00.000")},
        {QStringLiteral("text"), QString::fromUtf8(QJsonDocument(
            QJsonObject{{QStringLiteral("room_name"), QStringLiteral("VChat 大厅")},
                        {QStringLiteral("markdown"), historyMarkdown},
                        {QStringLiteral("message_count"), 4},
                        {QStringLiteral("first_sequence"), QStringLiteral("12")},
                        {QStringLiteral("last_sequence"), QStringLiteral("15")}})
            .toJson(QJsonDocument::Compact))}};
    const QByteArray historyLine = packet(historyItem).replace("AI_SUMMARY|", "HISTORY|");
    peer->write(historyLine);
    peer->write(historyLine);
    if (!waitFor([&] { return window.findChildren<QFrame *>(
        QStringLiteral("discussionSummaryCard")).size() == 2; })) return 58;
    const auto sharedCards = window.findChildren<QFrame *>(QStringLiteral("discussionSummaryCard"));
    auto *historical = sharedCards.last();
    if (historical->findChildren<QWidget *>(QStringLiteral("discussionSummaryEntry1")).size() != 3 ||
        historical->findChild<QLabel *>(QStringLiteral("discussionSummaryRemaining1"))->text()
            != QStringLiteral("还有 8 项，查看完整总结")) return 62;
    historical->findChild<QPushButton *>(QStringLiteral("discussionSummaryView"))->click();
    if (!waitFor([&] { return summary->isVisible() &&
        consensus->findChildren<QWidget *>(QStringLiteral("viviSummaryRow")).size() == 11; })) return 63;
    if (!waitFor([&] {
        const auto rows = consensus->findChildren<QWidget *>(QStringLiteral("viviSummaryRow"));
        auto *container = consensus->findChild<QWidget *>(QStringLiteral("viviSummaryRows"));
        if (rows.size() != 11 || !container) return false;
        for (QWidget *row : rows) {
            QLabel *text = row->findChild<QLabel *>(QStringLiteral("viviSummaryRowText"));
            if (!text || row->height() < text->heightForWidth(text->width())) return false;
        }
        return consensus->height() >= consensus->sizeHint().height() &&
            rows.last()->y() + rows.last()->height() <= container->height();
    })) return 68;
    if (scroll->horizontalScrollBar()->maximum() != 0) return 64;
    if (summary->findChildren<QScrollArea *>().size() != 1) return 69;
    if (!waitFor([&] { return scroll->verticalScrollBar()->maximum() > 0; })) return 70;
    if (!actions.at(0)->isVisible()) return 71;
    close->click();
    return 0;
}
