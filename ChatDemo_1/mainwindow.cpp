#include "mainwindow.h"
#include "aimarkdownwidget.h"
#include "mentioninput.h"
#include "roomsidebarwidget.h"
#include "roomcreatedialog.h"
#include "roominvitedialog.h"
#include "roommanagementdialog.h"
#include "roompermissionsdialog.h"
#include "ui_mainwindow.h"

#include <QFont>
#include <QFrame>
#include <QApplication>
#include <QEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QGuiApplication>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStyle>
#include <QSvgRenderer>
#include <QTimer>
#include <QToolButton>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextBoundaryFinder>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QUuid>
#include <algorithm>

namespace {
QIcon iconFromSvg(const QString &path, int size = 20)
{
    QSvgRenderer renderer(path);
    QPixmap pixmap(size * 2, size * 2);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(0, 0, size, size));
    return QIcon(pixmap);
}

QIcon inviteBellIcon(bool hasPending)
{
    QPixmap pixmap(44, 44);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QSvgRenderer renderer(QStringLiteral(":/login/bell.svg"));
    renderer.render(&painter, QRectF(1, 2, 18, 18));
    if (hasPending) {
        painter.setBrush(QColor("#d85d83"));
        painter.setPen(QPen(Qt::white, 1.2));
        painter.drawEllipse(QPointF(17, 4), 3.3, 3.3);
    }
    return QIcon(pixmap);
}

QIcon memberChevron(bool pointsLeft)
{
    QPixmap pixmap(40, 40);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor("#7956d5"), 2.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    if (pointsLeft) {
        painter.drawLine(QPointF(12, 5), QPointF(7, 10));
        painter.drawLine(QPointF(7, 10), QPointF(12, 15));
    } else {
        painter.drawLine(QPointF(8, 5), QPointF(13, 10));
        painter.drawLine(QPointF(13, 10), QPointF(8, 15));
    }
    return QIcon(pixmap);
}

QPixmap chatLogo()
{
    QPixmap pixmap(100, 100);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath bubble;
    bubble.addRoundedRect(QRectF(8, 7, 83, 71), 23, 23);
    bubble.moveTo(25, 72);
    bubble.lineTo(20, 91);
    bubble.lineTo(45, 77);
    QLinearGradient fill(10, 8, 88, 80);
    fill.setColorAt(0, QColor("#8765e9"));
    fill.setColorAt(1, QColor("#a875e6"));
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawPath(bubble);
    painter.setBrush(Qt::white);
    for (int x : {31, 50, 69})
        painter.drawEllipse(QPointF(x, 43), 5, 5);
    return pixmap;
}

QLabel *avatarLabel(const QString &name, QWidget *parent, int size = 36)
{
    auto *avatar = new QLabel(name.left(1).isEmpty() ? QStringLiteral("我") : name.left(1), parent);
    avatar->setTextFormat(Qt::PlainText);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setFixedSize(size, size);
    avatar->setStyleSheet(QStringLiteral("color:#694ac5; background:#eee5fc; border-radius:%1px; font-size:%2px; font-weight:600;")
                              .arg(size / 2).arg(size <= 36 ? 16 : 17));
    return avatar;
}

class SystemPill : public QFrame
{
public:
    explicit SystemPill(QWidget *parent, const QColor &color = QColor("#f1ecfc"))
        : QFrame(parent), fillColor(color) {}

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fillColor);
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 20, 20);
    }
private:
    QColor fillColor;
};
}

void HeaderWaveFrame::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(rect(), 18, 18);
    painter.setClipPath(clip);
    QLinearGradient base(0, 0, width(), height());
    base.setColorAt(0, QColor("#fefbfd"));
    base.setColorAt(0.55, QColor("#faf5ff"));
    base.setColorAt(1, QColor("#fffafd"));
    painter.fillRect(rect(), base);

    const qreal w = width();
    const qreal h = height();
    QPainterPath back;
    back.moveTo(0, h * 0.55);
    back.cubicTo(w * 0.16, h * 0.15, w * 0.24, h * 1.05, w * 0.43, h * 0.48);
    back.cubicTo(w * 0.61, h * -0.01, w * 0.75, h * 0.9, w, h * 0.30);
    back.lineTo(w, h);
    back.lineTo(0, h);
    back.closeSubpath();
    painter.fillPath(back, QColor(240, 232, 252, 145));

    QPainterPath front;
    front.moveTo(0, h * 0.86);
    front.cubicTo(w * 0.16, h * 0.34, w * 0.29, h * 1.26, w * 0.48, h * 0.74);
    front.cubicTo(w * 0.68, h * 0.30, w * 0.83, h * 1.17, w, h * 0.61);
    front.lineTo(w, h);
    front.lineTo(0, h);
    front.closeSubpath();
    painter.fillPath(front, QColor(237, 227, 250, 110));
}

void RoundedLabel::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);
    painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), height() / 2.0, height() / 2.0);
    painter.end();
    QLabel::paintEvent(event);
}

MainWindow::MainWindow(const QString &loginName, const QString &serverAddress, QWidget *parent,
                       qint64 accountId, const QString &joinToken)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , nickname(loginName.trimmed().isEmpty() ? QStringLiteral("User") : loginName.trimmed())
    , ownAccountId(accountId)
    , sessionToken(joinToken)
    , isConnected(false)
    , socket(new QTcpSocket(this))
{
    ui->setupUi(this);
    roomSettingsKey = QStringLiteral("rooms/last/%1/%2").arg(ownAccountId).arg(
        QString::fromLatin1(QCryptographicHash::hash(serverAddress.toUtf8(), QCryptographicHash::Sha256).toHex()));
    aiTypewriterTimer = new QTimer(this);
    aiTypewriterTimer->setInterval(20);
    connect(aiTypewriterTimer, &QTimer::timeout, this, &MainWindow::advanceAiTypewriter);

    // 主窗口创建后，先设置界面，再连接网络信号，最后根据登录界面的服务器地址自动连接。
    setupUI();
    connectSlots();
    updateUserList();

    appendSystemMessage(QStringLiteral("欢迎来到 VChat 聊天室"));

    if (serverAddress.trimmed().isEmpty()) {
        appendSystemMessage(QStringLiteral("请在设置中连接聊天服务器"));
    } else {
        connectToServerAddress(serverAddress, false);
    }
}

MainWindow::~MainWindow()
{
    // QTcpSocket may emit disconnected while its QObject parent is destroyed.
    // Stop UI slots before destroying the widgets they access.
    socket->disconnect(this);
    socket->abort();
    delete ui;
}

