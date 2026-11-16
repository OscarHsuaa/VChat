#pragma once

#include <QWidget>
#include <QList>

class QTextBrowser;
class QLabel;
class QShowEvent;

// Renders only Markdown's text features. Fenced code is kept in separate,
// selectable widgets; Qt's MarkdownNoHTML rejects model-supplied raw HTML.
class AiMarkdownWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AiMarkdownWidget(const QString &markdown, QWidget *parent = nullptr,
                              bool compact = false);
    void setContentWidth(int width, bool force = false);
    void setStreamingText(const QString &text);
    void beginStreaming();
    void finishStreaming(const QString &markdown);
    QString plainText() const { return plain; }
    QString markdownText() const { return sourceMarkdown; }
    bool isStreaming() const { return streamingLabel != nullptr; }

protected:
    void showEvent(QShowEvent *event) override;

private:
    QList<QTextBrowser *> proseBlocks;
    QString plain;
    QString sourceMarkdown;
    int contentWidth = 0;
    QLabel *streamingLabel = nullptr;
    AiMarkdownWidget *renderedBody = nullptr;
    bool compactLayout = false;
};
