#include "aimarkdownwidget.h"

#include <QClipboard>
#include <QColor>
#include <QFont>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QShowEvent>
#include <QSizePolicy>
#include <QSyntaxHighlighter>
#include <QTextBrowser>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QTextFormat>
#include <QTextFragment>
#include <QTextList>
#include <QTextTable>
#include <QTextTableCell>
#include <QTimer>
#include <QVBoxLayout>

namespace {
// QTextBrowser must never resolve an image or another resource named by AI text.
class SafeMarkdownBrowser final : public QTextBrowser
{
public:
    using QTextBrowser::QTextBrowser;
protected:
    QVariant loadResource(int, const QUrl &) override { return {}; }
    void paintEvent(QPaintEvent *event) override
    {
        QTextBrowser::paintEvent(event);
        QPainter painter(viewport());
        painter.setPen(QPen(QColor(QStringLiteral("#9470df")), 3, Qt::SolidLine, Qt::RoundCap));
        for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
            if (block.blockFormat().intProperty(QTextFormat::BlockQuoteLevel) <= 0) continue;
            const QRectF bounds = document()->documentLayout()->blockBoundingRect(block);
            const QTextBlockFormat format = block.blockFormat();
            const qreal x = qMax<qreal>(4, format.leftMargin()
                + format.indent() * document()->indentWidth() - 7)
                - horizontalScrollBar()->value();
            const qreal top = bounds.top() - verticalScrollBar()->value() + 2;
            painter.drawLine(QPointF(x, top), QPointF(x, top + bounds.height() - 4));
        }
    }
};

struct Fence {
    QChar marker;
    int length = 0;
    QString info;
};

Fence openingFence(const QString &line)
{
    int pos = 0;
    while (pos < line.size() && line.at(pos) == QLatin1Char(' ') && pos < 3) ++pos;
    if (pos == line.size()) return {};
    const QChar marker = line.at(pos);
    if (marker != QLatin1Char('`') && marker != QLatin1Char('~')) return {};
    const int start = pos;
    while (pos < line.size() && line.at(pos) == marker) ++pos;
    if (pos - start < 3) return {};
    const QString info = line.mid(pos).trimmed();
    if (marker == QLatin1Char('`') && info.contains(marker)) return {};
    return {marker, pos - start, info};
}

bool closingFence(const QString &line, const Fence &opening)
{
    int pos = 0;
    while (pos < line.size() && line.at(pos) == QLatin1Char(' ') && pos < 3) ++pos;
    const int start = pos;
    while (pos < line.size() && line.at(pos) == opening.marker) ++pos;
    return pos - start >= opening.length && line.mid(pos).trimmed().isEmpty();
}

