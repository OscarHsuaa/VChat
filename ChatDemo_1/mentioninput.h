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
    void inputHeightChanged();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void insertFromMimeData(const QMimeData *source) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void scheduleHeightUpdate();
    void updateInputHeight();
    bool preediting = false;
    bool heightUpdatePending = false;
    bool suppressImeReturn = false;
};