void MainWindow::setupUI()
{
    setWindowTitle(QStringLiteral("VChat - 聊天室"));
    menuBar()->hide();
    statusBar()->hide();
    ui->logoLabel->setPixmap(chatLogo().scaled(50, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    roomSidebar = new RoomSidebarWidget(ui->centralwidget);
    ui->bodyLayout->insertWidget(0, roomSidebar);
    ui->bodyLayout->setStretch(0, 0);
    ui->bodyLayout->setStretch(1, 3);
    ui->bodyLayout->setStretch(2, 1);
    QSettings roomSettings(QStringLiteral("VChat"), QStringLiteral("VChatClient"));
    roomSidebar->setCollapsed(roomSettings.value(QStringLiteral("rooms/sidebarCollapsed"), false).toBool());
    connect(roomSidebar, &RoomSidebarWidget::collapsedChanged, this, [this](bool collapsed) {
        QSettings settings(QStringLiteral("VChat"), QStringLiteral("VChatClient"));
        settings.setValue(QStringLiteral("rooms/sidebarCollapsed"), collapsed);
        QTimer::singleShot(0, this, &MainWindow::updateBubbleWidths);
    });
    connect(roomSidebar, &RoomSidebarWidget::roomSelected, this, &MainWindow::requestRoomSwitch);
    connect(roomSidebar, &RoomSidebarWidget::createRequested, this, &MainWindow::openCreateRoomDialog);
    roomCreateTimer = new QTimer(this);
    roomCreateTimer->setSingleShot(true);
    connect(roomCreateTimer, &QTimer::timeout, this, [this]() {
        if (!createRoomInFlight) return;
        if (roomCreateDialog) {
            roomCreateDialog->setPending(false);
            roomCreateDialog->showError(QStringLiteral("服务器响应超时，创建结果尚未确认。请检查聊天室列表；若持续无响应，请重新连接后再试。"));
        }
        sendRoomRequest(QStringLiteral("list_mine"));
    });
    memberCollapseButton = new QToolButton(ui->rightPanel);
    memberCollapseButton->setObjectName(QStringLiteral("memberCollapseButton"));
    memberCollapseButton->setIcon(memberChevron(false));
    memberCollapseButton->setIconSize(QSize(20, 20));
    memberCollapseButton->setToolTip(QStringLiteral("收起成员列表"));
    memberCollapseButton->setCursor(Qt::PointingHandCursor);
    memberCollapseButton->setFixedSize(28, 28);
    memberCollapseButton->setStyleSheet(QStringLiteral(
        "QToolButton { color:#7956d5; background:transparent; border:0; border-radius:8px; font-size:16px; }"
        "QToolButton:hover { background:#f0e9fc; }"));
    ui->memberHeader->addWidget(memberCollapseButton);
    connect(memberCollapseButton, &QToolButton::clicked, this, [this]() { setMembersCollapsed(true); });
    memberExpandButton = new QToolButton(ui->centralwidget);
    memberExpandButton->setObjectName(QStringLiteral("memberExpandButton"));
    memberExpandButton->setIcon(memberChevron(true));
    memberExpandButton->setIconSize(QSize(20, 20));
    memberExpandButton->setToolTip(QStringLiteral("展开成员列表"));
    memberExpandButton->setCursor(Qt::PointingHandCursor);
    memberExpandButton->setFixedWidth(40);
    memberExpandButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    memberExpandButton->setStyleSheet(QStringLiteral(
        "QToolButton { color:#7956d5; background:#fcfaff; border:1px solid #eee8f8; border-radius:13px; font-size:17px; }"
        "QToolButton:hover { background:#f0e9fc; }"));
    ui->bodyLayout->insertWidget(2, memberExpandButton);
    connect(memberExpandButton, &QToolButton::clicked, this, [this]() { setMembersCollapsed(false); });
    memberExpandButton->hide();
    ui->bodyLayout->setStretch(3, 1);
    setMembersCollapsed(roomSettings.value(QStringLiteral("members/collapsed/%1").arg(ownAccountId), false).toBool());
    ui->memberHeader->setSpacing(4);
    ui->memberSpacer->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum);
    ui->memberHeader->invalidate();
    ui->rightLayout->setContentsMargins(12, 18, 12, 20);
    ui->onlineBadge->setStyleSheet(QStringLiteral(
        "color:#6c52bc; padding:4px 6px; font-size:11px; font-weight:600;"));
    roomInfoBar = new QFrame(ui->chatPanel);
    roomInfoBar->setObjectName(QStringLiteral("roomInfoBar"));
    roomInfoBar->setStyleSheet(QStringLiteral(
        "QFrame#roomInfoBar { background:#fdfbff; border-bottom:1px solid #eee8f8; }"
        "QPushButton#inviteMembersButton { background:#8064d9; color:white; border:0;"
        " border-radius:9px; padding:8px 13px; font-weight:600; }"
        "QPushButton#inviteMembersButton:hover { background:#9275e8; }"
        "QPushButton#inviteMembersButton:disabled { background:#d8d1e7; }"));
    auto *roomBarLayout = new QHBoxLayout(roomInfoBar);
    roomBarLayout->setContentsMargins(18, 10, 18, 10);
    roomBarLayout->setSpacing(10);
    auto *roomText = new QVBoxLayout;
    roomText->setSpacing(2);
    auto *roomTitleRow = new QHBoxLayout;
    roomTitleRow->setSpacing(8);
    roomNameLabel = new QLabel(roomInfoBar);
    roomNameLabel->setObjectName(QStringLiteral("roomNameLabel"));
    roomNameLabel->setStyleSheet(QStringLiteral("color:#252044; font-size:15px; font-weight:600;"));
    roomTitleRow->addWidget(roomNameLabel);
    roomTitleRow->addStretch();
    roomText->addLayout(roomTitleRow);
    roomDescriptionLabel = new QLabel(roomInfoBar);
    roomDescriptionLabel->setStyleSheet(QStringLiteral("color:#827799; font-size:11px;"));
    roomDescriptionLabel->setTextFormat(Qt::PlainText);
    roomText->addWidget(roomDescriptionLabel);
    roomBarLayout->addLayout(roomText, 1);
    inviteMembersButton = new QPushButton(QStringLiteral("邀请成员"), roomInfoBar);
    inviteMembersButton->setObjectName(QStringLiteral("inviteMembersButton"));
    inviteMembersButton->setCursor(Qt::PointingHandCursor);
    roomBarLayout->addWidget(inviteMembersButton);
    connect(inviteMembersButton, &QPushButton::clicked, this, &MainWindow::openInviteDialog);
    roomMoreButton = new QToolButton(roomInfoBar);
    roomMoreButton->setObjectName(QStringLiteral("roomMoreButton"));
    roomMoreButton->setText(QStringLiteral("···"));
    roomMoreButton->setToolTip(QStringLiteral("聊天室管理"));
    roomMoreButton->setCursor(Qt::PointingHandCursor);
    roomMoreButton->setFixedSize(32, 32);
    roomMoreButton->setStyleSheet(QStringLiteral(
        "QToolButton { color:#705aab; background:#f4effb; border:1px solid #e8def6;"
        " border-radius:9px; font-size:18px; } QToolButton:hover { background:#e9def9; }"));
    roomBarLayout->addWidget(roomMoreButton);
    connect(roomMoreButton, &QToolButton::clicked, this, [this]() {
        if (!isConnected || switchInFlight || roomHistoryLoading ||
            currentRoomId == QStringLiteral("class-room")) return;
        auto *menu = new QMenu(roomMoreButton);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->setStyleSheet(QStringLiteral(
            "QMenu { background:white; border:1px solid #e8def6; border-radius:11px;"
            " padding:5px 0px; color:#352b59; font-family:'Microsoft YaHei UI'; font-size:13px; }"
            "QPushButton#roomMenuRow { background:transparent; border:0; border-radius:7px; padding:0; }"
            "QPushButton#roomMenuRow:hover { background:#f0e9fc; }"
            "QPushButton#roomMenuRow:pressed { background:#e5daf9; }"
            "QPushButton#roomMenuRow[danger='true']:hover { background:#fff0f3; }"
            "QPushButton#roomMenuRow[danger='true']:pressed { background:#fce3ea; }"));
        auto *shadow = new QGraphicsDropShadowEffect(menu);
        shadow->setBlurRadius(18);
        shadow->setOffset(0, 5);
        shadow->setColor(QColor(73, 47, 129, 35));
        menu->setGraphicsEffect(shadow);
        auto addRow = [this, menu](const QString &text, const QString &page,
                                   const QString &icon, bool danger = false) {
            auto *button = new QPushButton(menu);
            button->setObjectName(QStringLiteral("roomMenuRow"));
            button->setProperty("danger", danger);
            button->setAccessibleName(text);
            button->setCursor(Qt::PointingHandCursor);
            button->setFixedHeight(44);
            button->setMinimumWidth(194);
            auto *row = new QHBoxLayout(button);
            row->setContentsMargins(16, 0, 16, 0);
            row->setSpacing(12);
            auto *iconArea = new QLabel(button);
            iconArea->setObjectName(QStringLiteral("roomMenuIcon"));
            iconArea->setFixedSize(20, 20);
            iconArea->setAlignment(Qt::AlignCenter);
            iconArea->setPixmap(iconFromSvg(QStringLiteral(":/login/") + icon, 18).pixmap(18, 18));
            iconArea->setAttribute(Qt::WA_TransparentForMouseEvents);
            row->addWidget(iconArea);
            auto *caption = new QLabel(text, button);
            caption->setObjectName(QStringLiteral("roomMenuText"));
            caption->setStyleSheet(danger
                ? QStringLiteral("color:#c44764; font-size:13px; background:transparent;")
                : QStringLiteral("color:#352b59; font-size:13px; background:transparent;"));
            caption->setAttribute(Qt::WA_TransparentForMouseEvents);
            row->addWidget(caption, 1);
            auto *action = new QWidgetAction(menu);
            action->setText(text);
            action->setDefaultWidget(button);
            menu->addAction(action);
            connect(button, &QPushButton::clicked, action, &QAction::trigger);
            connect(action, &QAction::triggered, this, [this, menu, page]() {
                menu->close();
                openRoomManagement(page);
            });
        };
        auto addDivider = [menu]() {
            auto *container = new QWidget(menu);
            container->setFixedHeight(11);
            auto *lineLayout = new QHBoxLayout(container);
            lineLayout->setContentsMargins(16, 5, 16, 5);
            auto *line = new QFrame(container);
            line->setFixedHeight(1);
            line->setStyleSheet(QStringLiteral("background:#eee8f7; border:0;"));
            lineLayout->addWidget(line);
            auto *action = new QWidgetAction(menu);
            action->setDefaultWidget(container);
            menu->addAction(action);
        };
        addRow(QStringLiteral("聊天室详情"), QStringLiteral("details"), QStringLiteral("room.svg"));
        bool owner = false;
        bool canKick = false;
        for (const QJsonValue &value : joinedRooms) {
            const QJsonObject room = value.toObject();
            if (room.value(QStringLiteral("room_id")).toString() != currentRoomId) continue;
            owner = room.value(QStringLiteral("owner_id")).toString().toLongLong() == ownAccountId;
            canKick = owner || room.value(QStringLiteral("allow_member_kick")).toBool();
            break;
        }
        addRow(canKick ? QStringLiteral("成员管理") : QStringLiteral("查看成员"),
            QStringLiteral("members"), QStringLiteral("members.svg"));
        addDivider();
        addRow(owner ? QStringLiteral("解散聊天室") : QStringLiteral("退出聊天室"),
            owner ? QStringLiteral("dissolve") : QStringLiteral("leave"),
            owner ? QStringLiteral("trash.svg") : QStringLiteral("log-out.svg"), true);
        const QSize menuSize = menu->sizeHint();
        const QRect bounds = screen() ? screen()->availableGeometry()
                                      : QRect(mapToGlobal(QPoint(0, 0)), size());
        QPoint pos = roomMoreButton->mapToGlobal(QPoint(roomMoreButton->width() - menuSize.width(),
                                                       roomMoreButton->height() + 5));
        pos.setX(qBound(bounds.left(), pos.x(), bounds.right() - menuSize.width() + 1));
        if (pos.y() + menuSize.height() > bounds.bottom())
            pos.setY(roomMoreButton->mapToGlobal(QPoint(0, -menuSize.height() - 5)).y());
        pos.setY(qBound(bounds.top(), pos.y(), bounds.bottom() - menuSize.height() + 1));
        menu->popup(pos);
    });
    managementTimer = new QTimer(this);
    managementTimer->setSingleShot(true);
    connect(managementTimer, &QTimer::timeout, this, [this]() {
        if (!managementRequestId.isEmpty() && roomManagementDialog)
            roomManagementDialog->showError(QStringLiteral(
                "服务器尚未确认操作结果。请刷新聊天室列表核对状态，避免重复提交。"));
    });
    permissionsTimer = new QTimer(this);
    permissionsTimer->setSingleShot(true);
    connect(permissionsTimer, &QTimer::timeout, this, [this]() {
        if (!permissionsSaveId.isEmpty() && roomPermissionsDialog) {
            roomPermissionsDialog->setPending(false);
            roomPermissionsDialog->showError(QStringLiteral(
                "服务器尚未确认保存结果，请刷新权限配置后再修改，避免覆盖未知状态。"));
        }
        permissionsSaveId.clear();
    });
    ui->chatPanelLayout->insertWidget(0, roomInfoBar);
    inviteInboxButton = new QToolButton(ui->headerPanel);
    inviteInboxButton->setObjectName(QStringLiteral("inviteInboxButton"));
    inviteInboxButton->setText(QStringLiteral("邀请"));
    inviteInboxButton->setIcon(inviteBellIcon(false));
    inviteInboxButton->setIconSize(QSize(22, 22));
    inviteInboxButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    inviteInboxButton->setToolTip(QStringLiteral("查看待处理的聊天室邀请"));
    inviteInboxButton->setCursor(Qt::PointingHandCursor);
    inviteInboxButton->setStyleSheet(QStringLiteral("QToolButton { color:#5f4a95; background:transparent;"
        " border:0; border-radius:9px; padding:7px 9px; font-size:13px; }"
        "QToolButton:hover { background:#eee8fb; }"));
    ui->headerLayout->insertWidget(ui->headerLayout->count() - 3, inviteInboxButton);
    connect(inviteInboxButton, &QToolButton::clicked, this, &MainWindow::openInviteInbox);
    inviteSendTimer = new QTimer(this);
    inviteSendTimer->setSingleShot(true);
    connect(inviteSendTimer, &QTimer::timeout, this, [this]() {
        if (inviteSendInFlight && roomInviteDialog)
            roomInviteDialog->showError(QStringLiteral("服务器尚未确认邀请结果。为避免重复发送，请重新连接后刷新结果。"));
    });
    inviteDecisionTimer = new QTimer(this);
    inviteDecisionTimer->setSingleShot(true);
    connect(inviteDecisionTimer, &QTimer::timeout, this, [this]() {
        if (inviteDecisionInFlight && roomInviteInboxDialog)
            roomInviteInboxDialog->showError(QStringLiteral("服务器尚未确认处理结果，请重新连接后刷新邀请列表。"));
    });
    updateRoomTitle();
    // QTextEdit advertises vertical expansion through its parent layout. Keep
    // the original one-line composer height and give spare space to messages.
    ui->messageScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->composer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->composer->setFixedHeight(76);
    ui->chatPanelLayout->setStretch(0, 0);
    ui->chatPanelLayout->setStretch(1, 1);
    ui->chatPanelLayout->setStretch(2, 0);
    ui->messageInput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->messageInput->setFixedHeight(50);
    ui->messageInput->setPlaceholderText(QStringLiteral("请输入消息..."));
    ui->memberIcon->setPixmap(iconFromSvg(QStringLiteral(":/login/members.svg"), 22).pixmap(22, 22));
    ui->settingsButton->setIcon(iconFromSvg(QStringLiteral(":/login/settings.svg")));
    ui->helpButton->setIcon(iconFromSvg(QStringLiteral(":/login/help.svg")));
    ui->exitButton->setIcon(iconFromSvg(QStringLiteral(":/login/exit.svg")));
    ui->sendButton->setIcon(iconFromSvg(QStringLiteral(":/login/send.svg"), 19));
    ui->sendButton->setIconSize(QSize(19, 19));
    ui->settingsButton->setMenu(ui->menuSettings);
    ui->menuSettings->addAction(ui->actionConnect);
    ui->menuSettings->addAction(ui->actionDisconnect);
    connect(ui->helpButton, &QPushButton::clicked, ui->actionAbout, &QAction::trigger);
    connect(ui->exitButton, &QPushButton::clicked, ui->actionExit, &QAction::trigger);

    QFont bodyFont(QStringLiteral("Microsoft YaHei UI"));
    bodyFont.setPixelSize(14);
    setFont(bodyFont);
    QPalette inputPalette = ui->messageInput->palette();
    inputPalette.setColor(QPalette::PlaceholderText, QColor("#837a9a"));
    ui->messageInput->setPalette(inputPalette);
    ui->messageInput->setFocus();
    ui->sendButton->setEnabled(false);
    connect(ui->messageInput, &QTextEdit::textChanged, this, [this]() {
        ui->sendButton->setEnabled(!ui->messageInput->toPlainText().trimmed().isEmpty());
        updateMentionPopup();
    });
    connect(ui->messageInput, &QTextEdit::cursorPositionChanged, this, &MainWindow::updateMentionPopup);
    connect(ui->messageInput, &MentionInput::compositionFinished, this, &MainWindow::updateMentionPopup);
    connect(ui->messageInput->document(), &QTextDocument::contentsChange,
            this, &MainWindow::updateSelectedMentionRanges);
    ui->messageInput->installEventFilter(this);
    ui->centralwidget->installEventFilter(this);
    setupMentionPopup();
    ui->userList->setSpacing(4);
    ui->userList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    ui->userList->setFixedHeight(0);
    auto *viviDivider = new QFrame(ui->rightPanel);
    viviDivider->setObjectName(QStringLiteral("viviDivider"));
    viviDivider->setFixedHeight(1);
    viviDivider->setStyleSheet(QStringLiteral("background:#eee8f8; border:none;"));
    ui->rightLayout->insertWidget(2, viviDivider);

    auto *viviCard = new QFrame(ui->rightPanel);
    viviCard->setObjectName(QStringLiteral("viviCard"));
    viviCard->setAttribute(Qt::WA_StyledBackground, true);
    viviCard->setAttribute(Qt::WA_Hover, true);
    viviCard->setStyleSheet(QStringLiteral(
        "QFrame#viviCard { background:#f5f0fe; border-radius:13px; } "
        "QFrame#viviCard:hover { background:#eee6fb; }"));
    viviCard->setToolTip(QStringLiteral("VChat AI 智能助手，功能开发中"));
    auto *viviLayout = new QHBoxLayout(viviCard);
    viviLayout->setContentsMargins(11, 11, 11, 11);
    viviLayout->setSpacing(12);
    auto *viviAvatar = new QLabel(viviCard);
    viviAvatar->setFixedSize(44, 44);
    viviAvatar->setAlignment(Qt::AlignCenter);
    viviAvatar->setPixmap(iconFromSvg(QStringLiteral(":/login/vivi-robot.svg"), 25).pixmap(25, 25));
    viviAvatar->setStyleSheet(QStringLiteral("background:#e8defc; border-radius:22px;"));
    viviLayout->addWidget(viviAvatar);
    auto *viviDetails = new QVBoxLayout;
    viviDetails->setContentsMargins(0, 0, 0, 0);
    viviDetails->setSpacing(5);
    auto *viviTitleRow = new QHBoxLayout;
    viviTitleRow->setContentsMargins(0, 0, 0, 0);
    viviTitleRow->setSpacing(7);
    auto *viviName = new QLabel(QStringLiteral("Vivi"), viviCard);
    viviName->setStyleSheet(QStringLiteral("color:#252044; font-size:14px; font-weight:600;"));
    viviTitleRow->addWidget(viviName);
    auto *viviTag = new RoundedLabel(QStringLiteral("AI 助手"), viviCard);
    viviTag->setFillColor(QColor("#e8defc"));
    viviTag->setMinimumHeight(20);
    viviTag->setAlignment(Qt::AlignCenter);
    viviTag->setStyleSheet(QStringLiteral("color:#7956d5; padding:2px 9px; font-size:11px;"));
    viviTitleRow->addWidget(viviTag);
    viviTitleRow->addStretch();
    viviDetails->addLayout(viviTitleRow);
    auto *viviStatusRow = new QHBoxLayout;
    viviStatusRow->setContentsMargins(0, 0, 0, 0);
    viviStatusRow->setSpacing(7);
    viviStatusDot = new QLabel(viviCard);
    viviStatusDot->setFixedSize(8, 8);
    viviStatusRow->addWidget(viviStatusDot);
    viviStatusLabel = new QLabel(viviCard);
    viviStatusRow->addWidget(viviStatusLabel);
    viviStatusRow->addStretch();
    viviDetails->addLayout(viviStatusRow);
    viviLayout->addLayout(viviDetails, 1);
    ui->rightLayout->insertWidget(3, viviCard);
    setAiStatus(QStringLiteral("unavailable"));
    ui->rightLayout->insertStretch(4, 1);
    ui->messageLayout->setSpacing(8);
    ui->onlineBadge->setMinimumHeight(32);
    ui->onlineBadge->setAlignment(Qt::AlignCenter);
    for (QFrame *card : {ui->chatPanel, ui->rightPanel}) {
        auto *shadow = new QGraphicsDropShadowEffect(card);
        shadow->setBlurRadius(22);
        shadow->setOffset(0, 4);
        shadow->setColor(QColor(97, 69, 157, 24));
        card->setGraphicsEffect(shadow);
    }
    ui->serverStatusLabel->setText(QStringLiteral("● 服务器未连接"));
    ui->serverStatusLabel->setStyleSheet("color:#9b6b7d; font-size:12px;");
    ui->onlineCountLabel->setText(QStringLiteral("在线人数：0"));
    ui->onlineBadge->setText(QStringLiteral("0 人在线"));
}
void MainWindow::connectSlots()
{
    // .ui 中的 on_xxx_signal 槽函数由 setupUi 自动连接，这里只连接网络 socket。
    // 连接成功后，客户端会立刻发送 JOIN|昵称，让服务器知道这个用户正式进入聊天界面。
    connect(socket, &QTcpSocket::connected, this, [this]() {
        isConnected = true;
        roomRestorePending = true;
        roomListRequested = roomListLoaded = switchInFlight = roomHistoryLoading = initialHistoryDone = false;
        pendingRoomId.clear();
        queuedRoomId.clear();
        currentRoomId = QStringLiteral("class-room");
        roomMembers.clear();
        onlineRoomUserIds.clear();
        onlineNames.clear();
        clearMessageView();
        roomSidebar->setRooms({});
        roomSidebar->setCurrentRoom(currentRoomId);
        roomSidebar->setNotice(QStringLiteral("正在加载聊天室…"));
        pendingRoomInvites = {};
        updateInviteBadge();
        updateRoomTitle();
        ui->serverStatusLabel->setText(QStringLiteral("● 服务器已连接"));
        ui->serverStatusLabel->setStyleSheet("color:#31b875; font-size:12px;");
        appendSystemMessage(QStringLiteral("欢迎来到 VChat 聊天室"));
        appendSystemMessage("已连接到聊天服务器");
        statusBar()->showMessage("已连接到聊天服务器");
        sendJoinMessage();
    });

    // 断开连接后，界面状态恢复为“未连接”，在线列表也只显示自己。
    connect(socket, &QTcpSocket::disconnected, this, [this]() {
        isConnected = false;
        createRoomInFlight = false;
        roomCreateTimer->stop();
        inviteSendTimer->stop();
        inviteDecisionTimer->stop();
        inviteSendInFlight = inviteDecisionInFlight = false;
        inviteSearchRequestId.clear();
        inviteSendRequestId.clear();
        inviteDecisionRequestId.clear();
        managementTimer->stop();
        managementRequestId.clear();
        managementAction.clear();
        managementRoomId.clear();
        permissionsTimer->stop();
        permissionsReadId.clear();
        permissionsSaveId.clear();
        if (roomPermissionsDialog) {
            roomPermissionsDialog->setPending(false);
            roomPermissionsDialog->showError(QStringLiteral("连接已断开，权限保存结果尚未确认。"));
        }
        if (roomManagementDialog) {
            roomManagementDialog->setPending(false);
            roomManagementDialog->showError(QStringLiteral("连接已断开，请重新连接后核对操作结果。"));
        }
        pendingRoomInvites = {};
        updateInviteBadge();
        if (roomInviteDialog) {
            roomInviteDialog->setPending(false);
            roomInviteDialog->showError(QStringLiteral("服务器连接已断开，邀请结果请重新连接后核对。"));
        }
        if (roomInviteInboxDialog) {
            roomInviteInboxDialog->setPending(false);
            roomInviteInboxDialog->setInvites({});
            roomInviteInboxDialog->showError(QStringLiteral("服务器连接已断开，请重新连接后刷新邀请。"));
        }
        if (roomCreateDialog) {
            roomCreateDialog->setPending(false);
            roomCreateDialog->showError(QStringLiteral("服务器连接已断开，请重新连接后再创建。"));
        }
        roomListLoaded = switchInFlight = roomHistoryLoading = false;
        pendingRoomId.clear();
        queuedRoomId.clear();
        updateRoomTitle();
        roomSidebar->setNotice(QStringLiteral("连接已断开"));
        onlineRoomUserIds.clear();
        mentionProtocol = false;
        onlineMentionMembers.clear();
        streamedRequestIds.clear();
        aiReplyTargets.clear();
        for (auto it = aiStreams.begin(); it != aiStreams.end(); ++it) {
            if (it->retryAwaiting) {
                it->retryAwaiting = false;
                for (QToolButton *button : it->actions) button->setEnabled(true);
                if (it->actions.size() >= 2) it->actions.at(1)->setEnabled(it->canRegenerate);
            }
            if (it->finished) continue;
            it->interrupted = true;
            it->errorText = QStringLiteral("连接已断开，Vivi 生成中断。");
            it->errorReceived = true;
            it->raw = it->displayed;
            finalizeAiStream(it.key());
        }
        if (mentionPopup) mentionPopup->hide();
        setAiStatus(QStringLiteral("unavailable"));
        for (const QString &id : aiThinkingRows.keys()) removeAiThinking(id);
        ui->serverStatusLabel->setText(QStringLiteral("● 服务器未连接"));
        ui->serverStatusLabel->setStyleSheet("color:#9b6b7d; font-size:12px;");
        updateUserList();
        appendSystemMessage("已断开与聊天服务器的连接");
        statusBar()->showMessage("已断开连接");
    });

    // readyRead 表示服务器有新数据发过来了，读取工作交给 on_socketReadyRead()。
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::on_socketReadyRead);

    // 连接失败、服务器关闭等网络错误都会进入这里。
    connect(socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        isConnected = false;
        setAiStatus(QStringLiteral("unavailable"));
        ui->serverStatusLabel->setText(QStringLiteral("● 服务器未连接"));
        ui->serverStatusLabel->setStyleSheet("color:#9b6b7d; font-size:12px;");
        appendSystemMessage("连接服务器失败: " + socket->errorString());
        statusBar()->showMessage("连接服务器失败");
    });
}

