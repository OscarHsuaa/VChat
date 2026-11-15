#include "mentioninput.h"

#include <QEvent>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSyntaxHighlighter>
#include <QTextOption>
#include <QTextDocument>
#include <QTimer>
#include <cmath>

namespace {
class MentionHighlighter : public QSyntaxHighlighter
{
public:
    explicit MentionHighlighter(QTextDocument *document) : QSyntaxHighlighter(document) {}
protected:
    void highlightBlock(const QString &text) override
    {
        static const QRegularExpression mention(QStringLiteral(R"((?<![\p{L}\p{N}_])@[\p{L}\p{N}_]*)"));
        QTextCharFormat format;
        format.setForeground(QColor("#7956d5"));
        format.setFontWeight(QFont::DemiBold);
        auto matches = mention.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), format);
        }
    }
};
}

MentionInput::MentionInput(QWidget *parent) : QTextEdit(parent)
{
    setAcceptRichText(false);
    setTabChangesFocus(false);
    setLineWrapMode(QTextEdit::WidgetWidth);
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    document()->setDocumentMargin(0);
    new MentionHighlighter(document());
    connect(this, &QTextEdit::textChanged, this, &MentionInput::scheduleHeightUpdate);
}

void MentionInput::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (preediting || suppressImeReturn) {
            suppressImeReturn = false;
            event->accept();
            return;
        }
        if (event->modifiers() & Qt::ShiftModifier) {
            QTextEdit::keyPressEvent(event);
            return;
        }
        emit returnPressed();
        event->accept();
        return;
    }
    QTextEdit::keyPressEvent(event);
}

void MentionInput::inputMethodEvent(QInputMethodEvent *event)
{
    const bool wasPreediting = preediting;
    preediting = !event->preeditString().isEmpty();
    QTextEdit::inputMethodEvent(event);
    if (wasPreediting && !preediting) {
        // Some IMEs deliver the Return key after committing their candidate.
        // Ignore that key within this event-loop turn, then allow normal send.
        suppressImeReturn = true;
        QTimer::singleShot(0, this, [this]() { suppressImeReturn = false; });
        emit compositionFinished();
    }
}

void MentionInput::insertFromMimeData(const QMimeData *source)
{
    if (!source || !source->hasText()) return;
    QString text = source->text();
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    QTextCursor cursor = textCursor();
    cursor.insertText(text);
    setTextCursor(cursor);
}

void MentionInput::resizeEvent(QResizeEvent *event)
{
    QTextEdit::resizeEvent(event);
    if (event->size().width() != event->oldSize().width()) scheduleHeightUpdate();
}

void MentionInput::changeEvent(QEvent *event)
{
    QTextEdit::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
        scheduleHeightUpdate();
}

void MentionInput::scheduleHeightUpdate()
{
    if (heightUpdatePending) return;
    heightUpdatePending = true;
    QTimer::singleShot(0, this, [this]() {
        heightUpdatePending = false;
        updateInputHeight();
    });
}

void MentionInput::updateInputHeight()
{
    // QTextEdit wraps against the viewport. Its remaining height is the frame
    // and QSS padding, so it must be included in the document's height.
    const int chrome = height() - viewport()->height();
    const int oneLine = 50;
    const int fiveLines = qMax(oneLine,
        int(std::ceil(fontMetrics().lineSpacing() * 5.0)) + chrome);
    const int contentHeight = int(std::ceil(document()->size().height())) + chrome;
    const int target = qBound(oneLine, contentHeight, fiveLines);
    if (height() == target && minimumHeight() == target && maximumHeight() == target) return;
    setFixedHeight(target);
    emit inputHeightChanged();
}
