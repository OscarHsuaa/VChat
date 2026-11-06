#include "mentioninput.h"

#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextDocument>

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
    setLineWrapMode(QTextEdit::NoWrap);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    document()->setDocumentMargin(0);
    new MentionHighlighter(document());
}

void MentionInput::paintEvent(QPaintEvent *event)
{
    QTextEdit::paintEvent(event);
    if (!document()->isEmpty() || placeholderText().isEmpty() || preediting) return;

    // QTextEdit's native placeholder is not painted with this styled viewport.
    // Draw it in the viewport so it follows the same QSS padding as typed text.
    QPainter painter(viewport());
    painter.setPen(QColor(QStringLiteral("#837a9a")));
    painter.drawText(viewport()->rect(), Qt::AlignLeft | Qt::AlignVCenter, placeholderText());
}

void MentionInput::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && !preediting) {
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
    if (wasPreediting && !preediting) emit compositionFinished();
}

void MentionInput::insertFromMimeData(const QMimeData *source)
{
    if (!source || !source->hasText()) return;
    QString text = source->text();
    text.replace(QRegularExpression(QStringLiteral("[\\r\\n]+")), QStringLiteral(" "));
    QTextCursor cursor = textCursor();
    cursor.insertText(text);
    setTextCursor(cursor);
}