void MainWindow::on_sendButton_clicked()
{
    const QString raw = ui->messageInput->toPlainText();
    const QString message = raw.trimmed();
    if (message.isEmpty()) {
        return;
    }

    if (!isConnected || socket->state() != QAbstractSocket::ConnectedState) {
        appendSystemMessage("请先连接服务器");
        return;
    }
    if (switchInFlight) {
        roomSidebar->setNotice(QStringLiteral("聊天室正在加载，请稍候"));
        return;
    }

    // 本项目使用最简单的文本协议：每条消息以换行符 \n 结束。
    // 服务器收到 “昵称: 内容” 后，会转发给其他在线客户端，并保存到数据库。
    QByteArray networkMessage;
    QString outgoingMessageId;
    QJsonArray mentions;
    if (mentionProtocol) {
        int leading = 0;
        while (leading < raw.size() && raw.at(leading).isSpace()) ++leading;
        for (const SelectedMention &selected : selectedMentions) {
            const int start = selected.start - leading;
            const QString expected = QStringLiteral("@") + selected.candidate.name;
            if (start < 0 || start + selected.length > message.size() ||
                message.mid(start, selected.length) != expected) continue;
            mentions.append(QJsonObject{{QStringLiteral("type"), selected.candidate.ai ? QStringLiteral("ai") : QStringLiteral("user")},
                {QStringLiteral("id"), selected.candidate.ai ? QStringLiteral("vivi") : QString::number(selected.candidate.id)},
                {QStringLiteral("name"), selected.candidate.name},
                {QStringLiteral("start"), start}, {QStringLiteral("length"), selected.length}});
        }
        outgoingMessageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QJsonObject payload{{QStringLiteral("text"), message},
            {QStringLiteral("messageId"), outgoingMessageId},
            {QStringLiteral("mentions"), mentions}};
        networkMessage = "CHAT2|" + QJsonDocument(payload).toJson(QJsonDocument::Compact).toBase64() + '\n';
    } else {
        networkMessage = (nickname + ": " + message + "\n").toUtf8();
    }
    if (socket->write(networkMessage) == -1) {
        appendSystemMessage("消息发送失败: " + socket->errorString());
        return;
    }

    appendMessage(nickname, message, true, {}, false, {}, mentions, {}, {}, outgoingMessageId);
    selectedMentions.clear();
    if (mentionPopup) mentionPopup->hide();
    ui->messageInput->clear();
    ui->messageInput->setFocus();
}

void MainWindow::on_messageInput_returnPressed()
{
    on_sendButton_clicked();
}

void MainWindow::on_socketReadyRead()
{
    // TCP 是流式传输，不能保证一次 readyRead 正好是一条完整消息。
    // 所以先追加到 receiveBuffer，再按 \n 一行一行拆出来处理。
    receiveBuffer.append(socket->readAll());

    int lineEnd = -1;
    while ((lineEnd = receiveBuffer.indexOf('\n')) != -1) {
        const QByteArray lineBytes = receiveBuffer.left(lineEnd).trimmed();
        receiveBuffer.remove(0, lineEnd + 1);

        if (!lineBytes.isEmpty()) {
            appendIncomingLine(QString::fromUtf8(lineBytes));
        }
    }
}

void MainWindow::on_actionConnect_triggered()
{
    // 菜单栏“连接服务器”会弹出输入框，手动填写服务器 IP 和端口。
    bool ok = false;
    const QString serverAddress = QInputDialog::getText(
        this,
        "连接服务器",
        "请输入服务器地址:",
        QLineEdit::Normal,
        "127.0.0.1:8080",
        &ok);

    if (ok && !serverAddress.trimmed().isEmpty()) {
        connectToServerAddress(serverAddress, true);
    }
}

void MainWindow::on_actionDisconnect_triggered()
{
    if (socket->state() == QAbstractSocket::UnconnectedState) {
        appendSystemMessage("当前没有连接服务器");
        return;
    }

    appendSystemMessage("正在断开服务器连接...");
    socket->disconnectFromHost();
}

void MainWindow::on_actionExit_triggered()
{
    close();
}

void MainWindow::on_actionChangeNickname_triggered()
{
    bool ok = false;
    const QString newNickname = QInputDialog::getText(
        this,
        "修改昵称",
        "请输入新昵称:",
        QLineEdit::Normal,
        nickname,
        &ok);

    if (!ok || newNickname.trimmed().isEmpty()) {
        return;
    }

    const QString oldNickname = nickname;
    nickname = newNickname.trimmed();
    setWindowTitle("VChat - " + nickname);
    appendSystemMessage(oldNickname + " 修改昵称为 " + nickname);

    // 如果已经连接服务器，改名后重新发送 JOIN，让服务器刷新在线列表里的名字。
    if (socket->state() == QAbstractSocket::ConnectedState) {
        sendJoinMessage();
    }
}

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(
        this,
        "关于 VChat",
        "VChat 学生群体聊天系统\n\n"
        "基于 Qt6 开发，支持登录验证、局域网聊天、服务器转发、在线用户列表和聊天记录保存。"
    );
}

bool MainWindow::connectToServerAddress(const QString &address, bool showErrorMessage)
{
    const QString serverAddress = address.trimmed();

    // 服务器地址约定为 “IP:端口”，例如 192.168.1.10:8080。
    // lastIndexOf(':') 可以兼容 IP 中没有冒号的普通 IPv4 地址。
    const int separatorIndex = serverAddress.lastIndexOf(':');
    if (separatorIndex <= 0 || separatorIndex == serverAddress.size() - 1) {
        if (showErrorMessage) {
            QMessageBox::warning(this, "连接失败", "服务器地址格式应为 IP:端口，例如 127.0.0.1:8080");
        } else {
            appendSystemMessage("服务器地址格式不正确: " + serverAddress);
        }
        return false;
    }

    const QString host = serverAddress.left(separatorIndex).trimmed();
    bool portOk = false;
    const quint16 port = serverAddress.mid(separatorIndex + 1).trimmed().toUShort(&portOk);
    if (!portOk || port == 0) {
        if (showErrorMessage) {
            QMessageBox::warning(this, "连接失败", "端口号不正确，例如 8080");
        } else {
            appendSystemMessage("服务器端口不正确: " + serverAddress);
        }
        return false;
    }

    // 如果之前连过其他服务器，先 abort 关闭旧连接，再连接新地址。
    if (socket->state() != QAbstractSocket::UnconnectedState) {
        socket->abort();
    }

    statusBar()->showMessage("正在连接服务器...");
    socket->connectToHost(host, port);
    return true;
}

void MainWindow::sendJoinMessage()
{
    if (socket->state() == QAbstractSocket::ConnectedState) {
        if (sessionToken.isEmpty()) {
            roomSidebar->setNotice(QStringLiteral("登录会话缺失，请重新登录"));
            appendSystemMessage(QStringLiteral("登录会话缺失，请重新登录后进入聊天室。"));
            return;
        }
        socket->write(QStringLiteral("JOIN|%1|%2\n").arg(nickname, sessionToken).toUtf8());
    }
}

void MainWindow::sendRoomRequest(const QString &action, const QJsonObject &arguments)
{
    if (socket->state() != QAbstractSocket::ConnectedState || sessionToken.isEmpty()) return;
    socket->write("ROOM|" + action.toUtf8() + '|' +
        QJsonDocument(arguments).toJson(QJsonDocument::Compact).toBase64() + '\n');
}

void MainWindow::requestRoomSwitch(const QString &roomId)
{
    if (!roomListLoaded || roomId.isEmpty()) return;
    bool allowed = false;
    for (const QJsonValue &value : joinedRooms)
        if (value.toObject().value(QStringLiteral("room_id")).toString() == roomId) allowed = true;
    if (!allowed) return;
    if (switchInFlight) {
        queuedRoomId = roomId;
        return;
    }
    if (roomId == currentRoomId) return;
    switchInFlight = true;
    pendingRoomId = roomId;
    queuedRoomId.clear();
    updateRoomTitle();
    ui->messageInput->setEnabled(false);
    ui->sendButton->setEnabled(false);
    roomSidebar->setNotice(QStringLiteral("正在切换聊天室…"));
    sendRoomRequest(QStringLiteral("switch"), {{QStringLiteral("room_id"), roomId}});
}

void MainWindow::openCreateRoomDialog()
{
    if (roomCreateDialog) { roomCreateDialog->raise(); roomCreateDialog->activateWindow(); return; }
    if (!roomListLoaded || !isConnected || switchInFlight) {
        roomSidebar->setNotice(QStringLiteral("请等待聊天室加载完成并确认服务器已连接"));
        return;
    }
    auto *dialog = new RoomCreateDialog(this);
    roomCreateDialog = dialog;
    connect(dialog, &RoomCreateDialog::createSubmitted, this,
        [this, dialog](const QString &name, const QString &description, const QString &type) {
            if (createRoomInFlight) {
                dialog->setPending(false);
                dialog->showError(QStringLiteral("上一次创建请求结果尚未确认，不能重复提交。"));
                return;
            }
            if (socket->state() != QAbstractSocket::ConnectedState) {
                dialog->setPending(false);
                dialog->showError(QStringLiteral("服务器未连接，请稍后重试。"));
                return;
            }
            createRoomInFlight = true;
            sendRoomRequest(QStringLiteral("create"),
                {{QStringLiteral("room_name"), name},
                 {QStringLiteral("description"), description},
                 {QStringLiteral("room_type"), type}});
            roomCreateTimer->start(15000);
        });
    dialog->open();
}

void MainWindow::openInviteDialog()
{
    if (roomInviteDialog) { roomInviteDialog->raise(); roomInviteDialog->activateWindow(); return; }
    if (!canInviteToCurrentRoom() || switchInFlight || roomHistoryLoading) return;
    QJsonObject selectedRoom;
    for (const QJsonValue &value : joinedRooms) {
        if (value.toObject().value(QStringLiteral("room_id")).toString() == currentRoomId) {
            selectedRoom = value.toObject(); break;
        }
    }
    auto *dialog = new RoomInviteDialog(currentRoomId,
        selectedRoom.value(QStringLiteral("room_name")).toString(), this);
    roomInviteDialog = dialog;
    connect(dialog, &RoomInviteDialog::searchRequested, this,
        [this](const QString &roomId, const QString &query) {
            if (!isConnected) return;
            inviteSearchRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            sendRoomRequest(QStringLiteral("search_users"), {{QStringLiteral("request_id"), inviteSearchRequestId},
                {QStringLiteral("room_id"), roomId}, {QStringLiteral("query"), query}});
        });
    connect(dialog, &RoomInviteDialog::sendRequested, this,
        [this, dialog](const QString &roomId, const QJsonArray &ids) {
            if (inviteSendInFlight || !isConnected) {
                dialog->setPending(inviteSendInFlight);
                dialog->showError(QStringLiteral("上一次请求尚未确认，或服务器已断开。"));
                return;
            }
            inviteSendInFlight = true;
            inviteSendRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            sendRoomRequest(QStringLiteral("send_user_invites"),
                {{QStringLiteral("request_id"), inviteSendRequestId},
                 {QStringLiteral("room_id"), roomId}, {QStringLiteral("user_ids"), ids}});
            inviteSendTimer->start(15000);
        });
    dialog->open();
    dialog->requestSearch();
}

void MainWindow::openInviteInbox()
{
    if (roomInviteInboxDialog) { roomInviteInboxDialog->raise(); roomInviteInboxDialog->activateWindow(); return; }
    auto *dialog = new RoomInviteInboxDialog(this);
    roomInviteInboxDialog = dialog;
    dialog->setInvites(pendingRoomInvites);
    dialog->setPending(inviteDecisionInFlight);
    connect(dialog, &RoomInviteInboxDialog::decisionRequested, this,
        [this, dialog](const QString &inviteId, bool accept) {
            if (inviteDecisionInFlight || !isConnected) {
                dialog->showError(QStringLiteral("上一次处理尚未确认，或服务器已断开。"));
                return;
            }
            inviteDecisionInFlight = true;
            dialog->setPending(true);
            dialog->showError({});
            inviteDecisionRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            sendRoomRequest(QStringLiteral("respond_invite"),
                {{QStringLiteral("request_id"), inviteDecisionRequestId},
                 {QStringLiteral("invite_id"), inviteId},
                 {QStringLiteral("decision"), accept ? QStringLiteral("accept") : QStringLiteral("decline")}});
            inviteDecisionTimer->start(15000);
        });
    dialog->open();
    refreshPendingInvites();
}

void MainWindow::openRoomManagement(const QString &page)
{
    if (!isConnected || switchInFlight || roomHistoryLoading ||
        currentRoomId == QStringLiteral("class-room")) return;
    if (roomManagementDialog && roomManagementDialog->roomId() != currentRoomId)
        roomManagementDialog->close();
    if (!roomManagementDialog) {
        auto *dialog = new RoomManagementDialog(currentRoomId, ownAccountId, this);
        roomManagementDialog = dialog;
        connect(dialog, &RoomManagementDialog::saveRequested, this,
            [this](const QString &roomId, const QString &name, const QString &description) {
                submitRoomManagement(QStringLiteral("update_details"), roomId,
                    {{QStringLiteral("room_name"), name}, {QStringLiteral("description"), description}});
            });
        connect(dialog, &RoomManagementDialog::removeRequested, this,
            [this](const QString &roomId, const QString &userId) {
                submitRoomManagement(QStringLiteral("remove_member"), roomId,
                    {{QStringLiteral("user_id"), userId}});
            });
        connect(dialog, &RoomManagementDialog::leaveRequested, this,
            [this](const QString &roomId) { submitRoomManagement(QStringLiteral("leave"), roomId); });
        connect(dialog, &RoomManagementDialog::dissolveRequested, this,
            [this](const QString &roomId) { submitRoomManagement(QStringLiteral("dissolve"), roomId); });
        connect(dialog, &RoomManagementDialog::permissionsRequested, this,
            &MainWindow::openRoomPermissions);
        dialog->open();
    } else {
        roomManagementDialog->raise();
        roomManagementDialog->activateWindow();
    }
    roomManagementDialog->showPage(page);
    sendRoomRequest(QStringLiteral("details"), {{QStringLiteral("room_id"), currentRoomId},
        {QStringLiteral("request_id"), QUuid::createUuid().toString(QUuid::WithoutBraces)}});
    requestRoomMembers();
}

