#pragma once

#include <QWidget>
#include <QList>

class QTextBrowser;
class QLabel;

// Renders only Markdown's text features. Fenced code is kept in separate,
// selectable widgets; Qt's MarkdownNoHTML rejects model-supplied raw HTML.
class AiMarkdownWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AiMarkdownWidget(const QString &markdown, QWidget *parent = nullptr);
    void setContentWidth(int width);
    void setStreamingText(const QString &text);
    void beginStreaming();
    void finishStreaming(const QString &markdown);
    QString plainText() const { return plain; }

private:
    QList<QTextBrowser *> proseBlocks;
    QString plain;
    int contentWidth = 0;
    QLabel *streamingLabel = nullptr;
    AiMarkdownWidget *renderedBody = nullptr;
};
