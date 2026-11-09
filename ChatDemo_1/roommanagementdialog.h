#pragma once

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>

class QLabel;
class QShowEvent;
class QScrollArea;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

class RoomManagementDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RoomManagementDialog(const QString &roomId, qint64 accountId, QWidget *parent = nullptr);
    QString roomId() const { return id_; }
    void setRoom(const QJsonObject &room);
    void setMembers(const QJsonArray &members);
    void showPage(const QString &page);
    void setPending(bool pending);
    void showError(const QString &message);

signals:
    void saveRequested(const QString &roomId, const QString &name, const QString &description);
    void removeRequested(const QString &roomId, const QString &userId);
    void leaveRequested(const QString &roomId);
    void dissolveRequested(const QString &roomId);

private:
    void showEvent(QShowEvent *event) override;
    bool confirm(const QString &title, const QString &message, bool requireName = false);
    void runQueuedAction();
    void refreshDetails();
    void refreshMembers();
    void fitCurrentPage();
    QString id_;
    qint64 accountId_ = 0;
    QJsonObject room_;
    QJsonArray members_;
    bool membersLoaded_ = false;
    bool pending_ = false;
    QString queuedAction_;
    QStackedWidget *pages_ = nullptr;
    QLabel *error_ = nullptr;
    QLabel *nameLabel_ = nullptr;
    QLabel *headerTitle_ = nullptr;
    QLabel *headerIcon_ = nullptr;
    QLabel *typeIcon_ = nullptr;
    QLabel *memberTitle_ = nullptr;
    QLabel *descriptionCount_ = nullptr;
    QLabel *typeLabel_ = nullptr;
    QLabel *ownerLabel_ = nullptr;
    QLabel *countLabel_ = nullptr;
    QLabel *descriptionLabel_ = nullptr;
    QLabel *createdLabel_ = nullptr;
    QLineEdit *nameEdit_ = nullptr;
    QPlainTextEdit *descriptionEdit_ = nullptr;
    QVBoxLayout *membersLayout_ = nullptr;
    QScrollArea *membersScroll_ = nullptr;
    QPushButton *saveButton_ = nullptr;
    QPushButton *editButton_ = nullptr;
    QPushButton *manageButton_ = nullptr;
    QPushButton *leaveButton_ = nullptr;
    QPushButton *dissolveButton_ = nullptr;
};