void MainWindow::openRoomPermissions(const QString &roomId)
{
    if (!isConnected || roomId.isEmpty() || roomId == QStringLiteral("class-room")) return;
    if (roomPermissionsDialog && roomPermissionsDialog->roomId() != roomId)
        roomPermissionsDialog->close();
    if (roomPermissionsDialog) {
        roomPermissionsDialog->raise();
        return;
    }
    bool owner = false;
    for (const QJsonValue &value : joinedRooms) {
        const QJsonObject room = value.toObject();
        if (room.value(QStringLiteral("room_id")).toString() == roomId) {
            owner = room.value(QStringLiteral("owner_id")).toString().toLongLong() == ownAccountId;
            break;
        }
    }
    QWidget *dialogParent = roomManagementDialog && roomManagementDialog->roomId() == roomId
        ? static_cast<QWidget *>(roomManagementDialog.data()) : static_cast<QWidget *>(this);
    auto *dialog = new RoomPermissionsDialog(roomId, owner, dialogParent);
    roomPermissionsDialog = dialog;
    connect(dialog, &RoomPermissionsDialog::saveRequested, this,
        [this](const QString &id, const QJsonObject &settings) {
            if (!isConnected || !permissionsSaveId.isEmpty() || !roomPermissionsDialog ||
                roomPermissionsDialog->roomId() != id) {
                if (roomPermissionsDialog) {
                    roomPermissionsDialog->setPending(false);
                    roomPermissionsDialog->showError(QStringLiteral("当前无法保存权限配置，请检查连接。"));
                }
                return;
            }
            permissionsSaveId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            sendRoomRequest(QStringLiteral("update_permissions"),
                {{QStringLiteral("room_id"), id}, {QStringLiteral("permissions"), settings},
                 {QStringLiteral("request_id"), permissionsSaveId}});
            permissionsTimer->start(15000);
        });
    dialog->open();
    permissionsReadId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString readId = permissionsReadId;
    sendRoomRequest(QStringLiteral("permissions"), {{QStringLiteral("room_id"), roomId},
        {QStringLiteral("request_id"), permissionsReadId}});
    QTimer::singleShot(15000, this, [this, readId, roomId]() {
        if (permissionsReadId != readId) return;
        permissionsReadId.clear();
        if (roomPermissionsDialog && roomPermissionsDialog->roomId() == roomId)
            roomPermissionsDialog->showError(QStringLiteral("读取权限配置超时，请关闭后重试。"));
    });
}

void MainWindow::submitRoomManagement(const QString &action, const QString &roomId,
                                      const QJsonObject &arguments)
{
    if (!managementRequestId.isEmpty() || !isConnected || roomId.isEmpty()) {
        if (roomManagementDialog) {
            roomManagementDialog->setPending(!managementRequestId.isEmpty());
            roomManagementDialog->showError(QStringLiteral("服务器未连接，或上一次操作尚未确认。"));
        }
        return;
    }
    managementRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    managementAction = action;
    managementRoomId = roomId;
    QJsonObject request = arguments;
    request.insert(QStringLiteral("room_id"), roomId);
    request.insert(QStringLiteral("request_id"), managementRequestId);
    if (roomManagementDialog) roomManagementDialog->setPending(true);
    sendRoomRequest(action, request);
    managementTimer->start(15000);
}

void MainWindow::applyRoomUpdate(const QJsonObject &room)
{
    const QString roomId = room.value(QStringLiteral("room_id")).toString();
    if (roomId.isEmpty()) return;
    for (int i = 0; i < joinedRooms.size(); ++i) {
        if (joinedRooms.at(i).toObject().value(QStringLiteral("room_id")).toString() != roomId) continue;
        QJsonObject updated = joinedRooms.at(i).toObject();
        for (auto it = room.begin(); it != room.end(); ++it) updated.insert(it.key(), it.value());
        joinedRooms.replace(i, updated);
        roomSidebar->setRooms(joinedRooms);
        roomSidebar->setCurrentRoom(currentRoomId);
        if (roomManagementDialog && roomManagementDialog->roomId() == roomId)
            roomManagementDialog->setRoom(updated);
        if (roomId == currentRoomId) updateRoomTitle();
        return;
    }
}

void MainWindow::revokeRoomAccess(const QString &roomId)
{
    if (roomId.isEmpty()) return;
    if (currentRoomId == roomId && switchInFlight &&
        pendingRoomId == QStringLiteral("class-room")) return;
    for (int i = joinedRooms.size() - 1; i >= 0; --i)
        if (joinedRooms.at(i).toObject().value(QStringLiteral("room_id")).toString() == roomId)
            joinedRooms.removeAt(i);
    roomSidebar->setRooms(joinedRooms);
    if (roomManagementDialog && roomManagementDialog->roomId() == roomId)
        roomManagementDialog->close();
    if (currentRoomId == roomId || pendingRoomId == roomId) {
        ui->messageInput->setEnabled(false);
        ui->sendButton->setEnabled(false);
        requestRoomSwitch(QStringLiteral("class-room"));
    } else roomSidebar->setCurrentRoom(currentRoomId);
    sendRoomRequest(QStringLiteral("list_mine"));
}

void MainWindow::refreshPendingInvites()
{
    if (isConnected && initialHistoryDone) sendRoomRequest(QStringLiteral("pending_invites"));
}

void MainWindow::updateInviteBadge()
{
    inviteInboxButton->setText(QStringLiteral("邀请"));
    inviteInboxButton->setIcon(inviteBellIcon(!pendingRoomInvites.isEmpty()));
    inviteInboxButton->setProperty("pendingCount", pendingRoomInvites.size());
    inviteInboxButton->setProperty("hasPendingInvite", !pendingRoomInvites.isEmpty());
}

bool MainWindow::canInviteToCurrentRoom() const
{
    if (!isConnected || !roomListLoaded || ownAccountId <= 0 ||
        currentRoomId == QStringLiteral("class-room")) return false;
    for (const QJsonValue &value : joinedRooms) {
        const QJsonObject room = value.toObject();
        if (room.value(QStringLiteral("room_id")).toString() != currentRoomId) continue;
        bool valid = false;
        const QJsonValue rawOwner = room.value(QStringLiteral("owner_id"));
        const qint64 ownerId = rawOwner.isString()
            ? rawOwner.toString().toLongLong(&valid)
            : rawOwner.toVariant().toLongLong(&valid);
        if (valid && ownerId > 0)
            return ownerId == ownAccountId || room.value(QStringLiteral("allow_member_invite")).toBool();
        break; // Older ROOM servers may omit owner_id; use their members role.
    }
    for (const RoomMember &member : roomMembers)
        if (member.id == ownAccountId && member.role == QStringLiteral("owner")) return true;
    return false;
}

void MainWindow::handleRoomNotice(const QJsonObject &notice)
{
    const QString event = notice.value(QStringLiteral("event")).toString();
    if (event == QStringLiteral("invite_received")) {
        QApplication::alert(this);
        refreshPendingInvites();
    } else if (event == QStringLiteral("invites_updated")) {
        refreshPendingInvites();
    } else if (event == QStringLiteral("members_changed") &&
               notice.value(QStringLiteral("room_id")).toString() == currentRoomId) {
        requestRoomMembers();
    } else if (event == QStringLiteral("room_updated")) {
        applyRoomUpdate(notice.value(QStringLiteral("room")).toObject());
    } else if (event == QStringLiteral("permissions_updated")) {
        const QString changedRoom = notice.value(QStringLiteral("room_id")).toString();
        applyRoomUpdate(notice.value(QStringLiteral("room")).toObject());
        if (roomPermissionsDialog && roomPermissionsDialog->roomId() == changedRoom &&
            permissionsSaveId.isEmpty())
            roomPermissionsDialog->setPermissions(notice.value(QStringLiteral("permissions")).toObject());
        sendRoomRequest(QStringLiteral("list_mine"));
    } else if (event == QStringLiteral("access_revoked") ||
               event == QStringLiteral("room_dissolved")) {
        revokeRoomAccess(notice.value(QStringLiteral("room_id")).toString());
    }
}

void MainWindow::setMembersCollapsed(bool collapsed)
{
    membersCollapsed = collapsed;
    QScrollBar *scroll = ui->messageScroll->verticalScrollBar();
    const int oldPosition = scroll->value();
    const bool wasAtBottom = oldPosition >= scroll->maximum() - 40;
    ui->rightPanel->setVisible(!collapsed);
    memberExpandButton->setVisible(collapsed);
    ui->bodyLayout->setStretch(1, 3);
    ui->bodyLayout->setStretch(2, 0);
    ui->bodyLayout->setStretch(3, collapsed ? 0 : 1);
    QSettings settings(QStringLiteral("VChat"), QStringLiteral("VChatClient"));
    settings.setValue(QStringLiteral("members/collapsed/%1").arg(ownAccountId), collapsed);
    if (!collapsed) requestRoomMembers();
    QTimer::singleShot(0, this, [this, oldPosition, wasAtBottom]() {
        updateBubbleWidths();
        if (!membersCollapsed) updateMemberListHeight();
        QScrollBar *bar = ui->messageScroll->verticalScrollBar();
        bar->setValue(wasAtBottom ? bar->maximum() : qMin(oldPosition, bar->maximum()));
    });
}

void MainWindow::clearMessageView()
{
    aiTypewriterTimer->stop();
    aiStreams.clear();
    aiThinkingRows.clear();
    aiReplyTargets.clear();
    streamedRequestIds.clear();
    renderedUserMessageIds.clear();
    bubbleColumns.clear();
    messageBubbles.clear();
    messageBodies.clear();
    aiColumns.clear();
    aiBubbles.clear();
    aiBodies.clear();
    systemPills.clear();
    lastMessageKey.clear();
    lastSystemDate = QDate();
    // Keep the trailing stretch item; all message widgets belong to this layout.
    while (ui->messageLayout->count() > 1) {
        QLayoutItem *item = ui->messageLayout->takeAt(0);
        delete item->widget();
        delete item;
    }
}

void MainWindow::updateRoomTitle()
{
    QString name = QStringLiteral("VChat 大厅");
    QJsonObject active;
    for (const QJsonValue &value : joinedRooms) {
        const QJsonObject room = value.toObject();
        if (room.value(QStringLiteral("room_id")).toString() == currentRoomId) {
            name = room.value(QStringLiteral("room_name")).toString(name);
            active = room;
            break;
        }
    }
    if (roomNameLabel) {
        roomNameLabel->setText(name);
        roomDescriptionLabel->setText(active.value(QStringLiteral("description")).toString());
        roomDescriptionLabel->setVisible(!roomDescriptionLabel->text().isEmpty());
        const bool canInvite = canInviteToCurrentRoom();
        inviteMembersButton->setVisible(canInvite);
        inviteMembersButton->setEnabled(canInvite && !switchInFlight && !roomHistoryLoading);
        inviteMembersButton->setToolTip(QStringLiteral("邀请成员加入当前聊天室"));
        roomMoreButton->setVisible(currentRoomId != QStringLiteral("class-room") && roomListLoaded);
        roomMoreButton->setEnabled(isConnected && !switchInFlight && !roomHistoryLoading);
    }
    ui->subtitleLabel->setText(QStringLiteral("轻松连接每一次交流"));
    setWindowTitle(QStringLiteral("VChat - %1").arg(name));
}

void MainWindow::requestRoomMembers()
{
    if (roomListLoaded && !currentRoomId.isEmpty())
        sendRoomRequest(QStringLiteral("members"), {{QStringLiteral("room_id"), currentRoomId}});
}

