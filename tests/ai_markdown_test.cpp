#include "aimarkdownwidget.h"

#include <QApplication>
#include <QColor>
#include <QClipboard>
#include <QFrame>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextList>
#include <QTextLayout>
#include <QTextTable>
#include <QVBoxLayout>

static int check(bool condition, int error)
{
    if (!condition) qWarning("Markdown check %d failed", error);
    return condition ? 0 : error;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    for (const QString &markdown : {
             QStringLiteral("你好，有什么可以帮你的吗？"),
             QStringLiteral("**已确认使用 Qt Widgets。**"),
             QStringLiteral("[查看 Qt Widgets 文档](https://doc.qt.io/qt-6/qtwidgets-index.html)"),
             QStringLiteral("- 客户端采用 Qt Widgets 开发。")}) {
        AiMarkdownWidget natural(markdown);
        natural.show();
        natural.setContentWidth(natural.naturalContentWidth() + 2);
        app.processEvents();
        QTextBrowser *prose = natural.findChild<QTextBrowser *>();
        if (int result = check(prose &&
            prose->document()->firstBlock().layout()->lineCount() == 1 &&
            natural.width() < 500 && prose->verticalScrollBar()->maximum() == 0, 30)) return result;
    }
    const QStringList bubbleCases{
        QStringLiteral("你好，有什么可以帮你的吗？"),
        QStringLiteral("第一行说明。\n第二行说明。"),
        QStringLiteral("第一段讨论。\n\n第二段结论。"),
        QStringLiteral("1. 客户端采用 Qt Widgets。\n2. 服务端采用 QTcpServer。"),
        QStringLiteral("- 客户端采用 Qt Widgets。\n- 服务端采用 QTcpServer。"),
        QStringLiteral("结论是 **方案已确认**"),
        QStringLiteral("简短说明。\n\n```cpp\nQTcpServer server;\n```"),
        QStringLiteral("[这是一个较长的 Qt Widgets 参考链接标题，应该在气泡中自然换行]"
                       "(https://doc.qt.io/qt-6/qtwidgets-index.html)")};
    for (int index = 0; index < bubbleCases.size(); ++index) {
        QFrame card;
        card.setObjectName(QStringLiteral("viviReadingCard"));
        card.setStyleSheet(QStringLiteral(
            "QFrame#viviReadingCard { border:1px solid #e9dfff; border-radius:16px; }"));
        card.setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
        auto *cardLayout = new QVBoxLayout(&card);
        cardLayout->setContentsMargins(17, 13, 17, 13);
        cardLayout->setSpacing(0);
        AiMarkdownWidget body({}, &card);
        body.setStreamingText(bubbleCases.at(index));
        cardLayout->addWidget(&body);
        card.show();
        body.finishStreaming(bubbleCases.at(index));
        card.setFixedWidth(qMin(700, body.naturalContentWidth() + 36));
        cardLayout->activate();
        body.setContentWidth(cardLayout->contentsRect().width());
        for (int pass = 0; pass < 3; ++pass) app.processEvents();
        const int bottom = card.height() - body.y() - body.height();
        if (qAbs(body.y() - bottom) > 2) {
            qWarning("Bubble case %d: top=%d bottom=%d card=%d hint=%d body=%d",
                     index, body.y(), bottom, card.height(), card.sizeHint().height(), body.height());
            return 31;
        }
    }
    AiMarkdownWidget shortReply(QStringLiteral("- 客户端使用 Qt Widgets。\n- 服务端使用 QTcpServer。\n- 数据库使用 MySQL。"));
    shortReply.show();
    shortReply.setContentWidth(400);
    app.processEvents();
    auto *shortBrowser = shortReply.findChild<QTextBrowser *>();
    if (int result = check(shortBrowser && shortReply.height() < 160 &&
                           shortBrowser->verticalScrollBar()->maximum() == 0, 23)) return result;
    if (int result = check(shortBrowser->document()->lastBlock().blockFormat().bottomMargin() == 0, 28))
        return result;
    AiMarkdownWidget tenLines(QStringLiteral("- 技术方案已确认。\n").repeated(10));
    tenLines.show();
    tenLines.setContentWidth(400);
    app.processEvents();
    if (int result = check(tenLines.height() > shortReply.height() &&
                           tenLines.height() < 650 &&
                           tenLines.findChild<QTextBrowser *>()->verticalScrollBar()->maximum() == 0,
                           27)) return result;
    AiMarkdownWidget initiallyHidden(shortReply.markdownText());
    initiallyHidden.setContentWidth(400);
    initiallyHidden.show();
    app.processEvents();
    if (int result = check(qAbs(initiallyHidden.height() - shortReply.height()) <= 4, 24)) return result;
    AiMarkdownWidget replacing(QString{});
    replacing.show();
    replacing.setContentWidth(400);
    replacing.beginStreaming();
    replacing.setStreamingText(QStringLiteral("很长的流式内容").repeated(150));
    app.processEvents();
    if (int result = check(replacing.height() > shortReply.height() * 2, 25)) return result;
    replacing.finishStreaming(shortReply.markdownText());
    app.processEvents();
    if (int result = check(qAbs(replacing.height() - shortReply.height()) <= 4 &&
                           replacing.findChild<QTextBrowser *>()->verticalScrollBar()->maximum() == 0,
                           26)) return result;
    const QString longLine(280, QLatin1Char('x'));
    const QString markdown = QStringLiteral(
        "# 技术方案\n\n## 客户端\n\n### 通信\n\n"
        "这是中文与 Qt Widgets、QTcpServer 混排的长段落。**加粗**、*斜体*、`room_id`。\n\n"
        "- 第一项\n  - 子项\n1. 有序项\n2. 后续项\n\n"
        "> 这是引用内容。\n\n---\n\n"
        "[Qt 文档](https://doc.qt.io)\n\n"
        "| 阶段 | 状态 |\n| --- | --- |\n| 实现 | 完成 |\n\n"
        "```cpp\nQTcpServer server;\nserver.listen(QHostAddress::Any, 8888);\n")
        + longLine + QStringLiteral("\n```\n\n末尾段落。");

    QFrame bubble;
    bubble.setObjectName(QStringLiteral("testBubble"));
    bubble.setStyleSheet(QStringLiteral("QFrame#testBubble { background:#eee8fc; border-radius:14px; }"));
    auto *bubbleLayout = new QVBoxLayout(&bubble);
    bubbleLayout->setContentsMargins(16, 12, 16, 12);
    AiMarkdownWidget history(markdown, &bubble);
    bubbleLayout->addWidget(&history);
    bubble.show();
    for (const int width : {510, 360, 220}) {
        history.setContentWidth(width);
        app.processEvents();
        if (int result = check(history.width() == width && history.height() > 30, 1)) return result;
        for (QTextBrowser *browser : history.findChildren<QTextBrowser *>()) {
            const int expected = qCeil(browser->document()->size().height());
            if (int result = check(browser->height() >= expected, 2)) return result;
            if (int result = check(browser->width() == width, 3)) return result;
        }
    }
    QTextBrowser *browser = history.findChild<QTextBrowser *>();
    if (int result = check(browser != nullptr, 4)) return result;
    QTextDocument *doc = browser->document();
    bool heading = false, nestedList = false, quote = false, table = false;
    bool inlineCodeStyled = false;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        heading |= block.blockFormat().headingLevel() == 1;
        nestedList |= block.textList() && block.textList()->format().indent() > 1;
        quote |= block.blockFormat().intProperty(QTextFormat::BlockQuoteLevel) > 0;
        table |= QTextCursor(block).currentTable() != nullptr;
        for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
            const QTextFragment text = fragment.fragment();
            if (text.text().contains(QStringLiteral("room_id"))) {
                inlineCodeStyled = text.charFormat().background().color() == QColor(QStringLiteral("#eee9f8"));
            }
        }
    }
    if (int result = check(heading && nestedList && quote, 5)) return result;
    for (QTextBrowser *prose : history.findChildren<QTextBrowser *>())
        for (QTextBlock block = prose->document()->begin(); block.isValid(); block = block.next())
            table |= QTextCursor(block).currentTable() != nullptr;
    if (int result = check(table, 6)) return result;
    if (int result = check(inlineCodeStyled, 13)) return result;
    QPlainTextEdit *code = history.findChild<QPlainTextEdit *>();
    if (int result = check(code && code->toPlainText().contains(longLine), 7)) return result;
    if (int result = check(!code->toPlainText().endsWith(QLatin1Char('\n')), 29)) return result;
    app.processEvents();
    if (int result = check(code->horizontalScrollBar()->maximum() > 0, 8)) return result;
    if (int result = check(code->verticalScrollBar()->maximum() == 0, 14)) return result;
    QPushButton *copyCode = nullptr;
    for (QPushButton *button : history.findChildren<QPushButton *>())
        if (button->text() == QStringLiteral("复制代码")) copyCode = button;
    if (int result = check(copyCode != nullptr, 15)) return result;
    copyCode->click();
    if (QApplication::platformName() == QStringLiteral("offscreen"))
        if (int result = check(QApplication::clipboard()->text() == code->toPlainText(), 16)) return result;
    if (int result = check(history.plainText().contains(QStringLiteral("Qt Widgets")) &&
                           !history.plainText().contains(QStringLiteral("<table>")), 9)) return result;
    if (int result = check(history.markdownText() == markdown, 10)) return result;

    AiMarkdownWidget stream(QString{});
    stream.show();
    stream.setContentWidth(360);
    stream.beginStreaming();
    stream.setStreamingText(QStringLiteral("**尚未闭合\n```cpp\nint x"));
    app.processEvents();
    if (int result = check(stream.plainText().contains(QStringLiteral("尚未闭合")) &&
                           stream.findChildren<QPlainTextEdit *>().isEmpty(), 11)) return result;
    stream.finishStreaming(markdown);
    app.processEvents();
    if (int result = check(stream.plainText() == history.plainText() &&
                           stream.markdownText() == markdown &&
                           !stream.findChildren<QPlainTextEdit *>().isEmpty(), 12)) return result;

    AiMarkdownWidget longParagraph(QStringLiteral("多人讨论中的中文长段落与 C++、Qt Widgets 混排。")
        .repeated(30));
    longParagraph.setContentWidth(220);
    longParagraph.show();
    app.processEvents();
    if (int result = check(longParagraph.height() > 300 &&
                           longParagraph.findChild<QTextBrowser *>()->height() <= longParagraph.height(), 17)) return result;
    for (const int count : {50, 100, 500}) {
        AiMarkdownWidget answer(QString(count, QChar(0x4e2d)));
        answer.show();
        for (const int width : {500, 220}) {
            answer.setContentWidth(width);
            app.processEvents();
            auto *prose = answer.findChild<QTextBrowser *>();
            if (int result = check(prose && prose->verticalScrollBar()->maximum() == 0 &&
                                   prose->viewport()->height() + 2 >= qCeil(prose->document()->size().height()) &&
                                   prose->y() + prose->height() <= answer.height(), 22)) return result;
        }
    }
    AiMarkdownWidget unfinishedFence(QStringLiteral("~~~cpp\nint main() {\n"));
    unfinishedFence.setContentWidth(220);
    if (int result = check(unfinishedFence.findChild<QPlainTextEdit *>() != nullptr &&
                           unfinishedFence.plainText().contains(QStringLiteral("int main")), 18)) return result;

    const QString longCode = QStringLiteral("```cpp\n")
        + QStringLiteral("QTcpServer server; // line\n").repeated(40)
        + QStringLiteral("```");
    AiMarkdownWidget fullHeight(QStringLiteral("# 标题\n\n")
        + QStringLiteral("多人技术讨论与 Markdown 排版。 ").repeated(45)
        + QStringLiteral("\n\n- 第一项\n  - 子项\n\n") + longCode);
    fullHeight.show();
    for (const int width : {500, 300, 210}) {
        fullHeight.setContentWidth(width);
        app.processEvents();
        for (QTextBrowser *prose : fullHeight.findChildren<QTextBrowser *>()) {
            const int needed = qCeil(prose->document()->size().height());
            if (int result = check(prose->verticalScrollBar()->maximum() == 0 &&
                                   prose->viewport()->height() + 2 >= needed, 19)) return result;
        }
        for (QPlainTextEdit *editor : fullHeight.findChildren<QPlainTextEdit *>()) {
            if (int result = check(editor->verticalScrollBar()->maximum() == 0, 20)) return result;
        }
        for (QWidget *child : fullHeight.findChildren<QWidget *>(
                 QString(), Qt::FindDirectChildrenOnly))
            if (int result = check(child->y() + child->height() <= fullHeight.height(), 21)) return result;
    }

    // Qt screenshots exercise actual layout and painting at different widths.
    if (argc > 1) {
        history.setContentWidth(510);
        app.processEvents();
        bubble.grab().save(QString::fromLocal8Bit(argv[1]));
    }
    return 0;
}
