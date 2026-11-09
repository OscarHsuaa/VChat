#pragma once

#include <QDialog>
#include <QJsonArray>
#include <QSet>

class QLineEdit;
class QLabel;
class QListWidget;
class QPushButton;
class QTimer;

class RoomInviteDialog : public QDialog
{
    Q_OBJECT
public:
    RoomInviteDialog(const QString &roomId, const QString &roomName, QWidget *parent = nullptr);
    QString roomId() const { return roomId_; }
    void requestSearch();
    void setResults(const QJsonArray &users);
    void setPending(bool pending);
    void showError(const QString &message);
    void showSuccess(int count);

signals:
    void searchRequested(const QString &roomId, const QString &query);
    void sendRequested(const QString &roomId, const QJsonArray &userIds);

private:
    void updateSelection();
    QString roomId_;
    QLineEdit *search_ = nullptr;
    QListWidget *results_ = nullptr;
    QLabel *empty_ = nullptr;
    QLabel *selection_ = nullptr;
    QLabel *message_ = nullptr;
    QPushButton *send_ = nullptr;
    QTimer *debounce_ = nullptr;
    QSet<QString> selected_;
    bool pending_ = false;
};

class RoomInviteInboxDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RoomInviteInboxDialog(QWidget *parent = nullptr);
    void setInvites(const QJsonArray &invites);
    void setPending(bool pending);
    void showError(const QString &message);

signals:
    void decisionRequested(const QString &inviteId, bool accept);

private:
    QListWidget *list_ = nullptr;
    QLabel *empty_ = nullptr;
    QLabel *error_ = nullptr;
    bool pending_ = false;
};