void MainWindow::handleRoomResult(const QJsonObject &result)
{
    const QString action = result.value(QStringLiteral("action")).toString();
    const bool ok = result.value(QStringLiteral("ok")).toBool();
    const QString requestId = result.value(QStringLiteral("request_id")).toString();
    if (action == QStringLiteral("permissions")) {
        if (requestId == permissionsReadId && roomPermissionsDialog &&
            roomPermissionsDialog->roomId() == result.value(QStringLiteral("room_id")).toString()) {
            permissionsReadId.clear();
            if (ok) roomPermissionsDialog->setPermissions(result.value(QStringLiteral("permissions")).toObject());
            else roomPermissionsDialog->showError(result.value(QStringLiteral("error"))
                .toString(QStringLiteral("权限配置读取失败")));
        }
        return;
    }
    if (action == QStringLiteral("update_permissions")) {
        if (requestId != permissionsSaveId) return;
        permissionsTimer->stop();
        permissionsSaveId.clear();
        if (roomPermissionsDialog &&
            roomPermissionsDialog->roomId() == result.value(QStringLiteral("room_id")).toString()) {
            roomPermissionsDialog->setPending(false);
            if (ok) roomPermissionsDialog->accept();
            else roomPermissionsDialog->showError(result.value(QStringLiteral("error"))
                .toString(QStringLiteral("保存权限配置失败")));
        }
        if (ok) {
            sendRoomRequest(QStringLiteral("list_mine"));
            if (roomManagementDialog)
                sendRoomRequest(QStringLiteral("details"),
                    {{QStringLiteral("room_id"), result.value(QStringLiteral("room_id"))}});
        }
        return;
    }
    if (action == QStringLiteral("details")) {
        const QJsonObject room = result.value(QStringLiteral("room")).toObject();
        if (roomManagementDialog && roomManagementDialog->roomId() ==
            (ok ? room.value(QStringLiteral("room_id")).toString()
                : result.value(QStringLiteral("room_id")).toString())) {
            if (ok) roomManagementDialog->setRoom(room);
            else roomManagementDialog->showError(result.value(QStringLiteral("error"))
                .toString(QStringLiteral("聊天室详情加载失败")));
        }
        return;
    }
    if (action == QStringLiteral("update_details") || action == QStringLiteral("remove_member") ||
        action == QStringLiteral("leave") || action == QStringLiteral("dissolve")) {
        if (requestId != managementRequestId || action != managementAction) return;
        const QString operatedRoom = managementRoomId;
        managementTimer->stop();
        managementRequestId.clear();
        managementAction.clear();
        managementRoomId.clear();
        if (roomManagementDialog && roomManagementDialog->roomId() == operatedRoom)
            roomManagementDialog->setPending(false);
        if (!ok) {
            if (roomManagementDialog && roomManagementDialog->roomId() == operatedRoom)
                roomManagementDialog->showError(result.value(QStringLiteral("error"))
                    .toString(QStringLiteral("聊天室操作失败")));
            else roomSidebar->setNotice(result.value(QStringLiteral("error"))
                    .toString(QStringLiteral("聊天室操作失败")));
            return;
        }
        if (action == QStringLiteral("update_details")) {
            applyRoomUpdate(result.value(QStringLiteral("room")).toObject());
            if (roomManagementDialog && roomManagementDialog->roomId() == operatedRoom)
                roomManagementDialog->showPage(QStringLiteral("details"));
        } else if (action == QStringLiteral("remove_member")) {
            requestRoomMembers();
            sendRoomRequest(QStringLiteral("details"), {{QStringLiteral("room_id"), operatedRoom}});
        } else revokeRoomAccess(operatedRoom);
        return;
    }
    if (action == QStringLiteral("search_users")) {
        if (!roomInviteDialog || requestId != inviteSearchRequestId ||
            result.value(QStringLiteral("room_id")).toString() != roomInviteDialog->roomId()) return;
        if (ok) roomInviteDialog->setResults(result.value(QStringLiteral("users")).toArray());
        else roomInviteDialog->showError(result.value(QStringLiteral("error"))
            .toString(QStringLiteral("用户搜索失败")));
        return;
    }
    if (action == QStringLiteral("pending_invites")) {
        if (ok) {
            pendingRoomInvites = result.value(QStringLiteral("invites")).toArray();
            updateInviteBadge();
            if (roomInviteInboxDialog) roomInviteInboxDialog->setInvites(pendingRoomInvites);
        } else if (roomInviteInboxDialog) {
            roomInviteInboxDialog->showError(result.value(QStringLiteral("error"))
                .toString(QStringLiteral("邀请列表加载失败")));
        }
        return;
    }
    if (action == QStringLiteral("send_user_invites")) {
        if (!inviteSendInFlight || requestId != inviteSendRequestId) return;
        inviteSendInFlight = false;
        inviteSendTimer->stop();
        inviteSendRequestId.clear();
        if (roomInviteDialog) {
            roomInviteDialog->setPending(false);
            if (ok) roomInviteDialog->showSuccess(result.value(QStringLiteral("sent")).toArray().size());
            else roomInviteDialog->showError(result.value(QStringLiteral("error"))
                .toString(QStringLiteral("发送邀请失败")));
        } else {
            roomSidebar->setNotice(ok
                ? QStringLiteral("邀请已发送给 %1 位用户").arg(result.value(QStringLiteral("sent")).toArray().size())
                : result.value(QStringLiteral("error")).toString(QStringLiteral("发送邀请失败")));
        }
        return;
    }
    if (action == QStringLiteral("respond_invite")) {
        if (!inviteDecisionInFlight || requestId != inviteDecisionRequestId) return;
        inviteDecisionInFlight = false;
        inviteDecisionTimer->stop();
        inviteDecisionRequestId.clear();
        if (roomInviteInboxDialog) {
            roomInviteInboxDialog->setPending(false);
            if (ok) roomInviteInboxDialog->showError({});
            else roomInviteInboxDialog->showError(result.value(QStringLiteral("error"))
                .toString(QStringLiteral("处理邀请失败")));
        }
        refreshPendingInvites();
        if (ok && result.value(QStringLiteral("decision")).toString() == QStringLiteral("accept"))
            sendRoomRequest(QStringLiteral("list_mine"));
        return;
    }
    if (action == QStringLiteral("list_mine")) {
        if (!ok || !result.value(QStringLiteral("rooms")).isArray()) {
            roomSidebar->setNotice(result.value(QStringLiteral("error"))
                .toString(QStringLiteral("聊天室列表加载失败")));
            return;
        }
        joinedRooms = result.value(QStringLiteral("rooms")).toArray();
        roomListLoaded = true;
        roomSidebar->setRooms(joinedRooms);
        roomSidebar->setNotice(joinedRooms.isEmpty() ? QStringLiteral("尚未加入聊天室") : QString());
        roomSidebar->setCurrentRoom(currentRoomId);
        updateRoomTitle();
        requestRoomMembers();
        QSettings settings(QStringLiteral("VChat"), QStringLiteral("VChatClient"));
        const QString saved = settings.value(roomSettingsKey).toString();
        bool accessible = false;
        for (const QJsonValue &value : joinedRooms)
            if (value.toObject().value(QStringLiteral("room_id")).toString() == saved) accessible = true;
        if (roomRestorePending) {
            roomRestorePending = false;
            if (accessible && saved != currentRoomId) requestRoomSwitch(saved);
        }
        return;
    }
    if (action == QStringLiteral("create")) {
        if (!createRoomInFlight) return;
        createRoomInFlight = false;
        roomCreateTimer->stop();
        if (!ok || !result.value(QStringLiteral("room")).isObject()) {
            if (roomCreateDialog) {
                roomCreateDialog->setPending(false);
                roomCreateDialog->showError(result.value(QStringLiteral("error"))
                    .toString(QStringLiteral("创建聊天室失败，请稍后重试。")));
            }
            return;
        }
        const QJsonObject room = result.value(QStringLiteral("room")).toObject();
        const QString roomId = room.value(QStringLiteral("room_id")).toString();
        if (roomId.isEmpty()) {
            if (roomCreateDialog) {
                roomCreateDialog->setPending(false);
                roomCreateDialog->showError(QStringLiteral("服务器未返回聊天室 ID，请刷新聊天室列表。"));
            }
            return;
        }
        if (roomCreateDialog) roomCreateDialog->accept();
        roomRestorePending = false;
        bool found = false;
        for (const QJsonValue &value : joinedRooms)
            if (value.toObject().value(QStringLiteral("room_id")).toString() == roomId) found = true;
        if (!found) joinedRooms.append(room);
        roomSidebar->setRooms(joinedRooms);
        requestRoomSwitch(roomId);
        sendRoomRequest(QStringLiteral("list_mine"));
        return;
    }
    if (action == QStringLiteral("switch")) {
        if (!switchInFlight || (ok && result.value(QStringLiteral("room_id")).toString() != pendingRoomId))
            return;
        if (!ok) {
            switchInFlight = false;
            pendingRoomId.clear();
            ui->messageInput->setEnabled(true);
            ui->sendButton->setEnabled(!ui->messageInput->toPlainText().trimmed().isEmpty());
            roomSidebar->setCurrentRoom(currentRoomId);
            roomSidebar->setNotice(result.value(QStringLiteral("error"))
                .toString(QStringLiteral("切换聊天室失败")));
            updateRoomTitle();
            if (!queuedRoomId.isEmpty() && queuedRoomId != currentRoomId) {
                const QString next = queuedRoomId;
                queuedRoomId.clear();
                requestRoomSwitch(next);
            }
            return;
        }
        currentRoomId = pendingRoomId;
        roomHistoryLoading = true;
        clearMessageView();
        selectedMentions.clear();
        onlineMentionMembers.clear();
        if (mentionPopup) mentionPopup->hide();
        roomMembers.clear();
        onlineRoomUserIds.clear();
        onlineNames.clear();
        renderRoomMembers();
        roomSidebar->setCurrentRoom(currentRoomId);
        updateRoomTitle();
        requestRoomMembers();
        return;
    }
    if (action == QStringLiteral("members")) {
        if (!ok || result.value(QStringLiteral("room_id")).toString() != currentRoomId) return;
        if (roomManagementDialog && roomManagementDialog->roomId() == currentRoomId)
            roomManagementDialog->setMembers(result.value(QStringLiteral("members")).toArray());
        roomMembers.clear();
        for (const QJsonValue &value : result.value(QStringLiteral("members")).toArray()) {
            const QJsonObject member = value.toObject();
            bool valid = false;
            const qint64 id = member.value(QStringLiteral("user_id")).toString().toLongLong(&valid);
            const QString name = member.value(QStringLiteral("nickname")).toString();
            if (valid && !name.isEmpty())
                roomMembers.append({id, name,
                    member.contains(QStringLiteral("online")) ? member.value(QStringLiteral("online")).toBool()
                        : onlineRoomUserIds.contains(id),
                    member.value(QStringLiteral("role")).toString()});
        }
        renderRoomMembers();
        updateRoomTitle();
    }
}

bool MainWindow::currentMentionQuery(int *start, QString *query) const
{
    const QTextCursor cursor = ui->messageInput->textCursor();
    if (cursor.hasSelection()) return false;
    const QString prefix = ui->messageInput->toPlainText().left(cursor.position());
    static const QRegularExpression active(QStringLiteral(R"((?<![\p{L}\p{N}_])@([\p{L}\p{N}_]*)$)"));
    const auto match = active.match(prefix);
    if (!match.hasMatch()) return false;
    if (start) *start = match.capturedStart();
    if (query) *query = match.captured(1);
    return true;
}

void MainWindow::setupMentionPopup()
{
    mentionPopup = new QFrame(ui->centralwidget);
    mentionPopup->setObjectName(QStringLiteral("mentionPopup"));
    mentionPopup->setAttribute(Qt::WA_StyledBackground, true);
    mentionPopup->setStyleSheet(QStringLiteral(
        "QFrame#mentionPopup { background:#fff; border:1px solid #e7e0f5; border-radius:14px; } "
        "QListWidget#mentionList { background:transparent; border:0; outline:0; } "
        "QListWidget#mentionList::item { border:0; } "
        "QFrame#mentionRow { background:transparent; border-radius:10px; } "
        "QFrame#mentionRow:hover, QFrame#mentionRow[selected=\"true\"] { background:#f2edfc; }"));
    auto *shadow = new QGraphicsDropShadowEffect(mentionPopup);
    shadow->setBlurRadius(18);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(74, 50, 123, 35));
    mentionPopup->setGraphicsEffect(shadow);
    auto *layout = new QVBoxLayout(mentionPopup);
    layout->setContentsMargins(9, 9, 9, 9);
    layout->setSpacing(5);
    auto *title = new QLabel(QStringLiteral("选择要提及的成员"), mentionPopup);
    title->setStyleSheet(QStringLiteral("color:#756b8e; font-size:12px; padding:2px 5px;"));
    layout->addWidget(title);
    mentionList = new QListWidget(mentionPopup);
    mentionList->setObjectName(QStringLiteral("mentionList"));
    mentionList->setFocusPolicy(Qt::NoFocus);
    mentionList->setMouseTracking(true);
    mentionList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mentionList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mentionList->setSpacing(2);
    layout->addWidget(mentionList);
    mentionEmpty = new QLabel(QStringLiteral("没有找到相关成员"), mentionPopup);
    mentionEmpty->setAlignment(Qt::AlignCenter);
    mentionEmpty->setStyleSheet(QStringLiteral("color:#948aa7; font-size:13px; padding:12px;"));
    layout->addWidget(mentionEmpty);
    connect(mentionList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        mentionList->setCurrentItem(item);
        acceptMention();
    });
    connect(mentionList, &QListWidget::currentRowChanged, this, [this](int) {
        for (int i = 0; i < mentionList->count(); ++i) {
            QWidget *row = mentionList->itemWidget(mentionList->item(i));
            if (!row) continue;
            row->setProperty("selected", i == mentionList->currentRow());
            row->style()->unpolish(row);
            row->style()->polish(row);
        }
    });
    mentionPopup->hide();
}

void MainWindow::positionMentionPopup()
{
    if (!mentionPopup || !mentionPopup->isVisible()) return;
    const QRect chat = ui->chatPanel->geometry();
    const int width = qMin(360, chat.width() - 24);
    const int wanted = 42 + (visibleMentions.isEmpty() ? 49 : qMin(5, visibleMentions.size()) * 58);
    const int inputTop = ui->messageInput->mapTo(ui->centralwidget, QPoint(0, 0)).y();
    const int height = qMin(wanted, qMax(90, inputTop - chat.top() - 18));
    const QPoint cursor = ui->messageInput->viewport()->mapTo(ui->centralwidget,
        ui->messageInput->cursorRect().bottomLeft());
    const int x = qBound(chat.left() + 9, cursor.x() - 24, chat.right() - width - 9);
    mentionPopup->setGeometry(x, inputTop - height - 7, width, height);
    mentionPopup->raise();
}

void MainWindow::updateMentionPopup()
{
    if (!mentionPopup || ui->messageInput->composing()) {
        if (mentionPopup) mentionPopup->hide();
        return;
    }
    int start = 0;
    QString query;
    if (!currentMentionQuery(&start, &query)) { mentionPopup->hide(); return; }
    Q_UNUSED(start);
    visibleMentions.clear();
    if (QStringLiteral("Vivi").contains(query, Qt::CaseInsensitive))
        visibleMentions.append({0, QStringLiteral("Vivi"), true});
    QVector<MentionCandidate> members = onlineMentionMembers;
    if (members.isEmpty())
        for (const QString &name : onlineNames) members.append({0, name, false});
    for (const MentionCandidate &member : members) {
        if ((ownAccountId > 0 && member.id == ownAccountId) ||
            (member.id <= 0 && member.name == nickname)) continue; // No self mention.
        if (member.name.contains(query, Qt::CaseInsensitive)) visibleMentions.append(member);
    }
    mentionList->clear();
    QHash<QString, int> duplicateNames;
    for (const MentionCandidate &member : members) duplicateNames[member.name]++;
    for (const MentionCandidate &member : visibleMentions) {
        auto *item = new QListWidgetItem(mentionList);
        item->setSizeHint(QSize(0, 54));
        auto *row = new QFrame(mentionList);
        row->setObjectName(QStringLiteral("mentionRow"));
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setAttribute(Qt::WA_Hover, true);
        auto *horizontal = new QHBoxLayout(row);
        horizontal->setContentsMargins(9, 5, 9, 5);
        horizontal->setSpacing(10);
        if (member.ai) {
            auto *avatar = new QLabel(row);
            avatar->setFixedSize(38, 38);
            avatar->setAlignment(Qt::AlignCenter);
            avatar->setPixmap(iconFromSvg(QStringLiteral(":/login/vivi-robot.svg"), 22).pixmap(22, 22));
            avatar->setStyleSheet(QStringLiteral("background:#e8defc; border-radius:19px;"));
            horizontal->addWidget(avatar);
        } else horizontal->addWidget(avatarLabel(member.name, row, 38));
        auto *details = new QVBoxLayout;
        details->setContentsMargins(0, 0, 0, 0);
        details->setSpacing(2);
        QString name = member.name;
        if (!member.ai && duplicateNames.value(member.name) > 1 && member.id != 0)
            name += QStringLiteral("  #") + QString::number(member.id);
        auto *label = new QLabel(name, row);
        label->setStyleSheet(QStringLiteral("color:#30234e; font-size:13px; font-weight:600;"));
        details->addWidget(label);
        auto *status = new QLabel(member.ai
            ? QStringLiteral("AI 助手 · ") + (viviStatusLabel ? viviStatusLabel->text() : QStringLiteral("暂时不可用"))
            : QStringLiteral("在线成员 · 在线"), row);
        status->setStyleSheet(QStringLiteral("color:#827799; font-size:11px;"));
        details->addWidget(status);
        horizontal->addLayout(details, 1);
        mentionList->setItemWidget(item, row);
    }
    mentionEmpty->setVisible(visibleMentions.isEmpty());
    mentionList->setVisible(!visibleMentions.isEmpty());
    if (!visibleMentions.isEmpty()) mentionList->setCurrentRow(0);
    mentionPopup->show();
    positionMentionPopup();
}

void MainWindow::acceptMention()
{
    const int index = mentionList ? mentionList->currentRow() : -1;
    if (index < 0 || index >= visibleMentions.size()) return;
    int start = 0;
    QString query;
    if (!currentMentionQuery(&start, &query)) { mentionPopup->hide(); return; }
    const MentionCandidate candidate = visibleMentions.at(index);
    QTextCursor cursor = ui->messageInput->textCursor();
    const int end = cursor.position();
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    const QString mention = QStringLiteral("@") + candidate.name;
    cursor.insertText(mention + QLatin1Char(' '));
    ui->messageInput->setTextCursor(cursor);
    selectedMentions.append({start, int(mention.size()), candidate});
    mentionPopup->hide();
    ui->messageInput->setFocus();
}

