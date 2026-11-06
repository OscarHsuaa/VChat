#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFont>
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
#include <QSvgRenderer>
#include <QTimer>
#include <QVBoxLayout>

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

QLabel *avatarLabel(const QString &name, QWidget *parent, int size = 38)
{
    auto *avatar = new QLabel(name.left(1).isEmpty() ? QStringLiteral("我") : name.left(1), parent);
    avatar->setTextFormat(Qt::PlainText);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setFixedSize(size, size);
    avatar->setStyleSheet(QStringLiteral("color:#694ac5; background:#eee5fc; border-radius:%1px; font-size:17px; font-weight:600;").arg(size / 2));
    return avatar;
}

class SystemPill : public QFrame
{
public:
    using QFrame::QFrame;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#f1ecfc"));
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 20, 20);
    }
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

MainWindow::MainWindow(const QString &loginName, const QString &serverAddress, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , nickname(loginName.trimmed().isEmpty() ? QStringLiteral("User") : loginName.trimmed())
    , isConnected(false)
    , socket(new QTcpSocket(this))
{
    ui->setupUi(this);

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
    delete ui;
}

void MainWindow::setupUI()
{
    setWindowTitle(QStringLiteral("VChat - 聊天室"));
    menuBar()->hide();
    statusBar()->hide();
    ui->logoLabel->setPixmap(chatLogo().scaled(50, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ui->bodyLayout->setStretch(0, 3);
    ui->bodyLayout->setStretch(1, 1);
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
    connect(ui->messageInput, &QLineEdit::textChanged, ui->sendButton,
            [this](const QString &text) { ui->sendButton->setEnabled(!text.trimmed().isEmpty()); });
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
    auto *viviDot = new QLabel(viviCard);
    viviDot->setFixedSize(8, 8);
    viviDot->setStyleSheet(QStringLiteral("background:#a6a0b3; border-radius:4px;"));
    viviStatusRow->addWidget(viviDot);
    auto *viviStatus = new QLabel(QStringLiteral("功能开发中"), viviCard);
    viviStatus->setStyleSheet(QStringLiteral("color:#827a95; font-size:12px;"));
    viviStatusRow->addWidget(viviStatus);
    viviStatusRow->addStretch();
    viviDetails->addLayout(viviStatusRow);
    viviLayout->addLayout(viviDetails, 1);
    ui->rightLayout->insertWidget(3, viviCard);
    ui->rightLayout->insertStretch(4, 1);
    ui->messageLayout->setSpacing(12);
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
        ui->serverStatusLabel->setText(QStringLiteral("● 服务器已连接"));
        ui->serverStatusLabel->setStyleSheet("color:#31b875; font-size:12px;");
        appendSystemMessage("已连接到聊天服务器");
        statusBar()->showMessage("已连接到聊天服务器");
        sendJoinMessage();
    });

    // 断开连接后，界面状态恢复为“未连接”，在线列表也只显示自己。
    connect(socket, &QTcpSocket::disconnected, this, [this]() {
        isConnected = false;
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
        ui->serverStatusLabel->setText(QStringLiteral("● 服务器未连接"));
        ui->serverStatusLabel->setStyleSheet("color:#9b6b7d; font-size:12px;");
        appendSystemMessage("连接服务器失败: " + socket->errorString());
        statusBar()->showMessage("连接服务器失败");
    });
}

void MainWindow::on_sendButton_clicked()
{
    const QString message = ui->messageInput->text().trimmed();
    if (message.isEmpty()) {
        return;
    }

    if (!isConnected || socket->state() != QAbstractSocket::ConnectedState) {
        appendSystemMessage("请先连接服务器");
        return;
    }

    // 本项目使用最简单的文本协议：每条消息以换行符 \n 结束。
    // 服务器收到 “昵称: 内容” 后，会转发给其他在线客户端，并保存到数据库。
    const QString networkMessage = nickname + ": " + message + "\n";
    if (socket->write(networkMessage.toUtf8()) == -1) {
        appendSystemMessage("消息发送失败: " + socket->errorString());
        return;
    }

    appendMessage(nickname, message, true);
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
        // JOIN 协议只负责“上线通知”，不保存到聊天记录表。
        socket->write(("JOIN|" + nickname + "\n").toUtf8());
    }
}

