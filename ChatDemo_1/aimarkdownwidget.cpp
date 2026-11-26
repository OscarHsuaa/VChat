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
#include <QStyle>
#include <QSyntaxHighlighter>
#include <QTextBrowser>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QTextFormat>
#include <QTextFragment>
#include <QTextList>
#include <QTextLayout>
#include <QTextTable>
#include <QTextTableCell>
#include <QTimer>
#include <QVBoxLayout>

namespace {
int scrollBarSpace(const QScrollBar *bar)
{
    if (bar->style()->pixelMetric(QStyle::PM_ScrollView_ScrollBarOverlap, nullptr, bar) > 0)
        return 0;
    return bar->orientation() == Qt::Vertical ? bar->sizeHint().width() : bar->sizeHint().height();
}

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
    // The compact renderer belongs to the right-side summary report. Keep
    // Markdown's separate list paragraphs, but let their text share the same
    // left edge as the surrounding prose without bullets or numbering.
    if (compact) {
        QSet<QTextList *> lists;
        for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
            QTextList *list = block.textList();
            if (!list || lists.contains(list)) continue;
            lists.insert(list);
            QTextListFormat format = list->format();
            format.setStyle(QTextListFormat::ListStyleUndefined);
            format.setIndent(0);
            list->setFormat(format);
        }
    }
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
        if (compact && inList) {
            format.setLeftMargin(0);
            format.setTextIndent(0);
        }
        // The final block already sits above the bubble's bottom padding.
        // Its paragraph gap belongs only between blocks, not after the reply.
        format.setBottomMargin(!block.next().isValid() ? 0 :
            inTable ? 1 : inList ? (compact ? 7 : 3) : heading ? 5 : 9);
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
        // Measure the styled document independently of the browser's current
        // viewport. The live document may already be wrapped to a narrow
        // width, making idealWidth() report that wrapped width instead.
        QTextDocument *unwrapped = browser->document()->clone();
        unwrapped->setTextWidth(10000);
        unwrapped->documentLayout()->documentSize();
        qreal naturalWidth = 0;
        for (QTextBlock block = unwrapped->begin(); block.isValid(); block = block.next()) {
            QTextLayout *textLayout = block.layout();
            if (!textLayout) continue;
            qreal blockWidth = block.blockFormat().leftMargin();
            if (QTextList *list = block.textList())
                blockWidth += list->format().indent() * unwrapped->indentWidth();
            for (int line = 0; line < textLayout->lineCount(); ++line)
                naturalWidth = qMax(naturalWidth,
                    blockWidth + textLayout->lineAt(line).naturalTextWidth());
        }
        // idealWidth also accounts for structures such as tables, where the
        // combined columns are wider than any single text block.
        proseNaturalWidths.append(qCeil(qMax(naturalWidth + 2 * unwrapped->documentMargin(),
                                            unwrapped->idealWidth())));
        delete unwrapped;
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
        // The newline before a closing fence terminates the last code line;
        // QPlainTextEdit would otherwise render it as an extra empty line.
        const QString displayCode = code.endsWith(QLatin1Char('\n')) ? code.chopped(1) : code;
        editor->setPlainText(displayCode);
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);
        editor->setFrameShape(QFrame::NoFrame);
        editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        QFont mono(QStringLiteral("Consolas"));
        mono.setPixelSize(13);
        editor->setFont(mono);
        editor->setStyleSheet("QPlainTextEdit { background:transparent; color:#30234e; border:0; padding:0; }");
        new CodeHighlighter(editor->document(), language);
        box->addWidget(editor);
        CodeBlock block{editor};
        if (compactLayout) {
            editor->setFixedHeight(qMax(42,
                editor->document()->blockCount() * editor->fontMetrics().lineSpacing() + 8));
        } else {
            editor->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
            editor->ensurePolished();
            constexpr int visibleCodeLines = 20;
            QTextDocument *document = editor->document();
            auto *documentLayout = document->documentLayout();
            block.limitedHeight = document->blockCount() > visibleCodeLines;
            // Full-height blocks fit all rows. A premature vertical bar on a
            // hidden editor would steal width and make both bars stick.
            if (block.limitedHeight) editor->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            // The last block already includes the bottom document margin.
            qreal height = document->documentMargin();
            if (block.limitedHeight) height += document->documentMargin();
            for (QTextBlock line = document->begin(); line.isValid(); line = line.next()) {
                const QRectF bounds = documentLayout->blockBoundingRect(line);
                if (line.blockNumber() < visibleCodeLines) height += bounds.height();
                const QTextLayout *textLayout = line.layout();
                for (int i = 0; i < textLayout->lineCount(); ++i)
                    block.textWidth = qMax(block.textWidth, textLayout->lineAt(i).naturalTextWidth());
            }
            block.textWidth += 2 * document->documentMargin();
            // QPlainTextEdit reserves one logical pixel in its vertical fit check.
            block.viewportHeight = qCeil(height) + 1;
        }
        codeBlocks.append(block);
        connect(copy, &QPushButton::clicked, this, [displayCode]() {
            QGuiApplication::clipboard()->setText(displayCode);
        });
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