void MainWindow::updateSelectedMentionRanges(int position, int removed, int added)
{
    for (int i = selectedMentions.size() - 1; i >= 0; --i) {
        SelectedMention &selected = selectedMentions[i];
        if (position + removed <= selected.start) selected.start += added - removed;
        else if (position >= selected.start + selected.length) continue;
        else selectedMentions.removeAt(i);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->centralwidget && event->type() == QEvent::Resize)
        QTimer::singleShot(0, this, &MainWindow::positionMentionPopup);
    if (watched == ui->messageInput && event->type() == QEvent::KeyPress &&
        mentionPopup && mentionPopup->isVisible() && !ui->messageInput->composing()) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape) { mentionPopup->hide(); return true; }
        if (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down) {
            if (!visibleMentions.isEmpty()) {
                const int direction = key->key() == Qt::Key_Down ? 1 : -1;
                mentionList->setCurrentRow((mentionList->currentRow() + direction + visibleMentions.size()) % visibleMentions.size());
            }
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter || key->key() == Qt::Key_Tab) {
            if (!visibleMentions.isEmpty()) acceptMention();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::appendMessage(const QString &user, const QString &message, bool own, const QString &displayTime,
                               bool ai, const QString &aiRequestId, const QJsonArray &mentions,
                               const QJsonObject &replyTarget, const QString &streamMessageId,
                               const QString &userMessageId)
{
    if (!ai && !userMessageId.isEmpty()) {
        if (renderedUserMessageIds.contains(userMessageId)) return;
        renderedUserMessageIds.insert(userMessageId);
    }
    QScrollBar *scroll = ui->messageScroll->verticalScrollBar();
    const bool followBottom = scroll->value() >= scroll->maximum() - 24;
    const QString messageKey = (ai ? QStringLiteral("ai:") : own ? QStringLiteral("self:") : QStringLiteral("other:")) + user;
    auto *row = new QWidget(ui->messageContainer);
    row->setProperty("mentions", mentions.toVariantList());
    if (!streamMessageId.isEmpty()) row->setProperty("aiMessageId", streamMessageId);
    if (!userMessageId.isEmpty()) row->setProperty("userMessageId", userMessageId);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, lastMessageKey == messageKey ? 0 : 8, 0, 0);
    rowLayout->setSpacing(10);

    auto *avatar = avatarLabel(own ? QStringLiteral("我") : user, row);
    if (ai) {
        avatar->clear();
        avatar->setPixmap(iconFromSvg(QStringLiteral(":/login/vivi-robot.svg"), 22).pixmap(22, 22));
        avatar->setStyleSheet(QStringLiteral("background:#e8defc; border-radius:18px;"));
    }
    auto *column = new QWidget(row);
    auto *columnLayout = new QVBoxLayout(column);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->setSpacing(ai ? 8 : 5);
    auto *captionRow = new QHBoxLayout;
    captionRow->setContentsMargins(0, 0, 0, 0);
    captionRow->setSpacing(8);
    if (own) captionRow->addStretch();
    auto *nameLabel = new QLabel(own ? QStringLiteral("我") : user, column);
    nameLabel->setTextFormat(Qt::PlainText);
    QFont nameFont(QStringLiteral("Microsoft YaHei UI"));
    nameFont.setPixelSize(13);
    nameFont.setWeight(QFont::Medium);
    nameLabel->setFont(nameFont);
    nameLabel->setStyleSheet(ai ? QStringLiteral("color:#7956d5;") : QStringLiteral("color:#685d83;"));
    captionRow->addWidget(nameLabel);
    const QString replyName = replyTarget.value(QStringLiteral("nickname")).toString();
    if (ai && !replyName.isEmpty()) {
        auto *replyLabel = new QLabel(QStringLiteral("回复 @") + replyName, column);
        replyLabel->setObjectName(QStringLiteral("aiReplyLabel"));
        replyLabel->setTextFormat(Qt::PlainText);
        replyLabel->setStyleSheet(QStringLiteral("color:#9185aa; font-size:11px;"));
        captionRow->addWidget(replyLabel);
        row->setProperty("replyAccountId", replyTarget.value(QStringLiteral("accountId")).toString());
    }
    const QString rawTime = displayTime.isEmpty()
        ? QTime::currentTime().toString(QStringLiteral("hh:mm:ss"))
        : displayTime.contains(QLatin1Char('T'))
            ? QDateTime::fromString(displayTime, Qt::ISODateWithMs).time().toString(QStringLiteral("hh:mm:ss"))
            : displayTime;
    auto *timeLabel = new QLabel(rawTime.left(5), column);
    QFont timeFont(QStringLiteral("Microsoft YaHei UI"));
    timeFont.setPixelSize(11);
    timeFont.setWeight(QFont::Normal);
    timeLabel->setFont(timeFont);
    timeLabel->setStyleSheet(QStringLiteral("color:#9a91ad;"));
    captionRow->addWidget(timeLabel);
    if (!own) captionRow->addStretch();
    columnLayout->addLayout(captionRow);
    auto *bubble = new QFrame(column);
    bubble->setStyleSheet(ai
        ? QStringLiteral("QFrame { background-color:#eee8fc; border-radius:14px; border-top-left-radius:7px; } QLabel { color:#30234e; font-size:14px; font-weight:400; }")
        : own
        ? QStringLiteral("QFrame { background-color:#8064d9; border-radius:14px; border-top-right-radius:7px; } QLabel { color:white; font-size:14px; font-weight:400; }")
        : QStringLiteral("QFrame { background-color:#f3f0f9; border-radius:14px; border-top-left-radius:7px; } QLabel { color:#292342; font-size:14px; font-weight:400; }"));
    auto *bubbleLayout = new QVBoxLayout(bubble);
    bubbleLayout->setContentsMargins(ai ? 16 : 14, ai ? 12 : 10,
                                    ai ? 16 : 14, ai ? 12 : 10);
    if (ai) {
        auto *body = new AiMarkdownWidget(message, bubble);
        if (!streamMessageId.isEmpty()) body->setStreamingText({});
        bubbleLayout->addWidget(body);
        aiColumns.append(column);
        aiBubbles.append(bubble);
        aiBodies.append(body);
    } else {
        auto *body = new QLabel(message, bubble);
        body->setTextFormat(Qt::PlainText);
        body->setWordWrap(true);
        body->setTextInteractionFlags(Qt::TextSelectableByMouse);
        body->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        bubbleLayout->addWidget(body);
        bubbleColumns.append(column);
        messageBubbles.append(bubble);
        messageBodies.append(body);
    }
    bubble->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    columnLayout->addWidget(bubble, 0, own ? Qt::AlignRight : Qt::AlignLeft);
    if (ai) {
        auto *actions = new QHBoxLayout;
        actions->setContentsMargins(2, 0, 0, 0);
        actions->setSpacing(4);
        auto makeAction = [column, actions](const QString &label, const QString &iconPath, const QString &tip) {
            auto *button = new QToolButton(column);
            button->setText(label);
            button->setIcon(iconFromSvg(iconPath, 16));
            button->setIconSize(QSize(16, 16));
            button->setToolButtonStyle(label.isEmpty() ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
            button->setToolTip(tip);
            button->setCursor(Qt::PointingHandCursor);
            button->setStyleSheet(QStringLiteral(
                "QToolButton { color:#827799; border:0; border-radius:6px; padding:3px 5px; font-size:12px; background:transparent; } "
                "QToolButton:hover { color:#7956d5; background:#f2edfb; } "
                "QToolButton:checked { color:#6746c4; background:#e9e0fa; }"));
            actions->addWidget(button);
            return button;
        };
        auto *copy = makeAction(QStringLiteral("复制全文"), QStringLiteral(":/login/ai-copy.svg"), QStringLiteral("复制 Vivi 回复"));
        auto *retry = makeAction(QStringLiteral("重新生成"), QStringLiteral(":/login/refresh.svg"), QStringLiteral("按原问题和上下文重新生成"));
        auto *up = makeAction({}, QStringLiteral(":/login/ai-up.svg"), QStringLiteral("有帮助"));
        auto *down = makeAction({}, QStringLiteral(":/login/ai-down.svg"), QStringLiteral("没有帮助"));
        up->setCheckable(true);
        down->setCheckable(true);
        connect(copy, &QToolButton::clicked, this, [this, body = aiBodies.last()]() {
            QGuiApplication::clipboard()->setText(body->plainText());
        });
        retry->setEnabled(!aiRequestId.isEmpty() &&
            (streamMessageId.isEmpty() || replyTarget.value(QStringLiteral("accountId")).toString()
                == QString::number(ownAccountId)));
        connect(retry, &QToolButton::clicked, this, [this, aiRequestId, streamMessageId]() {
            if (!isConnected || socket->state() != QAbstractSocket::ConnectedState) {
                appendSystemMessage(QStringLiteral("无法重新生成：服务器未连接。"));
                return;
            }
            QString originalId = aiRequestId;
            if (!streamMessageId.isEmpty()) {
                auto it = aiStreams.find(streamMessageId);
                if (it == aiStreams.end() || !it->finished || it->raw.isEmpty() ||
                    !it->canRegenerate || it->retryAwaiting) return;
                originalId = it->originalRequestId;
                // Prevent repeated clicks before the server broadcasts the new generation.
                it->retryAwaiting = true;
                for (QToolButton *button : it->actions) button->setEnabled(false);
            }
            socket->write(QStringLiteral("AI_REGENERATE|%1\n").arg(originalId).toUtf8());
        });
        connect(up, &QToolButton::clicked, this, [down](bool checked) { if (checked) down->setChecked(false); });
        connect(down, &QToolButton::clicked, this, [up](bool checked) { if (checked) up->setChecked(false); });
        if (!streamMessageId.isEmpty()) {
            AiStreamState state;
            state.body = aiBodies.last();
            state.actions = {copy, retry, up, down};
            state.requestId = aiRequestId;
            state.originalRequestId = aiRequestId;
            state.canRegenerate = !aiRequestId.startsWith(QStringLiteral("history:")) &&
                replyTarget.value(QStringLiteral("accountId")).toString()
                == QString::number(ownAccountId);
            for (QToolButton *button : state.actions) button->setEnabled(false);
            auto *cancel = makeAction(QStringLiteral("取消生成"), QStringLiteral(":/login/refresh.svg"),
                                      QStringLiteral("取消当前 Vivi 请求"));
            cancel->setVisible(replyTarget.value(QStringLiteral("accountId")).toString()
                               == QString::number(ownAccountId));
            connect(cancel, &QToolButton::clicked, this, [this, streamMessageId]() {
                const auto it = aiStreams.constFind(streamMessageId);
                if (it != aiStreams.cend() && isConnected &&
                    socket->state() == QAbstractSocket::ConnectedState)
                    socket->write(QStringLiteral("AI_CANCEL|%1\n").arg(it->requestId).toUtf8());
            });
            state.cancel = cancel;
            aiStreams.insert(streamMessageId, state);
        }
        actions->addStretch();
        columnLayout->addLayout(actions);
    }
    if (own) {
        rowLayout->addStretch();
        rowLayout->addWidget(column);
        rowLayout->addWidget(avatar, 0, Qt::AlignTop);
    } else {
        rowLayout->addWidget(avatar, 0, Qt::AlignTop);
        rowLayout->addWidget(column);
        rowLayout->addStretch();
    }
    ui->messageLayout->insertWidget(ui->messageLayout->count() - 1, row);
    lastMessageKey = messageKey;
    updateBubbleWidths();
    QTimer::singleShot(0, this, [this, scroll, followBottom]() {
        updateBubbleWidths();
        if (followBottom) {
            ui->messageLayout->activate();
            scroll->setValue(scroll->maximum());
            // QTextDocument reports its final wrapped height after the layout pass.
            const int previousMax = scroll->maximum();
            QTimer::singleShot(40, this, [scroll, previousMax]() {
                if (scroll->value() >= previousMax - 24)
                    scroll->setValue(scroll->maximum());
            });
        }
    });
}

void MainWindow::flushAiStream(const QString &messageId)
{
    auto it = aiStreams.find(messageId);
    if (it == aiStreams.end() || it->finished || !it->body) return;
    QScrollBar *scroll = ui->messageScroll->verticalScrollBar();
    const int oldMax = scroll->maximum();
    const bool followBottom = scroll->value() >= oldMax - 24;
    it->body->setStreamingText(it->displayed);
    ++it->paints;
    const auto follow = [scroll, oldMax, followBottom]() {
        if (followBottom && scroll->value() >= oldMax - 24)
            scroll->setValue(scroll->maximum());
    };
    QTimer::singleShot(0, this, follow);
    QTimer::singleShot(40, this, follow);
}

void MainWindow::advanceAiTypewriter()
{
    bool pending = false;
    QList<QString> readyToFinish;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = aiStreams.begin(); it != aiStreams.end(); ++it) {
        AiStreamState &state = it.value();
        if (state.finished) continue;
        const int backlog = state.raw.size() - state.displayed.size();
        if (backlog <= 0) {
            if (state.completeReceived || state.errorReceived) readyToFinish.append(it.key());
            continue;
        }
        // Keep one trailing grapheme briefly so a combining mark in the next
        // network chunk cannot be shown as a separate character.
        QTextBoundaryFinder boundaries(QTextBoundaryFinder::Grapheme, state.raw);
        boundaries.setPosition(state.displayed.size());
        const int steps = backlog > 1500 ? 6 : backlog > 600 ? 4 : backlog > 200 ? 2 : 1;
        int position = state.displayed.size();
        for (int i = 0; i < steps; ++i) {
            const int next = boundaries.toNextBoundary();
            if (next < 0 || next <= position) break;
            if (next == state.raw.size() && !state.completeReceived && !state.errorReceived
                && now - state.lastChunkMs < 60) break;
            position = next;
        }
        if (position > state.displayed.size()) {
            state.displayed = state.raw.left(position);
            flushAiStream(it.key());
        }
        if (state.displayed.size() == state.raw.size() && (state.completeReceived || state.errorReceived))
            readyToFinish.append(it.key());
        else if (state.displayed.size() < state.raw.size()) pending = true;
    }
    for (const QString &id : readyToFinish) finalizeAiStream(id);
    if (!pending) aiTypewriterTimer->stop();
}

void MainWindow::finalizeAiStream(const QString &messageId)
{
    auto it = aiStreams.find(messageId);
    if (it == aiStreams.end() || it->finished || !it->body) return;
    it->finished = true;
    QScrollBar *scroll = ui->messageScroll->verticalScrollBar();
    const int oldMax = scroll->maximum();
    const bool followBottom = scroll->value() >= oldMax - 24;
    if (it->completeReceived) {
        it->body->finishStreaming(it->raw);
        it->previousContent.clear();
        for (QToolButton *button : it->actions) button->setEnabled(true);
        if (it->actions.size() >= 2) it->actions.at(1)->setEnabled(it->canRegenerate);
    } else {
        if (!it->previousContent.isEmpty()) {
            it->raw = it->previousContent;
            it->displayed = it->raw;
            it->body->finishStreaming(it->raw);
            for (QToolButton *button : it->actions) button->setEnabled(true);
            if (it->actions.size() >= 2) it->actions.at(1)->setEnabled(it->canRegenerate);
        } else {
            it->body->setStreamingText(it->raw.isEmpty() ? it->errorText
                : it->raw + QLatin1Char('\n') + it->errorText);
            if (!it->actions.isEmpty()) it->actions.first()->setEnabled(!it->raw.isEmpty());
        }
    }
    if (it->cancel) it->cancel->hide();
    updateBubbleWidths();
    qInfo() << "Vivi UI stream complete: chunks=" << it->chunksReceived
            << "chunk_span_ms=" << (it->lastChunkMs - it->firstChunkMs)
            << "max_chunk_gap_ms=" << it->maxChunkGapMs
            << "bubble_updates=" << it->paints;
    const auto follow = [scroll, oldMax, followBottom]() {
        if (followBottom && scroll->value() >= oldMax - 24)
            scroll->setValue(scroll->maximum());
    };
    QTimer::singleShot(0, this, follow);
    QTimer::singleShot(40, this, follow);
}