void styleDocument(QTextDocument *document, bool compact)
{
    QFont body(QStringLiteral("Microsoft YaHei UI"));
    body.setPixelSize(14);
    document->setDefaultFont(body);
    document->setDocumentMargin(1);
    document->setIndentWidth(compact ? 17 : 23);
    // Qt parses the Markdown; QTextBlockFormat supplies predictable Chinese
    // line spacing and paragraph rhythm independent of platform HTML defaults.
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        QTextCursor cursor(block);
        QTextBlockFormat format = block.blockFormat();
        const bool inTable = cursor.currentTable() != nullptr;
        const bool inList = block.textList() != nullptr;
        const int heading = format.headingLevel();
        format.setLineHeight(heading ? (compact ? 140 : 133) : compact ? 130 : 148,
                             QTextBlockFormat::ProportionalHeight);
        format.setTopMargin(heading ? 5 : 0);
        format.setBottomMargin(inTable ? 1 : inList ? (compact ? 7 : 3)
            : heading ? 5 : compact ? 9 : 9);
        if (format.intProperty(QTextFormat::BlockQuoteLevel) > 0) {
            format.setLeftMargin(qMax<qreal>(format.leftMargin(), 12));
            format.setBackground(QColor(QStringLiteral("#f4f0fc")));
        }
        cursor.mergeBlockFormat(format);
        if (heading >= 1 && heading <= 3) {
            QTextCharFormat headingFormat;
            QFont headingFont = body;
            headingFont.setPixelSize(compact
                ? (heading == 1 ? 18 : heading == 2 ? 16 : 15)
                : (heading == 1 ? 19 : heading == 2 ? 17 : 15));
            headingFont.setWeight(QFont::DemiBold);
            headingFormat.setFont(headingFont);
            headingFormat.setForeground(QColor(QStringLiteral("#231647")));
            cursor.select(QTextCursor::BlockUnderCursor);
            cursor.mergeCharFormat(headingFormat);
        }
        for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
            const QTextFragment text = fragment.fragment();
            if (!text.isValid()) continue;
            const QTextCharFormat current = text.charFormat();
            if (current.fontWeight() >= QFont::Bold) {
                QTextCursor boldCursor(document);
                boldCursor.setPosition(text.position());
                boldCursor.setPosition(text.position() + text.length(), QTextCursor::KeepAnchor);
                QTextCharFormat boldFormat;
                boldFormat.setFontLetterSpacingType(QFont::AbsoluteSpacing);
                boldFormat.setFontLetterSpacing(0.25);
                boldCursor.mergeCharFormat(boldFormat);
            }
            if (current.isAnchor()) {
                QTextCursor linkCursor(document);
                linkCursor.setPosition(text.position());
                linkCursor.setPosition(text.position() + text.length(), QTextCursor::KeepAnchor);
                QTextCharFormat linkFormat;
                linkFormat.setForeground(QColor(QStringLiteral("#7046cf")));
                linkFormat.setFontUnderline(true);
                linkCursor.mergeCharFormat(linkFormat);
            }
            if (!current.fontFixedPitch() &&
                !current.fontFamilies().toStringList().contains(QStringLiteral("monospace"))) continue;
            QTextCursor codeCursor(document);
            codeCursor.setPosition(text.position());
            codeCursor.setPosition(text.position() + text.length(), QTextCursor::KeepAnchor);
            QTextCharFormat codeFormat;
            QFont codeFont(QStringLiteral("Consolas"));
            codeFont.setPixelSize(13);
            codeFormat.setFont(codeFont);
            codeFormat.setBackground(QColor(QStringLiteral("#eee9f8")));
            codeFormat.setForeground(QColor(QStringLiteral("#5c3ba9")));
            codeCursor.mergeCharFormat(codeFormat);
        }
    }
    QSet<QTextTable *> tables;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        QTextTable *table = QTextCursor(block).currentTable();
        if (!table || tables.contains(table)) continue;
        tables.insert(table);
        QTextTableFormat format = table->format();
        format.setBorder(1);
        format.setBorderBrush(QColor(QStringLiteral("#ddd5ef")));
        format.setCellSpacing(0);
        format.setCellPadding(6);
        format.setBorderCollapse(true);
        format.setWidth(QTextLength(QTextLength::PercentageLength, 100));
        table->setFormat(format);
        for (int column = 0; column < table->columns(); ++column) {
            QTextTableCell cell = table->cellAt(0, column);
            QTextCharFormat cellFormat = cell.format();
            cellFormat.setBackground(QColor(QStringLiteral("#f1ebfd")));
            cell.setFormat(cellFormat);
        }
    }
}

