#pragma once

#include <QDialog>
#include <QJsonObject>

class QCheckBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QScrollArea;
class QShowEvent;
class QStackedWidget;
class QFrame;

class RoomPermissionsDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit RoomPermissionsDialog(const QString &roomId, bool owner, QWidget *parent = nullptr);
    QString roomId() const { return roomId_; }
    void setPermissions(const QJsonObject &settings);
    void setPending(bool pending);
    void showError(const QString &message);
signals:
    void saveRequested(const QString &roomId, const QJsonObject &settings);
private:
    void showEvent(QShowEvent *event) override;
    void updateState();
    void selectPage(int index);
    void refreshCardStyles();
    QJsonObject currentSettings() const;
    QString roomId_;
    bool owner_ = false;
    bool loaded_ = false;
    bool pending_ = false;
    QJsonObject baseline_;
    QLabel *error_ = nullptr;
    QRadioButton *private_ = nullptr;
    QRadioButton *public_ = nullptr;
    QRadioButton *direct_ = nullptr;
    QRadioButton *approval_ = nullptr;
    QCheckBox *invite_ = nullptr;
    QCheckBox *edit_ = nullptr;
    QCheckBox *kick_ = nullptr;
    QCheckBox *manage_ = nullptr;
    QPushButton *save_ = nullptr;
    QStackedWidget *pages_ = nullptr;
    QPushButton *navigation_[3] = {};
    QFrame *privateCard_ = nullptr;
    QFrame *publicCard_ = nullptr;
    QFrame *directCard_ = nullptr;
    QFrame *approvalCard_ = nullptr;
};
