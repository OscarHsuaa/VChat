#include "mainwindow.h"
#include "aimarkdownwidget.h"
#include <QApplication>
#include <QClipboard>
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
        + QStringLiteral("- 继续使用 Qt Widgets。\n").repeated(12)
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
    if (bodies.at(0)->height() > 170 || bodies.at(0)->parentWidget()->height() > 235 ||
        bodies.at(1)->height() > 170 || bodies.at(1)->parentWidget()->height() > 235) return 34;
    window.resize(1200, 780);
    if (!waitFor([&] { return scroll->viewport()->height() >= scroll->widget()->height(); }) ||
        scroll->verticalScrollBar()->maximum() != 0 ||
        scroll->horizontalScrollBar()->maximum() != 0) return 35;
    if (!waitFor([&] { return !scroll->verticalScrollBar()->isVisible(); })) return 36;
    if (!capture.isEmpty())
        window.grab().save(QDir(capture).filePath(QStringLiteral("vivi-summary-compact-fixed.png")));
    close->click();
    return 0;
}
