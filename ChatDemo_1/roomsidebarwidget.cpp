#include "roomsidebarwidget.h"

#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QSvgRenderer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QIcon sidebarIcon(const QString &path)
{
    QSvgRenderer renderer(path);
    QPixmap image(44, 44);
    image.setDevicePixelRatio(2);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    renderer.render(&painter, QRectF(0, 0, 22, 22));
    return QIcon(image);
}

QIcon sidebarChevron(bool pointsRight)
{
    QPixmap image(40, 40);
    image.setDevicePixelRatio(2);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#7956d5"), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (pointsRight) {
        painter.drawLine(QPointF(7, 5), QPointF(12, 10));
        painter.drawLine(QPointF(12, 10), QPointF(7, 15));
    } else {
        painter.drawLine(QPointF(12, 5), QPointF(7, 10));
        painter.drawLine(QPointF(7, 10), QPointF(12, 15));
    }
    return QIcon(image);
}
}

RoomSidebarWidget::RoomSidebarWidget(QWidget *parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("roomSidebar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral(
        "QFrame#roomSidebar { background:#fcfaff; border:1px solid #eee8f8; border-radius:18px; }"
        "QListWidget#roomList { background:transparent; border:0; outline:0; }"
        "QListWidget#roomList::item { border-radius:12px; margin:2px 0; }"
        "QListWidget#roomList::item:hover { background:#f5f0fd; }"
        "QListWidget#roomList::item:selected { background:#eee6fb; border:1px solid #b69bf2; }"
        "QToolButton { background:transparent; border:0; border-radius:9px; color:#6951aa; padding:6px; }"
        "QToolButton:hover { background:#f0e9fc; }"
        "QToolButton:disabled { color:#aaa0bc; }"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 14, 12, 15);
    root->setSpacing(10);
    auto *heading = new QHBoxLayout;
    title_ = new QLabel(QStringLiteral("我的聊天室"), this);
    title_->setStyleSheet(QStringLiteral("color:#252044; font-size:15px; font-weight:600;"));
    heading->addWidget(title_, 1);
    toggle_ = new QToolButton(this);
    toggle_->setIcon(sidebarChevron(false));
    toggle_->setIconSize(QSize(20, 20));
    toggle_->setToolTip(QStringLiteral("收起聊天室导航"));
    toggle_->setCursor(Qt::PointingHandCursor);
    heading->addWidget(toggle_);
    root->addLayout(heading);
    connect(toggle_, &QToolButton::clicked, this, [this]() {
        setCollapsed(!collapsed_);
        emit collapsedChanged(collapsed_);
    });

    notice_ = new QLabel(QStringLiteral("正在加载聊天室…"), this);
    notice_->setWordWrap(true);
    notice_->setStyleSheet(QStringLiteral("color:#827799; font-size:12px;"));
    root->addWidget(notice_);
    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("roomList"));
    list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    root->addWidget(list_, 1);
    connect(list_, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const QString requested = item->data(Qt::UserRole).toString();
        setCurrentRoom(currentRoom_); // Selection changes only after server confirmation.
        if (!requested.isEmpty()) emit roomSelected(requested);
    });

    auto *divider = new QFrame(this);
    divider->setFixedHeight(1);
    divider->setStyleSheet(QStringLiteral("background:#eee8f8; border:0;"));
    root->addWidget(divider);
    create_ = new QToolButton(this);
    create_->setObjectName(QStringLiteral("createRoomButton"));
    create_->setText(QStringLiteral("＋  创建聊天室"));
    create_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    create_->setToolTip(QStringLiteral("创建聊天室"));
    create_->setCursor(Qt::PointingHandCursor);
    create_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    root->addWidget(create_);
    connect(create_, &QToolButton::clicked, this, &RoomSidebarWidget::createRequested);
    discover_ = new QToolButton(this);
    discover_->setText(QStringLiteral("◇  发现公开聊天室"));
    discover_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    discover_->setToolTip(QStringLiteral("发现公开聊天室将在后续版本开放"));
    discover_->setEnabled(false);
    discover_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    root->addWidget(discover_);
    setCollapsed(false);
}

void RoomSidebarWidget::setRooms(const QJsonArray &rooms)
{
    rooms_ = rooms;
    rebuild();
}

void RoomSidebarWidget::setCurrentRoom(const QString &roomId)
{
    currentRoom_ = roomId;
    for (int i = 0; i < list_->count(); ++i) {
        auto *item = list_->item(i);
        item->setSelected(item->data(Qt::UserRole).toString() == roomId);
    }
}

void RoomSidebarWidget::setCollapsed(bool collapsed)
{
    collapsed_ = collapsed;
    setFixedWidth(collapsed ? 62 : 230);
    title_->setVisible(!collapsed);
    notice_->setVisible(!collapsed && !notice_->text().isEmpty());
    toggle_->setIcon(sidebarChevron(collapsed));
    toggle_->setToolTip(collapsed ? QStringLiteral("展开聊天室导航") : QStringLiteral("收起聊天室导航"));
    create_->setText(collapsed ? QStringLiteral("＋") : QStringLiteral("＋  创建聊天室"));
    discover_->setText(collapsed ? QStringLiteral("◇") : QStringLiteral("◇  发现公开聊天室"));
    rebuild();
}

void RoomSidebarWidget::setNotice(const QString &notice)
{
    notice_->setText(notice);
    notice_->setVisible(!collapsed_ && !notice.isEmpty());
}

void RoomSidebarWidget::rebuild()
{
    list_->clear();
    for (const QJsonValue &value : rooms_) {
        const QJsonObject room = value.toObject();
        const QString id = room.value(QStringLiteral("room_id")).toString();
        const QString name = room.value(QStringLiteral("room_name")).toString();
        if (id.isEmpty() || name.isEmpty()) continue;
        const QString description = room.value(QStringLiteral("description")).toString();
        auto *item = new QListWidgetItem(list_);
        item->setData(Qt::UserRole, id);
        item->setToolTip(description.isEmpty() ? name : name + QStringLiteral("\n") + description);
        item->setSizeHint(QSize(0, collapsed_ ? 50 : 68));
        auto *row = new QWidget(list_);
        row->setMinimumHeight(collapsed_ ? 48 : 66);
        row->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(collapsed_ ? 4 : 8, 4, 4, 4);
        layout->setSpacing(10);
        auto *icon = new QLabel(row);
        icon->setFixedSize(32, 32);
        icon->setAlignment(Qt::AlignCenter);
        icon->setPixmap(sidebarIcon(QStringLiteral(":/login/room.svg")).pixmap(21, 21));
        icon->setStyleSheet(QStringLiteral("background:#eee5fc; border-radius:16px;"));
        layout->addWidget(icon);
        if (!collapsed_) {
            auto *texts = new QVBoxLayout;
            texts->setSpacing(2);
            auto *nameLabel = new QLabel(name, row);
            nameLabel->setTextFormat(Qt::PlainText);
            nameLabel->setStyleSheet(QStringLiteral("color:#292342; font-size:13px; font-weight:600;"));
            texts->addWidget(nameLabel);
            if (!description.isEmpty()) {
                auto *descriptionLabel = new QLabel(description, row);
                descriptionLabel->setTextFormat(Qt::PlainText);
                descriptionLabel->setStyleSheet(QStringLiteral("color:#837a9a; font-size:11px;"));
                texts->addWidget(descriptionLabel);
            }
            layout->addLayout(texts, 1);
        }
        list_->setItemWidget(item, row);
    }
    setCurrentRoom(currentRoom_);
}