void MainWindow::appendMessage(const QString &user, const QString &message, bool own, const QString &displayTime)
{
    QScrollBar *scroll = ui->messageScroll->verticalScrollBar();
    const bool followBottom = scroll->value() >= scroll->maximum() - 24;
    auto *row = new QWidget(ui->messageContainer);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(10);

    auto *avatar = avatarLabel(own ? QStringLiteral("我") : user, row);
    auto *column = new QWidget(row);
    auto *columnLayout = new QVBoxLayout(column);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->setSpacing(6);
    auto *caption = new QLabel((own ? QStringLiteral("我") : user) + QStringLiteral(" · ") +
                               (displayTime.isEmpty() ? QTime::currentTime().toString(QStringLiteral("hh:mm:ss")) : displayTime), column);
    caption->setTextFormat(Qt::PlainText);
    caption->setAlignment(own ? Qt::AlignRight : Qt::AlignLeft);
    caption->setStyleSheet(QStringLiteral("color:#79718f; font-size:12px; font-weight:500;"));
    columnLayout->addWidget(caption);
    auto *bubble = new QFrame(column);
    bubble->setStyleSheet(own
        ? QStringLiteral("QFrame { background-color:#8064d9; border-radius:16px; } QLabel { color:white; font-size:14px; font-weight:400; }")
        : QStringLiteral("QFrame { background-color:#f2eff8; border-radius:16px; } QLabel { color:#292342; font-size:14px; font-weight:400; }"));
    auto *bubbleLayout = new QVBoxLayout(bubble);
    bubbleLayout->setContentsMargins(15, 10, 15, 10);
    auto *body = new QLabel(message, bubble);
    body->setTextFormat(Qt::PlainText);
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    bubbleLayout->addWidget(body);
    bubble->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    columnLayout->addWidget(bubble, 0, own ? Qt::AlignRight : Qt::AlignLeft);
    bubbleColumns.append(column);
    messageBubbles.append(bubble);
    messageBodies.append(body);
    if (own) {
        rowLayout->addStretch();
        rowLayout->addWidget(column);
        rowLayout->addWidget(avatar, 0, Qt::AlignBottom);
    } else {
        rowLayout->addWidget(avatar, 0, Qt::AlignBottom);
        rowLayout->addWidget(column);
        rowLayout->addStretch();
    }
    ui->messageLayout->insertWidget(ui->messageLayout->count() - 1, row);
    updateBubbleWidths();
    QTimer::singleShot(0, this, [this, scroll, followBottom]() {
        updateBubbleWidths();
        if (followBottom)
            scroll->setValue(scroll->maximum());
    });
}

void MainWindow::appendSystemMessage(const QString &message)
{
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
    const int natural = label->fontMetrics().horizontalAdvance(message) + 38 + (connectedNotice || welcomeNotice ? 29 : 0);
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

void MainWindow::appendIncomingLine(const QString &line)
{
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
    onlineNames.clear();
    if (isConnected) {
        for (const QString &user : users) {
            if (!user.trimmed().isEmpty())
                onlineNames.append(user.trimmed());
        }
    }
    ui->userList->clear();
    for (const QString &user : onlineNames) {
        auto *item = new QListWidgetItem(ui->userList);
        item->setSizeHint(QSize(0, 72));
        auto *row = new QFrame(ui->userList);
        row->setObjectName(QStringLiteral("memberCard"));
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setAttribute(Qt::WA_Hover, true);
        row->setStyleSheet(user == nickname
            ? QStringLiteral("QFrame#memberCard { background:#f5f0fd; border-radius:13px; } QFrame#memberCard:hover { background:#efe7fa; }")
            : QStringLiteral("QFrame#memberCard { background:transparent; border-radius:13px; } QFrame#memberCard:hover { background:#f7f4fc; }"));
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(12, 8, 12, 8);
        layout->setSpacing(12);
        layout->addWidget(avatarLabel(user, row, 42));
        auto *details = new QVBoxLayout();
        details->setSpacing(4);
        auto *name = new QLabel(user == nickname ? user + QStringLiteral("（我）") : user, row);
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
}

void MainWindow::updateMemberListHeight()
{
    // The real user list scrolls when crowded; Vivi stays in its own card below it.
    const int needed = onlineNames.size() * 76;
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
        bubble->setMinimumWidth(qMin(limit, qMax(56, natural + 30)));
    }
    const int systemLimit = qMax(180, int(ui->messageScroll->viewport()->width() * 0.78));
    for (QFrame *pill : systemPills) {
        QLabel *label = pill->findChild<QLabel *>(QStringLiteral("systemMessageText"));
        const bool hasIcon = label->text() == QStringLiteral("已连接到聊天服务器") ||
                             label->text() == QStringLiteral("欢迎来到 VChat 聊天室");
        const int natural = label->fontMetrics().horizontalAdvance(label->text()) + 38 + (hasIcon ? 29 : 0);
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
