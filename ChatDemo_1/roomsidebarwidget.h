#ifndef ROOMSIDEBARWIDGET_H
#define ROOMSIDEBARWIDGET_H

#include <QFrame>
#include <QJsonArray>

class QLabel;
class QListWidget;
class QToolButton;

class RoomSidebarWidget : public QFrame
{
    Q_OBJECT
public:
    explicit RoomSidebarWidget(QWidget *parent = nullptr);
    void setRooms(const QJsonArray &rooms);
    void setCurrentRoom(const QString &roomId);
    void setCollapsed(bool collapsed);
    void setNotice(const QString &notice);
    bool isCollapsed() const { return collapsed_; }

signals:
    void roomSelected(const QString &roomId);
    void collapsedChanged(bool collapsed);

private:
    void rebuild();
    QJsonArray rooms_;
    QString currentRoom_;
    bool collapsed_ = false;
    QLabel *title_ = nullptr;
    QLabel *notice_ = nullptr;
    QListWidget *list_ = nullptr;
    QToolButton *toggle_ = nullptr;
    QToolButton *create_ = nullptr;
    QToolButton *discover_ = nullptr;
};

#endif