class CodeHighlighter final : public QSyntaxHighlighter
{
public:
    CodeHighlighter(QTextDocument *document, QString language)
        : QSyntaxHighlighter(document), lang(language.toLower()) {}

protected:
    void highlightBlock(const QString &text) override
    {
        QColor keyword("#7956d5"), stringColor("#356e63"), number("#ad6b38"), comment("#8a829b");
        QString words;
        if (lang == "cpp" || lang == "c++" || lang == "c" || lang == "h")
            words = "auto|bool|break|case|char|class|const|continue|delete|do|double|else|enum|false|float|for|if|include|int|namespace|new|nullptr|private|protected|public|return|static|struct|switch|template|this|true|using|void|while";
        else if (lang == "python" || lang == "py")
            words = "and|as|assert|async|await|break|class|continue|def|del|elif|else|except|False|finally|for|from|if|import|in|is|lambda|None|not|or|pass|raise|return|True|try|while|with|yield";
        else if (lang == "sql")
            words = "and|as|by|case|create|delete|distinct|drop|else|end|from|group|having|insert|into|join|left|limit|not|null|on|or|order|right|select|set|table|then|update|values|when|where";
        if (!words.isEmpty()) {
            QRegularExpression rx(QStringLiteral("\\b(?:%1)\\b").arg(words), QRegularExpression::CaseInsensitiveOption);
            auto it = rx.globalMatch(text);
            while (it.hasNext()) { auto m = it.next(); setFormat(m.capturedStart(), m.capturedLength(), keyword); }
        }
        QRegularExpression numeric(QStringLiteral(R"(\b\d+(?:\.\d+)?\b)"));
        auto nums = numeric.globalMatch(text);
        while (nums.hasNext()) { auto m = nums.next(); setFormat(m.capturedStart(), m.capturedLength(), number); }
        QRegularExpression strings(QStringLiteral(R"("(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')"));
        auto literals = strings.globalMatch(text);
        while (literals.hasNext()) { auto m = literals.next(); setFormat(m.capturedStart(), m.capturedLength(), stringColor); }
        if (lang == "cpp" || lang == "c++" || lang == "c" || lang == "h" || lang == "python" || lang == "py" || lang == "sql") {
            int at = lang == "python" || lang == "py" ? text.indexOf('#') : text.indexOf("//");
            if (lang == "sql") at = text.indexOf("--");
            if (at >= 0) setFormat(at, text.size() - at, comment);
        }
        if (lang == "json") {
            QRegularExpression keys(QStringLiteral(R"("(?:\\.|[^"\\])*"(?=\s*:))"));
            auto matches = keys.globalMatch(text);
            while (matches.hasNext()) { auto m = matches.next(); setFormat(m.capturedStart(), m.capturedLength(), keyword); }
        }
    }
private:
    QString lang;
};

QString displayLanguage(const QString &raw)
{
    const QString lang = raw.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).value(0).toLower();
    if (lang == "cpp" || lang == "c++" || lang == "cc" || lang == "hpp") return "C++";
    if (lang == "py" || lang == "python") return "Python";
    if (lang == "sql") return "SQL";
    if (lang == "json") return "JSON";
    return lang.isEmpty() ? QStringLiteral("代码") : lang.left(18).toUpper();
}
}

AiMarkdownWidget::AiMarkdownWidget(const QString &markdown, QWidget *parent, bool compact)
    : QWidget(parent), sourceMarkdown(markdown), compactLayout(compact)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(9);
    const QStringList lines = markdown.split('\n', Qt::KeepEmptyParts);
    QString prose, code, language;
    Fence fence;
    auto addProse = [&]() {
        if (prose.trimmed().isEmpty()) { prose.clear(); return; }
        auto *browser = new SafeMarkdownBrowser(this);
        browser->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        browser->setFrameShape(QFrame::NoFrame);
        browser->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        browser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        browser->setOpenLinks(false);
        browser->setOpenExternalLinks(false);
        browser->setTextInteractionFlags(Qt::TextSelectableByMouse);
        browser->setStyleSheet("QTextBrowser { background:transparent; color:#30234e; border:0; padding:0; }");
        browser->document()->setMarkdown(prose.trimmed(),
            QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub)
                | QTextDocument::MarkdownFeatures(QTextDocument::MarkdownNoHTML));
        styleDocument(browser->document(), compactLayout);
        proseBlocks.append(browser);
        layout->addWidget(browser);
        plain += browser->document()->toPlainText() + '\n';
        prose.clear();
    };
    auto addCode = [&]() {
        auto *panel = new QFrame(this);
        panel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        panel->setStyleSheet("QFrame { background:#f8f6fc; border:1px solid #ded6ef; border-radius:9px; } QLabel { border:0; color:#776b91; font-size:12px; } QPushButton { border:0; color:#694ac5; background:transparent; padding:3px 5px; } QPushButton:hover { color:#4e31a9; text-decoration:underline; }");
        auto *box = new QVBoxLayout(panel);
        box->setContentsMargins(10, 7, 10, 8);
        box->setSpacing(6);
        auto *header = new QHBoxLayout;
        auto *label = new QLabel(displayLanguage(language), panel);
        header->addWidget(label);
        header->addStretch();
        auto *copy = new QPushButton(QStringLiteral("复制代码"), panel);
        copy->setCursor(Qt::PointingHandCursor);
        header->addWidget(copy);
        box->addLayout(header);
        auto *editor = new QPlainTextEdit(panel);
        editor->setReadOnly(true);
        editor->setPlainText(code);
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);
        editor->setFrameShape(QFrame::NoFrame);
        editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        QFont mono(QStringLiteral("Consolas"));
        mono.setPixelSize(13);
        editor->setFont(mono);
        editor->setStyleSheet("QPlainTextEdit { background:transparent; color:#30234e; border:0; padding:0; }");
        editor->setFixedHeight(qMax(42,
            (editor->document()->blockCount() + 1) * editor->fontMetrics().lineSpacing() + 8));
        new CodeHighlighter(editor->document(), language);
        box->addWidget(editor);
        connect(copy, &QPushButton::clicked, this, [code]() { QGuiApplication::clipboard()->setText(code); });
        layout->addWidget(panel);
        plain += code + '\n';
        code.clear();
        language.clear();
    };
    for (const QString &line : lines) {
        if (fence.length) {
            if (closingFence(line, fence)) { addCode(); fence = {}; }
            else code += line + '\n';
        } else {
            const Fence opening = openingFence(line);
            if (opening.length) { addProse(); fence = opening; language = opening.info; }
            else prose += line + '\n';
        }
    }
    if (fence.length) addCode();
    addProse();
    plain = plain.trimmed();
}

void AiMarkdownWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // A hidden QTextBrowser has no final viewport geometry. Re-measure once
    // Qt has shown the row and assigned its actual width.
    QTimer::singleShot(0, this, [this] {
        if (contentWidth <= 0 || !isVisible()) return;
        setContentWidth(contentWidth, true);
        if (auto *outer = qobject_cast<AiMarkdownWidget *>(parentWidget()))
            if (outer->contentWidth > 0) outer->setContentWidth(outer->contentWidth, true);
    });
}