int AiMarkdownWidget::naturalContentWidth() const
{
    if (renderedBody) return renderedBody->naturalContentWidth();
    int width = 0;
    if (streamingLabel) {
        const QFontMetrics metrics(streamingLabel->font());
        for (const QString &line : streamingLabel->text().split(QLatin1Char('\n')))
            width = qMax(width, metrics.horizontalAdvance(line));
    }
    for (int i = 0; i < proseBlocks.size(); ++i) {
        QTextBrowser *browser = proseBlocks.at(i);
        width = qMax(width, proseNaturalWidths.value(i)
            + browser->width() - browser->viewport()->width());
    }
    for (const auto &code : codeBlocks) {
        const auto *editor = code.editor;
        const auto *panel = editor->parentWidget();
        const QMargins layoutInsets = panel->layout()->contentsMargins();
        if (compactLayout) {
            const QFontMetrics metrics(editor->font());
            for (const QString &line : editor->toPlainText().split(QLatin1Char('\n')))
                width = qMax(width, metrics.horizontalAdvance(line) + layoutInsets.left() + layoutInsets.right());
            continue;
        }
        const int scrollbar = code.limitedHeight ? scrollBarSpace(editor->verticalScrollBar()) : 0;
        const QMargins editorInsets = editor->contentsMargins();
        const QMargins panelInsets = panel->contentsMargins() + layoutInsets;
        const int editorWidth = qCeil(code.textWidth) + scrollbar + editorInsets.left() + editorInsets.right();
        const int headerWidth = panel->layout()->itemAt(0)->minimumSize().width();
        width = qMax(width, qMax(editorWidth, headerWidth) + panelInsets.left() + panelInsets.right());
    }
    return qMax(1, width);
}

int AiMarkdownWidget::trailingTextSpace() const
{
    if (renderedBody) return renderedBody->trailingTextSpace();
    if (compactLayout || streamingLabel || layout()->count() == 0) return 0;
    auto *browser = qobject_cast<QTextBrowser *>(layout()->itemAt(layout()->count() - 1)->widget());
    if (!browser || browser->horizontalScrollBar()->isVisible()) return 0;
    QTextDocument *document = browser->document();
    const QTextBlock last = document->lastBlock();
    if (!last.isValid()) return 0;
    auto *documentLayout = document->documentLayout();
    documentLayout->documentSize();
    qreal contentBottom = documentLayout->blockBoundingRect(last).bottom();
    if (QTextTable *table = QTextCursor(last).currentTable())
        contentBottom = qMax(contentBottom, documentLayout->frameBoundingRect(table).bottom());
    // QTextBrowser needs the full document height to avoid an internal scroll
    // range. Report its trailing line-height space to the surrounding bubble
    // so that the *visible* text can have balanced top and bottom insets.
    return qMax(0, browser->viewport()->height()
        - qCeil(contentBottom + document->documentMargin()));
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
    for (const auto &code : codeBlocks) {
        auto *editor = code.editor;
        auto *panel = editor->parentWidget();
        panel->setFixedWidth(width);
        if (compactLayout) continue;
        const QMargins editorInsets = editor->contentsMargins();
        const QMargins insets = panel->contentsMargins() + panel->layout()->contentsMargins() + editorInsets;
        const int verticalBar = code.limitedHeight ? scrollBarSpace(editor->verticalScrollBar()) : 0;
        const int viewportWidth = qMax(1, width - insets.left() - insets.right() - verticalBar);
        const int horizontalBar = qFloor(code.textWidth) > viewportWidth
            ? scrollBarSpace(editor->horizontalScrollBar()) : 0;
        editor->setFixedHeight(code.viewportHeight + editorInsets.top() + editorInsets.bottom() + horizontalBar);
    }
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
    // The summary panel keeps its existing compact Markdown layout.
    if (compactLayout) {
        for (const auto &code : codeBlocks) {
            auto *editor = code.editor;
            auto *panel = editor->parentWidget();
            panel->layout()->invalidate();
            panel->layout()->activate();
            for (int pass = 0; pass < 3; ++pass) {
                const int chrome = editor->height() - editor->viewport()->height();
                const int linesHeight = editor->document()->blockCount() * editor->fontMetrics().lineSpacing();
                const int needed = qMax(42, linesHeight + chrome
                    + qCeil(editor->document()->documentMargin() * 2) + 4);
                if (editor->height() < needed) editor->setFixedHeight(needed);
                panel->layout()->invalidate();
                panel->layout()->activate();
                if (editor->verticalScrollBar()->maximum() == 0) break;
                editor->setFixedHeight(editor->height() + editor->fontMetrics().lineSpacing());
            }
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
