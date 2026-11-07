#include "aimarkdownwidget.h"

#include <QClipboard>
#include <QFont>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSyntaxHighlighter>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextFormat>
#include <QVBoxLayout>

namespace {
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

AiMarkdownWidget::AiMarkdownWidget(const QString &markdown, QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(9);
    const QStringList lines = markdown.split('\n', Qt::KeepEmptyParts);
    QString prose, code, language;
    bool inCode = false;
    auto addProse = [&]() {
        if (prose.trimmed().isEmpty()) { prose.clear(); return; }
        auto *browser = new QTextBrowser(this);
        browser->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        browser->setFrameShape(QFrame::NoFrame);
        browser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        browser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        browser->setOpenLinks(false);
        browser->setOpenExternalLinks(false);
        browser->setTextInteractionFlags(Qt::TextSelectableByMouse);
        browser->setStyleSheet("QTextBrowser { background:transparent; color:#30234e; border:0; padding:0; font-size:14px; }");
        browser->document()->setDocumentMargin(0);
        browser->document()->setMarkdown(prose.trimmed(), QTextDocument::MarkdownNoHTML);
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
        editor->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        QFont mono(QStringLiteral("Consolas"));
        mono.setPixelSize(13);
        editor->setFont(mono);
        editor->setStyleSheet("QPlainTextEdit { background:transparent; color:#30234e; border:0; padding:0; }");
        editor->setFixedHeight(qMin(242, qMax(34, (code.count('\n') + 2) * editor->fontMetrics().lineSpacing() + 8)));
        new CodeHighlighter(editor->document(), language);
        box->addWidget(editor);
        connect(copy, &QPushButton::clicked, this, [code]() { QGuiApplication::clipboard()->setText(code); });
        layout->addWidget(panel);
        plain += code + '\n';
        code.clear();
        language.clear();
    };
    for (const QString &line : lines) {
        if (line.startsWith("```")) {
            if (inCode) { addCode(); inCode = false; }
            else { addProse(); language = line.mid(3); inCode = true; }
        } else if (inCode) code += line + '\n';
        else prose += line + '\n';
    }
    if (inCode) { prose += "```" + language + '\n' + code; code.clear(); }
    addProse();
    plain = plain.trimmed();
}

void AiMarkdownWidget::setContentWidth(int width)
{
    if (contentWidth == width && !streamingLabel && !renderedBody) return;
    contentWidth = width;
    setFixedWidth(width);
    if (streamingLabel) {
        streamingLabel->setFixedWidth(width);
        streamingLabel->setFixedHeight(qMax(streamingLabel->fontMetrics().height(),
                                           streamingLabel->heightForWidth(width)));
    }
    if (renderedBody) renderedBody->setContentWidth(width);
    for (QTextBrowser *browser : proseBlocks) {
        browser->setFixedWidth(width);
        browser->document()->setTextWidth(width);
        browser->setFixedHeight(qCeil(browser->document()->size().height()) + 2);
    }
    for (QFrame *panel : findChildren<QFrame *>(QString(), Qt::FindDirectChildrenOnly))
        panel->setFixedWidth(width);
    layout()->invalidate();
    layout()->activate();
    // A newly inserted final Markdown child can report a stale parent
    // sizeHint during this layout pass. Its own measured height is authoritative.
    const int finalHeight = renderedBody ? renderedBody->height() : 0;
    setFixedHeight(qMax(1, qMax(layout()->sizeHint().height(), finalHeight)));
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
        streamingLabel->setStyleSheet(QStringLiteral("color:#30234e; font-size:14px; background:transparent;"));
        layout()->addWidget(streamingLabel);
    }
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
    renderedBody = new AiMarkdownWidget(markdown, this);
    layout()->addWidget(renderedBody);
    plain = renderedBody->plainText();
    if (contentWidth > 0) setContentWidth(contentWidth);
}