void AiMarkdownWidget::setContentWidth(int width, bool force)
{
    if (!force && contentWidth == width && !streamingLabel && !renderedBody) return;
    const QSizePolicy previousPolicy = sizePolicy();
    const bool flexibleWidth = previousPolicy.horizontalPolicy() == QSizePolicy::Ignored;
    contentWidth = width;
    setFixedWidth(width);
    if (streamingLabel) {
        if (isVisible()) streamingLabel->show();
        streamingLabel->setFixedWidth(width);
        streamingLabel->setFixedHeight(qMax(streamingLabel->fontMetrics().height(),
                                           streamingLabel->heightForWidth(width)));
    }
    if (renderedBody) {
        if (isVisible()) renderedBody->show();
        renderedBody->setContentWidth(width);
    }
    const auto panels = findChildren<QFrame *>(QString(), Qt::FindDirectChildrenOnly);
    for (QFrame *panel : panels) panel->setFixedWidth(width);
    layout()->invalidate();
    layout()->activate();
    for (QTextBrowser *browser : proseBlocks) {
        browser->setFixedWidth(width);
        browser->resize(width, browser->height());
        // The horizontal bar and the viewport can change after the first
        // layout pass. Measure against the actual viewport until they settle.
        for (int pass = 0; pass < 3; ++pass) {
            browser->document()->setTextWidth(qMax(1, browser->viewport()->width()));
            const int chrome = browser->height() - browser->viewport()->height();
            const int needed = qCeil(browser->document()->documentLayout()->documentSize().height())
                + chrome + 3;
            if (browser->height() == needed) break;
            browser->setFixedHeight(needed);
            browser->resize(width, needed);
        }
    }
    for (QFrame *panel : panels) {
        auto *editor = panel->findChild<QPlainTextEdit *>();
        if (!editor) continue;
        panel->layout()->invalidate();
        panel->layout()->activate();
        for (int pass = 0; pass < 3; ++pass) {
            const int chrome = editor->height() - editor->viewport()->height();
            // QPlainTextDocumentLayout::documentSize() reports only its
            // margin here, even when there are many unwrapped code lines.
            const int linesHeight = editor->document()->blockCount()
                * editor->fontMetrics().lineSpacing();
            const int needed = qMax(42, linesHeight + chrome
                + qCeil(editor->document()->documentMargin() * 2)
                + editor->fontMetrics().lineSpacing() + 4);
            if (editor->height() == needed) break;
            editor->setFixedHeight(needed);
            panel->layout()->invalidate();
            panel->layout()->activate();
        }
    }
    layout()->invalidate();
    layout()->activate();
    // A newly inserted final Markdown child can report a stale parent
    // sizeHint during this layout pass. Its own measured height is authoritative.
    const int finalHeight = renderedBody ? renderedBody->height() : 0;
    setFixedHeight(qMax(1, qMax(layout()->sizeHint().height(), finalHeight)));
    if (flexibleWidth) {
        setMinimumWidth(0);
        setMaximumWidth(QWIDGETSIZE_MAX);
        setSizePolicy(previousPolicy);
        resize(width, height());
    }
    updateGeometry();
}

