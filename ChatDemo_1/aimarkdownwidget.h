#pragma once

#include <QWidget>
#include <QList>

class QTextBrowser;

// Renders only Markdown's text features. Fenced code is kept in separate,
// selectable widgets; Qt's MarkdownNoHTML rejects model-supplied raw HTML.
class AiMarkdownWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AiMarkdownWidget(const QString &markdown, QWidget *parent = nullptr);
    void setContentWidth(int width);
    QString plainText() const { return plain; }

private:
    QList<QTextBrowser *> proseBlocks;
    QString plain;
    int contentWidth = 0;
};