void MainWindow::appendAiStreamEvent(const QJsonObject &event)
{
    const QString kind = event.value(QStringLiteral("event")).toString();
    const QString requestId = event.value(QStringLiteral("request_id")).toString();
    const QString messageId = event.value(QStringLiteral("message_id")).toString();
    const QString generationId = event.value(QStringLiteral("generation_id")).toString();
    if (requestId.isEmpty() || messageId.isEmpty()) return;
    streamedRequestIds.insert(requestId);
    removeAiThinking(requestId);
    const bool existed = aiStreams.contains(messageId);
    if (!existed) {
        const QJsonObject target{{QStringLiteral("accountId"), event.value(QStringLiteral("user_id")).toString()},
                                 {QStringLiteral("nickname"), event.value(QStringLiteral("nickname")).toString()}};
        appendMessage(QStringLiteral("Vivi"), {}, false,
            event.value(QStringLiteral("created_at")).toString(), true, requestId, {}, target, messageId);
    }
    auto it = aiStreams.find(messageId);
    if (it == aiStreams.end() || !it->body) return;
    if (it->generationId.isEmpty()) it->generationId = generationId;
    // History is replayed before AiRoom's active snapshots. Attach the live
    // request to the row already placed by HISTORY, rather than adding a
    // second bubble or rejecting its chunks because the placeholder ID differs.
    if (it->requestId.startsWith(QStringLiteral("history:")) && !it->finished &&
        (kind == QStringLiteral("ai_start") || kind == QStringLiteral("ai_complete") ||
         kind == QStringLiteral("ai_error")) &&
        !event.value(QStringLiteral("regeneration")).toBool()) {
        it->requestId = requestId;
        it->originalRequestId = event.value(QStringLiteral("original_request_id")).toString();
        it->generationId = generationId;
        it->canRegenerate = event.value(QStringLiteral("user_id")).toString()
            == QString::number(ownAccountId);
        if (it->cancel) it->cancel->setVisible(it->canRegenerate);
    }
    if ((kind == QStringLiteral("ai_start") || kind == QStringLiteral("ai_complete")) && existed &&
        event.value(QStringLiteral("regeneration")).toBool() && it->finished &&
        ((generationId != it->generationId &&
          requestId.toULongLong() > it->requestId.toULongLong()) ||
         (it->interrupted && requestId == it->requestId && generationId == it->generationId))) {
        it->previousContent = it->raw.isEmpty()
            ? event.value(QStringLiteral("previous_text")).toString() : it->raw;
        it->body->beginStreaming();
        it->requestId = requestId;
        it->generationId = generationId;
        it->retryAwaiting = false;
        it->interrupted = false;
        it->raw.clear();
        it->displayed.clear();
        it->errorText.clear();
        it->sequence = 0;
        it->finished = false;
        it->completeReceived = false;
        it->errorReceived = false;
        it->chunksReceived = 0;
        it->paints = 0;
        it->firstChunkMs = 0;
        it->lastChunkMs = 0;
        it->maxChunkGapMs = 0;
        for (QToolButton *button : it->actions) button->setEnabled(false);
        if (it->cancel) it->cancel->setVisible(
            event.value(QStringLiteral("user_id")).toString() == QString::number(ownAccountId));
        qInfo() << "Vivi regeneration started: request=" << requestId
                << "message=" << messageId << "generation=" << generationId;
    }
    if (it->finished && it->requestId.startsWith(QStringLiteral("history:")) &&
        kind == QStringLiteral("ai_complete") &&
        it->raw == event.value(QStringLiteral("full_text")).toString()) {
        it->requestId = requestId;
        it->originalRequestId = event.value(QStringLiteral("original_request_id")).toString();
        it->generationId = generationId;
        it->canRegenerate = event.value(QStringLiteral("user_id")).toString()
            == QString::number(ownAccountId);
        if (it->actions.size() >= 2) it->actions.at(1)->setEnabled(it->canRegenerate);
    }
    if (it->finished) return;
    if (it->requestId != requestId || it->generationId != generationId) {
        qWarning() << "Vivi stream event rejected: request/message association mismatch";
        return;
    }
    const QString originalId = event.value(QStringLiteral("original_request_id")).toString();
    if (!originalId.isEmpty()) it->originalRequestId = originalId;
    if (event.value(QStringLiteral("regeneration")).toBool() && it->previousContent.isEmpty())
        it->previousContent = event.value(QStringLiteral("previous_text")).toString();
    const int sequence = event.value(QStringLiteral("sequence")).toInt();
    if (kind == QStringLiteral("ai_start")) {
        if (sequence >= it->sequence) {
            it->sequence = sequence;
            it->raw = event.value(QStringLiteral("full_text")).toString();
            if (!it->raw.isEmpty()) aiTypewriterTimer->start();
        }
    } else if (kind == QStringLiteral("ai_chunk")) {
        if (sequence <= it->sequence) return;
        it->sequence = sequence;
        it->raw += event.value(QStringLiteral("text")).toString();
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (it->chunksReceived == 0) it->firstChunkMs = now;
        else it->maxChunkGapMs = qMax(it->maxChunkGapMs, now - it->lastChunkMs);
        it->lastChunkMs = now;
        ++it->chunksReceived;
        if (!aiTypewriterTimer->isActive()) aiTypewriterTimer->start();
    } else if (kind == QStringLiteral("ai_complete") || kind == QStringLiteral("ai_error")) {
        it->sequence = sequence;
        if (kind == QStringLiteral("ai_complete")) {
            const QString finalText = event.value(QStringLiteral("full_text")).toString();
            if (!finalText.isEmpty() || it->raw.isEmpty()) it->raw = finalText;
            else qWarning() << "Vivi completion contained no text; retaining streamed content"
                            << "characters=" << it->raw.size();
            it->completeReceived = true;
        } else {
            it->errorText = event.value(QStringLiteral("error")).toString();
            it->errorReceived = true;
        }
        // A reconnecting client may receive only the final snapshot.
        if (!existed && it->chunksReceived == 0) it->displayed = it->raw;
        if (!it->raw.startsWith(it->displayed)) it->displayed.clear();
        if (it->displayed.size() == it->raw.size()) finalizeAiStream(messageId);
        else if (!aiTypewriterTimer->isActive()) aiTypewriterTimer->start();
    }
}

void MainWindow::appendAiThinking(const QString &id)
{
    removeAiThinking(id);
    QScrollBar *scroll = ui->messageScroll->verticalScrollBar();
    const bool followBottom = scroll->value() >= scroll->maximum() - 24;
    auto *row = new QWidget(ui->messageContainer);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->addStretch();
    auto *pill = new SystemPill(row, QColor("#eee8fc"));
    auto *inside = new QHBoxLayout(pill);
    inside->setContentsMargins(14, 8, 14, 8);
    inside->setSpacing(8);
    auto *icon = new QLabel(pill);
    icon->setPixmap(iconFromSvg(QStringLiteral(":/login/vivi-robot.svg"), 18).pixmap(18, 18));
    inside->addWidget(icon);
    auto *text = new QLabel(QStringLiteral("Vivi 正在思考…"), pill);
    text->setStyleSheet(QStringLiteral("color:#7956d5; font-size:13px;"));
    inside->addWidget(text);
    auto *pulse = new QTimer(row);
    pulse->setInterval(420);
    connect(pulse, &QTimer::timeout, row, [text, phase = 0]() mutable {
        phase = (phase + 1) % 4;
        text->setText(QStringLiteral("Vivi 正在思考") + QString(phase, QChar(0x2022)));
    });
    pulse->start();
    layout->addWidget(pill);
    layout->addStretch();
    ui->messageLayout->insertWidget(ui->messageLayout->count() - 1, row);
    aiThinkingRows.insert(id, row);
    QTimer::singleShot(0, this, [scroll, followBottom]() {
        if (followBottom) scroll->setValue(scroll->maximum());
    });
}

void MainWindow::removeAiThinking(const QString &id)
{
    QWidget *row = aiThinkingRows.take(id);
    if (!row) return;
    ui->messageLayout->removeWidget(row);
    row->deleteLater();
}

void MainWindow::setAiStatus(const QString &status)
{
    if (!viviStatusLabel || !viviStatusDot) return;
    const bool online = status == QStringLiteral("online");
    const bool thinking = status == QStringLiteral("thinking");
    viviStatusLabel->setText(online ? QStringLiteral("在线")
        : thinking ? QStringLiteral("思考中") : QStringLiteral("暂时不可用"));
    viviStatusDot->setStyleSheet(QStringLiteral("background:%1; border-radius:4px;")
        .arg(online ? QStringLiteral("#31b875") : thinking ? QStringLiteral("#9068f3") : QStringLiteral("#a6a0b3")));
    viviStatusLabel->setStyleSheet(QStringLiteral("color:%1; font-size:12px;")
        .arg(online ? QStringLiteral("#328d62") : thinking ? QStringLiteral("#7956d5") : QStringLiteral("#827a95")));
    viviStatusLabel->parentWidget()->setToolTip(online ? QStringLiteral("Vivi AI 服务已配置")
        : thinking ? QStringLiteral("Vivi 正在处理请求") : QStringLiteral("Vivi AI 服务暂时不可用"));
    if (mentionPopup && mentionPopup->isVisible()) updateMentionPopup();
}

void MainWindow::appendSystemMessage(const QString &message)
{
    lastMessageKey.clear();
    QScrollBar *scroll = ui->messageScroll->verticalScrollBar();
    const bool followBottom = scroll->value() >= scroll->maximum() - 24;
    const QDate today = QDate::currentDate();
    if (lastSystemDate != today) {
        auto *dateRow = new QWidget(ui->messageContainer);
        auto *dateLayout = new QHBoxLayout(dateRow);
        dateLayout->setContentsMargins(0, 0, 0, 2);
        dateLayout->addStretch();
        auto *dateLabel = new RoundedLabel(QStringLiteral("今天 ") + QTime::currentTime().toString(QStringLiteral("hh:mm")), dateRow);
        dateLabel->setAlignment(Qt::AlignCenter);
        dateLabel->setMinimumHeight(32);
        dateLabel->setFillColor(QColor("#f1ecfc"));
        dateLabel->setStyleSheet(QStringLiteral("color:#756991; padding:5px 13px; font-size:12px;"));
        dateLayout->addWidget(dateLabel);
        dateLayout->addStretch();
        ui->messageLayout->insertWidget(ui->messageLayout->count() - 1, dateRow);
        lastSystemDate = today;
    }
    auto *row = new QWidget(ui->messageContainer);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addStretch();
    auto *pill = new SystemPill(row);
    auto *pillLayout = new QHBoxLayout(pill);
    pillLayout->setContentsMargins(19, 8, 19, 8);
    pillLayout->setSpacing(9);
    const bool connectedNotice = message == QStringLiteral("已连接到聊天服务器");
    const bool welcomeNotice = message == QStringLiteral("欢迎来到 VChat 聊天室");
    pill->setProperty("hasIcon", connectedNotice || welcomeNotice);
    if (connectedNotice || welcomeNotice) {
        auto *icon = new QLabel(pill);
        icon->setFixedSize(20, 20);
        const QString path = connectedNotice ? QStringLiteral(":/login/connected.svg")
                                             : QStringLiteral(":/login/members.svg");
        icon->setPixmap(iconFromSvg(path, 20).pixmap(20, 20));
        pillLayout->addWidget(icon, 0, Qt::AlignVCenter);
    }
    auto *label = new QLabel(message, pill);
    label->setObjectName(QStringLiteral("systemMessageText"));
    label->setTextFormat(Qt::PlainText);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    QFont pillFont(QStringLiteral("Microsoft YaHei UI"));
    pillFont.setPixelSize(13);
    pillFont.setWeight(QFont::Medium);
    label->setFont(pillFont);
    label->setStyleSheet(QStringLiteral("color:#59468e; background:transparent;"));
    pillLayout->addWidget(label);
    systemPills.append(pill);
    const int available = qMax(180, int(ui->messageScroll->viewport()->width() * 0.78));
    const int natural = label->fontMetrics().horizontalAdvance(message) + 48 + (connectedNotice || welcomeNotice ? 28 : 0);
    pill->setFixedWidth(qMin(available, qMax(110, natural)));
    pill->setMinimumHeight(40);
    layout->addWidget(pill);
    layout->addStretch();
    ui->messageLayout->insertWidget(ui->messageLayout->count() - 1, row);
    QTimer::singleShot(0, this, [this, scroll, followBottom]() {
        updateBubbleWidths();
        if (followBottom)
            scroll->setValue(scroll->maximum());
    });
}

void MainWindow::appendPresenceNotice(const QString &name, bool joined)
{
    lastMessageKey.clear();
    QScrollBar *scroll = ui->messageScroll->verticalScrollBar();
    const bool followBottom = scroll->value() >= scroll->maximum() - 24;
    auto *row = new QWidget(ui->messageContainer);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addStretch();
    auto *pill = new SystemPill(row, QColor(joined ? "#eee8fc" : "#f1f0f5"));
    pill->setProperty("hasIcon", true);
    auto *pillLayout = new QHBoxLayout(pill);
    pillLayout->setContentsMargins(19, 8, 19, 8);
    pillLayout->setSpacing(8);
    auto *icon = new QLabel(pill);
    icon->setFixedSize(20, 20);
    icon->setPixmap(iconFromSvg(joined ? QStringLiteral(":/login/user-plus.svg")
                                       : QStringLiteral(":/login/log-out.svg"), 20).pixmap(20, 20));
    pillLayout->addWidget(icon, 0, Qt::AlignVCenter);
    auto *label = new QLabel(name + (joined ? QStringLiteral(" 加入了聊天室")
                                           : QStringLiteral(" 离开了聊天室")), pill);
    label->setObjectName(QStringLiteral("systemMessageText"));
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPixelSize(14);
    font.setWeight(QFont::Medium);
    label->setFont(font);
    label->setStyleSheet(joined ? QStringLiteral("color:#7658c4; background:transparent;")
                                : QStringLiteral("color:#8a849a; background:transparent;"));
    pillLayout->addWidget(label);
    const int natural = label->fontMetrics().horizontalAdvance(label->text()) + 48 + 28;
    const int available = qMax(180, int(ui->messageScroll->viewport()->width() * 0.78));
    pill->setFixedWidth(qMin(available, qMax(110, natural)));
    pill->setMinimumHeight(40);
    systemPills.append(pill);
    layout->addWidget(pill);
    layout->addStretch();
    ui->messageLayout->insertWidget(ui->messageLayout->count() - 1, row);
    QTimer::singleShot(0, this, [this, scroll, followBottom]() {
        updateBubbleWidths();
        if (followBottom)
            scroll->setValue(scroll->maximum());
    });
}