void AiMarkdownWidget::setStreamingText(const QString &text)
{
    if (renderedBody) return;
    if (!streamingLabel) {
        streamingLabel = new QLabel(this);
        streamingLabel->setTextFormat(Qt::PlainText);
        streamingLabel->setWordWrap(true);
        streamingLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        streamingLabel->setStyleSheet(compactLayout
            ? QStringLiteral("color:#30234e; font-size:14px; background:transparent;")
            : QStringLiteral("color:#30234e; font-size:14px; background:transparent;"));
        layout()->addWidget(streamingLabel);
    }
    sourceMarkdown = text;
    plain = text;
    streamingLabel->setText(text.isEmpty() ? QStringLiteral("Vivi 正在思考…") : text);
    if (contentWidth > 0) setContentWidth(contentWidth);
}

void AiMarkdownWidget::beginStreaming()
{
    if (renderedBody) {
        layout()->removeWidget(renderedBody);
        delete renderedBody;
        renderedBody = nullptr;
    }
    if (streamingLabel) {
        layout()->removeWidget(streamingLabel);
        delete streamingLabel;
        streamingLabel = nullptr;
    }
    plain.clear();
    sourceMarkdown.clear();
    setStreamingText({});
    layout()->invalidate();
    updateGeometry();
}

void AiMarkdownWidget::finishStreaming(const QString &markdown)
{
    if (renderedBody) return;
    if (streamingLabel) {
        layout()->removeWidget(streamingLabel);
        delete streamingLabel;
        streamingLabel = nullptr;
    }
    renderedBody = new AiMarkdownWidget(markdown, this, compactLayout);
    layout()->addWidget(renderedBody);
    if (isVisible()) renderedBody->show();
    sourceMarkdown = markdown;
    plain = renderedBody->plainText();
    if (contentWidth > 0) setContentWidth(contentWidth);
}
