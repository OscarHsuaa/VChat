#ifndef ROOMCREATEDIALOG_H
#define ROOMCREATEDIALOG_H

#include <QDialog>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QRadioButton;
class QPushButton;
class QShowEvent;

class RoomCreateDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RoomCreateDialog(QWidget *parent = nullptr);
    void setPending(bool pending);
    void showError(const QString &message);

signals:
    void createSubmitted(const QString &name, const QString &description, const QString &type);

protected:
    void reject() override;
    void showEvent(QShowEvent *event) override;

private:
    QLineEdit *name_ = nullptr;
    QPlainTextEdit *description_ = nullptr;
    QRadioButton *invite_ = nullptr;
    QPushButton *submit_ = nullptr;
    QPushButton *cancel_ = nullptr;
    QLabel *error_ = nullptr;
    bool pending_ = false;
};

#endif