void MainWindow::appendIncomingLine(const QString &line)
{
    if (line.startsWith(QStringLiteral("ROOM|notice|"))) {
        const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromBase64(line.mid(12).toLatin1()));
        if (document.isObject()) handleRoomNotice(document.object());
        return;
    }
    if (line.startsWith(QStringLiteral("ROOM|result|"))) {
        const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromBase64(line.mid(12).toLatin1()));
        if (document.isObject()) handleRoomResult(document.object());
        return;
    }
    if (line.startsWith(QStringLiteral("HISTORY|"))) {
        const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromBase64(line.mid(8).toLatin1()));
        if (!document.isObject()) return;
        const QJsonObject item = document.object();
        if (item.value(QStringLiteral("room_id")).toString() != currentRoomId) return;
        const QString id = item.value(QStringLiteral("message_id")).toString();
        if (id.isEmpty()) return;
        const QString body = item.value(QStringLiteral("text")).toString();
        if (item.value(QStringLiteral("type")) == QStringLiteral("ai")) {
            auto existing = aiStreams.find(id);
            if (existing != aiStreams.end()) {
                if (existing->finished && existing->raw != body && existing->body) {
                    existing->body->beginStreaming();
                    existing->body->finishStreaming(body);
                    existing->raw = body;
                    existing->displayed = body;
                    updateBubbleWidths();
                }
                return;
            }
            const QString persistedState = item.value(QStringLiteral("ai_state")).toString();
            const bool pending = persistedState == QStringLiteral("pending");
            const bool failed = persistedState == QStringLiteral("failed") ||
                persistedState == QStringLiteral("cancelled") ||
                persistedState == QStringLiteral("timed_out");
            const QJsonObject event{{QStringLiteral("event"), pending ? QStringLiteral("ai_start")
                : failed ? QStringLiteral("ai_error") : QStringLiteral("ai_complete")},
                {QStringLiteral("request_id"), QStringLiteral("history:") + id},
                {QStringLiteral("message_id"), id},
                {QStringLiteral("generation_id"), QStringLiteral("persisted")},
                {QStringLiteral("original_request_id"), QStringLiteral("0")},
                {QStringLiteral("user_id"), item.value(QStringLiteral("sender_id")).toString()},
                {QStringLiteral("nickname"), item.value(QStringLiteral("reply_to_name")).toString()},
                {QStringLiteral("created_at"), item.value(QStringLiteral("time")).toString()},
                {QStringLiteral("sequence"), 0}, {QStringLiteral("full_text"), pending ? QString() : body},
                {QStringLiteral("error"), failed ? body : QString()}};
            appendAiStreamEvent(event);
            if (pending) {
                auto stream = aiStreams.find(id);
                if (stream != aiStreams.end() && stream->body) {
                    stream->body->setStreamingText(QStringLiteral("Vivi 正在思考…"));
                    if (stream->cancel) stream->cancel->hide();
                }
            }
        } else if (item.value(QStringLiteral("type")) == QStringLiteral("user")) {
            const QString senderId = item.value(QStringLiteral("sender_id")).toString();
            appendMessage(item.value(QStringLiteral("sender")).toString(), body,
                !senderId.isEmpty() && senderId == QString::number(ownAccountId),
                item.value(QStringLiteral("time")).toString(), false, {}, {}, {}, {}, id);
        }
        return;
    }
    if (line == QStringLiteral("HISTORY_END")) {
        if (roomHistoryLoading) {
            roomHistoryLoading = false;
            switchInFlight = false;
            pendingRoomId.clear();
            ui->messageInput->setEnabled(true);
            ui->sendButton->setEnabled(!ui->messageInput->toPlainText().trimmed().isEmpty());
            roomSidebar->setNotice({});
            QSettings settings(QStringLiteral("VChat"), QStringLiteral("VChatClient"));
            settings.setValue(roomSettingsKey, currentRoomId);
            if (!queuedRoomId.isEmpty() && queuedRoomId != currentRoomId) {
                const QString next = queuedRoomId;
                queuedRoomId.clear();
                requestRoomSwitch(next);
            } else updateRoomTitle();
        } else if (!initialHistoryDone) {
            initialHistoryDone = true;
            if (!roomListRequested) {
                roomListRequested = true;
                sendRoomRequest(QStringLiteral("list_mine"));
            }
            refreshPendingInvites();
        }
        return;
    }
    if (line.startsWith(QStringLiteral("USERLIST2|"))) {
        const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromBase64(line.mid(10).toLatin1()));
        if (!document.isArray()) return;
        QVector<MentionCandidate> members;
        for (const QJsonValue &value : document.array()) {
            const QJsonObject object = value.toObject();
            bool valid = false;
            const qint64 id = object.value(QStringLiteral("id")).toString().toLongLong(&valid);
            const QString name = object.value(QStringLiteral("name")).toString();
            if (valid && !name.isEmpty()) members.append({id, name, false});
        }
        onlineMentionMembers = members;
        onlineRoomUserIds.clear();
        for (const MentionCandidate &member : members) onlineRoomUserIds.insert(member.id);
        if (roomListLoaded) {
            requestRoomMembers(); // Membership may have changed since the last snapshot.
        }
        mentionProtocol = true;
        if (mentionPopup && mentionPopup->isVisible()) updateMentionPopup();
        return;
    }
    if (line.startsWith(QStringLiteral("CHAT2|"))) {
        const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromBase64(line.mid(6).toLatin1()));
        if (!document.isObject()) return;
        const QJsonObject payload = document.object();
        const QString roomId = payload.value(QStringLiteral("room_id")).toString();
        if ((!roomId.isEmpty() && roomId != currentRoomId) ||
            (roomId.isEmpty() && currentRoomId != QStringLiteral("class-room"))) return;
        appendMessage(payload.value(QStringLiteral("sender")).toString(),
            payload.value(QStringLiteral("text")).toString(), false,
            payload.value(QStringLiteral("time")).toString(), false, {},
            payload.value(QStringLiteral("mentions")).toArray(), {}, {},
            payload.value(QStringLiteral("messageId")).toString());
        return;
    }
    if (line.startsWith(QStringLiteral("AI|"))) {
        const QStringList parts = line.split(QLatin1Char('|'));
        if (parts.size() != 4) return;
        const QString state = parts.at(1);
        const QString id = parts.at(2);
        const QString content = QString::fromUtf8(QByteArray::fromBase64(parts.at(3).toLatin1()));
        if (state == QStringLiteral("stream")) {
            const QJsonDocument document = QJsonDocument::fromJson(content.toUtf8());
            if (document.isObject() &&
                document.object().value(QStringLiteral("room_id")).toString() == currentRoomId &&
                document.object().value(QStringLiteral("request_id")).toString() == id)
                appendAiStreamEvent(document.object());
            return;
        }
        if (state == QStringLiteral("status")) setAiStatus(content);
        else if (state == QStringLiteral("meta")) {
            const QJsonDocument metadata = QJsonDocument::fromJson(content.toUtf8());
            if (metadata.isObject()) aiReplyTargets.insert(id, metadata.object());
        }
        else if (state == QStringLiteral("thinking")) {
            if (!streamedRequestIds.contains(id)) appendAiThinking(id);
        }
        else if (state == QStringLiteral("reply")) {
            if (streamedRequestIds.contains(id)) { aiReplyTargets.remove(id); return; }
            removeAiThinking(id);
            appendMessage(QStringLiteral("Vivi"), content, false, QString(), true, id, {}, aiReplyTargets.take(id));
        } else if (state == QStringLiteral("error")) {
            if (id == QStringLiteral("0")) {
                for (auto it = aiStreams.begin(); it != aiStreams.end(); ++it) {
                    if (!it->retryAwaiting) continue;
                    it->retryAwaiting = false;
                    for (QToolButton *button : it->actions) button->setEnabled(true);
                    if (it->actions.size() >= 2) it->actions.at(1)->setEnabled(it->canRegenerate);
                }
            }
            if (streamedRequestIds.contains(id)) return;
            removeAiThinking(id);
            aiReplyTargets.remove(id);
            appendSystemMessage(QStringLiteral("Vivi：") + content);
        }
        return;
    }
    if (line.startsWith(QStringLiteral("USERLIST|"))) {
        QStringList users = line.mid(QStringLiteral("USERLIST|").size()).split(',', Qt::SkipEmptyParts);
        for (QString &user : users)
            user = user.trimmed();
        updateUserList(users);
        return;
    }
    if (line.startsWith(QStringLiteral("[System]"))) {
        const QString notice = line.mid(QStringLiteral("[System]").size()).trimmed();
        if (notice == QStringLiteral("Connected to chat server") ||
            notice == QStringLiteral("Welcome %1").arg(nickname)) {
            return; // Already represented by local connection and welcome notices.
        }
        const QString joinedSuffix = QStringLiteral(" joined the chat");
        const QString leftSuffix = QStringLiteral(" left the chat");
        if (notice.endsWith(joinedSuffix) && notice.size() > joinedSuffix.size()) {
            appendPresenceNotice(notice.left(notice.size() - joinedSuffix.size()), true);
            return;
        }
        if (notice.endsWith(leftSuffix) && notice.size() > leftSuffix.size()) {
            appendPresenceNotice(notice.left(notice.size() - leftSuffix.size()), false);
            return;
        }
        appendSystemMessage(notice);
        return;
    }

    // The server forwards [hh:mm:ss] nickname: text. Preserve its time and sender.
    static const QRegularExpression timePrefix(QStringLiteral(R"(^\[(\d{2}:\d{2}:\d{2})\] (.*)$)"));
    const QRegularExpressionMatch match = timePrefix.match(line);
    const QString time = match.hasMatch() ? match.captured(1) : QString();
    const QString payload = match.hasMatch() ? match.captured(2) : line;
    QString sender;
    QString body;
    // Match the longest online nickname first, including names that contain a colon.
    for (const QString &candidate : onlineNames) {
        const QString prefix = candidate + QStringLiteral(": ");
        if (payload.startsWith(prefix) && candidate.size() > sender.size()) {
            sender = candidate;
            body = payload.mid(prefix.size());
        }
    }
    if (sender.isEmpty()) {
        const int separator = payload.indexOf(QStringLiteral(": "));
        if (separator > 0) {
            sender = payload.left(separator);
            body = payload.mid(separator + 2);
        }
    }
    if (sender.isEmpty()) {
        appendSystemMessage(payload);
        return;
    }
    appendMessage(sender, body, false, time);
}

void MainWindow::updateUserList(const QStringList &users)
{
    if (roomListLoaded) {
        onlineNames = users;
        renderRoomMembers();
        return;
    }
    QStringList incomingNames;
    if (isConnected) {
        for (const QString &user : users) {
            if (!user.trimmed().isEmpty())
                incomingNames.append(user.trimmed());
        }
    }
    // Identical broadcasts need no widget rebuild, which also avoids visible flicker.
    if (incomingNames == onlineNames && renderedUserListOwner == nickname)
        return;
    onlineNames = incomingNames;
    renderedUserListOwner = nickname;

    // USERLIST contains names only. The nickname sent in JOIN is the strongest
    // identity available without changing the protocol. Do not invent a missing self.
    QStringList displayNames = onlineNames;
    const int selfPosition = displayNames.indexOf(nickname);
    if (selfPosition > 0)
        displayNames.move(selfPosition, 0);
    ui->userList->clear();
    for (int index = 0; index < displayNames.size(); ++index) {
        const QString &user = displayNames.at(index);
        const bool isSelf = selfPosition >= 0 && index == 0;
        auto *item = new QListWidgetItem(ui->userList);
        item->setSizeHint(QSize(0, 72));
        auto *row = new QFrame(ui->userList);
        row->setObjectName(QStringLiteral("memberCard"));
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setAttribute(Qt::WA_Hover, true);
        row->setStyleSheet(isSelf
            ? QStringLiteral("QFrame#memberCard { background:#f5f0fd; border-radius:13px; } QFrame#memberCard:hover { background:#efe7fa; }")
            : QStringLiteral("QFrame#memberCard { background:transparent; border-radius:13px; } QFrame#memberCard:hover { background:#f7f4fc; }"));
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(12, 8, 12, 8);
        layout->setSpacing(12);
        layout->addWidget(avatarLabel(user, row, 42));
        auto *details = new QVBoxLayout();
        details->setSpacing(4);
        auto *name = new QLabel(isSelf ? user + QStringLiteral("（我）") : user, row);
        name->setTextFormat(Qt::PlainText);
        name->setStyleSheet(QStringLiteral("color:#252044; font-size:14px; font-weight:600;"));
        details->addWidget(name);
        auto *status = new QHBoxLayout();
        status->setContentsMargins(0, 0, 0, 0);
        status->setSpacing(6);
        auto *dot = new QLabel(row);
        dot->setFixedSize(8, 8);
        dot->setStyleSheet(QStringLiteral("background-color:#31b875; border-radius:4px;"));
        status->addWidget(dot);
        auto *online = new QLabel(QStringLiteral("在线"), row);
        online->setStyleSheet(QStringLiteral("color:#79718f; font-size:12px;"));
        status->addWidget(online);
        status->addStretch();
        details->addLayout(status);
        layout->addLayout(details, 1);
        ui->userList->setItemWidget(item, row);
    }
    const QString count = QString::number(onlineNames.size());
    ui->onlineBadge->setText(count + QStringLiteral(" 人在线"));
    ui->onlineCountLabel->setText(QStringLiteral("在线人数：") + count);
    updateMemberListHeight();
    if (mentionPopup && mentionPopup->isVisible()) updateMentionPopup();
}

void MainWindow::renderRoomMembers()
{
    if (!roomListLoaded) return;
    QVector<RoomMember> display = roomMembers;
    std::stable_sort(display.begin(), display.end(), [this](const RoomMember &a, const RoomMember &b) {
        const auto rank = [this](const RoomMember &m) {
            if (m.id == ownAccountId) return 0;
            if (m.role == QStringLiteral("owner")) return 1;
            return m.online ? 2 : 3;
        };
        return rank(a) < rank(b);
    });
    ui->userList->clear();
    int onlineCount = 0;
    for (const RoomMember &member : display) {
        if (member.online) ++onlineCount;
        const bool isSelf = member.id == ownAccountId;
        auto *item = new QListWidgetItem(ui->userList);
        item->setSizeHint(QSize(0, 72));
        auto *row = new QFrame(ui->userList);
        row->setObjectName(QStringLiteral("memberCard"));
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setAttribute(Qt::WA_Hover, true);
        row->setStyleSheet(isSelf
            ? QStringLiteral("QFrame#memberCard { background:#f5f0fd; border-radius:13px; } QFrame#memberCard:hover { background:#efe7fa; }")
            : QStringLiteral("QFrame#memberCard { background:transparent; border-radius:13px; } QFrame#memberCard:hover { background:#f7f4fc; }"));
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(12, 8, 12, 8);
        layout->setSpacing(12);
        layout->addWidget(avatarLabel(member.name, row, 42));
        auto *details = new QVBoxLayout;
        details->setSpacing(4);
        auto *name = new QLabel(isSelf ? member.name + QStringLiteral("（我）") : member.name, row);
        name->setTextFormat(Qt::PlainText);
        name->setStyleSheet(QStringLiteral("color:#252044; font-size:14px; font-weight:600;"));
        if (member.role == QStringLiteral("owner") && currentRoomId != QStringLiteral("class-room")) {
            auto *nameRow = new QHBoxLayout;
            nameRow->setContentsMargins(0, 0, 0, 0);
            nameRow->setSpacing(4);
            nameRow->addWidget(name);
            auto *owner = new QLabel(QStringLiteral("房主"), row);
            owner->setStyleSheet(QStringLiteral("color:#8058c9; background:#eee6fc;"
                " border-radius:7px; padding:1px 3px; font-size:10px;"));
            nameRow->addWidget(owner);
            nameRow->addStretch();
            details->addLayout(nameRow);
        } else {
            details->addWidget(name);
        }
        auto *status = new QHBoxLayout;
        status->setContentsMargins(0, 0, 0, 0);
        status->setSpacing(6);
        auto *dot = new QLabel(row);
        dot->setFixedSize(8, 8);
        dot->setStyleSheet(member.online
            ? QStringLiteral("background:#31b875; border-radius:4px;")
            : QStringLiteral("background:#b7afc6; border-radius:4px;"));
        status->addWidget(dot);
        auto *state = new QLabel(member.online ? QStringLiteral("在线") : QStringLiteral("离线"), row);
        state->setStyleSheet(QStringLiteral("color:#79718f; font-size:12px;"));
        status->addWidget(state);
        status->addStretch();
        details->addLayout(status);
        layout->addLayout(details, 1);
        ui->userList->setItemWidget(item, row);
    }
    ui->onlineTitle->setText(QStringLiteral("聊天室成员"));
    ui->onlineBadge->setText(QStringLiteral("%1 人在线").arg(onlineCount));
    ui->onlineCountLabel->setText(QStringLiteral("聊天室成员：%1 · 在线：%2")
        .arg(display.size()).arg(onlineCount));
    updateMemberListHeight();
}

void MainWindow::updateMemberListHeight()
{
    // The real user list scrolls when crowded; Vivi stays in its own card below it.
    const int needed = (roomListLoaded ? roomMembers.size() : onlineNames.size()) * 76;
    const int available = qMax(100, ui->rightPanel->height() - 215);
    ui->userList->setFixedHeight(qMin(needed, available));
}

void MainWindow::updateBubbleWidths()
{
    const int limit = qMax(180, int(ui->messageScroll->viewport()->width() * 0.68));
    for (QWidget *column : bubbleColumns)
        column->setMaximumWidth(limit);
    for (int i = 0; i < messageBubbles.size(); ++i) {
        QFrame *bubble = messageBubbles.at(i);
        QLabel *body = messageBodies.at(i);
        int natural = 0;
        for (const QString &line : body->text().split('\n'))
            natural = qMax(natural, body->fontMetrics().horizontalAdvance(line));
        bubble->setMaximumWidth(limit);
        bubble->setMinimumWidth(qMin(limit, qMax(56, natural + 28)));
    }
    for (int i = 0; i < aiBubbles.size(); ++i) {
        aiColumns.at(i)->setMaximumWidth(limit);
        QFrame *bubble = aiBubbles.at(i);
        AiMarkdownWidget *body = aiBodies.at(i);
        const int width = qMax(140, limit - 28);
        body->setContentWidth(width);
        bubble->setMaximumWidth(limit);
        bubble->setMinimumWidth(qMin(limit, width + 28));
    }
    const int systemLimit = qMax(180, int(ui->messageScroll->viewport()->width() * 0.78));
    for (QFrame *pill : systemPills) {
        QLabel *label = pill->findChild<QLabel *>(QStringLiteral("systemMessageText"));
        const bool hasIcon = pill->property("hasIcon").toBool();
        const int natural = label->fontMetrics().horizontalAdvance(label->text()) + 48 + (hasIcon ? 28 : 0);
        pill->setFixedWidth(qMin(systemLimit, qMax(110, natural)));
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (ui && ui->messageScroll) {
        updateBubbleWidths();
        QTimer::singleShot(0, this, &MainWindow::updateMemberListHeight);
    }
}
