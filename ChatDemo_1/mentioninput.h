#pragma once

#include <QTextEdit>

class MentionInput : public QTextEdit
{
    Q_OBJECT
public:
    explicit MentionInput(QWidget *parent = nullptr);
    bool composing() const { return preediting; }

signals:
    void returnPressed();
    void compositionFinished();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void insertFromMimeData(const QMimeData *source) override;

private:
    bool preediting = false;
};
