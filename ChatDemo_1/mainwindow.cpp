#include "mainwindow.h"
#include "aimarkdownwidget.h"
#include "mentioninput.h"
#include "ui_mainwindow.h"

#include <QFont>
#include <QEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QClipboard>
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
#include <QStyle>
#include <QSvgRenderer>
#include <QTimer>
#include <QToolButton>
#include <QTextCursor>
#include <QTextDocument>
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
    ui->bodyLayout->setStretch(0, 3);
    ui->bodyLayout->setStretch(1, 1);
    // QTextEdit advertises vertical expansion through its parent layout. Keep
    // the original one-line composer height and give spare space to messages.
    ui->messageScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->composer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->composer->setFixedHeight(76);
    ui->chatPanelLayout->setStretch(0, 1);
    ui->chatPanelLayout->setStretch(1, 0);
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
        ui->serverStatusLabel->setText(QStringLiteral("● 服务器已连接"));
        ui->serverStatusLabel->setStyleSheet("color:#31b875; font-size:12px;");
        appendSystemMessage("已连接到聊天服务器");
        statusBar()->showMessage("已连接到聊天服务器");
        sendJoinMessage();
    });

    // 断开连接后，界面状态恢复为“未连接”，在线列表也只显示自己。
    connect(socket, &QTcpSocket::disconnected, this, [this]() {
        isConnected = false;
        mentionProtocol = false;
        onlineMentionMembers.clear();
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

    // 本项目使用最简单的文本协议：每条消息以换行符 \n 结束。
    // 服务器收到 “昵称: 内容” 后，会转发给其他在线客户端，并保存到数据库。
    QByteArray networkMessage;
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
        const QJsonObject payload{{QStringLiteral("text"), message}, {QStringLiteral("mentions"), mentions}};
        networkMessage = "CHAT2|" + QJsonDocument(payload).toJson(QJsonDocument::Compact).toBase64() + '\n';
    } else {
        networkMessage = (nickname + ": " + message + "\n").toUtf8();
    }
    if (socket->write(networkMessage) == -1) {
        appendSystemMessage("消息发送失败: " + socket->errorString());
        return;
    }

    appendMessage(nickname, message, true, {}, false, {}, mentions);
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
        // JOIN 协议只负责“上线通知”，不保存到聊天记录表。
        const QString join = sessionToken.isEmpty() ? QStringLiteral("JOIN|%1\n").arg(nickname)
            : QStringLiteral("JOIN|%1|%2\n").arg(nickname, sessionToken);
        socket->write(join.toUtf8());
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
                               const QJsonObject &replyTarget)
{
    QScrollBar *scroll = ui->messageScroll->verticalScrollBar();
    const bool followBottom = scroll->value() >= scroll->maximum() - 24;
    const QString messageKey = (ai ? QStringLiteral("ai:") : own ? QStringLiteral("self:") : QStringLiteral("other:")) + user;
    auto *row = new QWidget(ui->messageContainer);
    row->setProperty("mentions", mentions.toVariantList());
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
        ? QTime::currentTime().toString(QStringLiteral("hh:mm:ss")) : displayTime;
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
        retry->setEnabled(!aiRequestId.isEmpty());
        connect(retry, &QToolButton::clicked, this, [this, aiRequestId]() {
            if (!isConnected || socket->state() != QAbstractSocket::ConnectedState) {
                appendSystemMessage(QStringLiteral("无法重新生成：服务器未连接。"));
                return;
            }
            socket->write(QStringLiteral("AI_REGENERATE|%1\n").arg(aiRequestId).toUtf8());
        });
        connect(up, &QToolButton::clicked, this, [down](bool checked) { if (checked) down->setChecked(false); });
        connect(down, &QToolButton::clicked, this, [up](bool checked) { if (checked) up->setChecked(false); });
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
        mentionProtocol = true;
        if (mentionPopup && mentionPopup->isVisible()) updateMentionPopup();
        return;
    }
    if (line.startsWith(QStringLiteral("CHAT2|"))) {
        const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromBase64(line.mid(6).toLatin1()));
        if (!document.isObject()) return;
        const QJsonObject payload = document.object();
        appendMessage(payload.value(QStringLiteral("sender")).toString(),
            payload.value(QStringLiteral("text")).toString(), false,
            payload.value(QStringLiteral("time")).toString(), false, {},
            payload.value(QStringLiteral("mentions")).toArray());
        return;
    }
    if (line.startsWith(QStringLiteral("AI|"))) {
        const QStringList parts = line.split(QLatin1Char('|'));
        if (parts.size() != 4) return;
        const QString state = parts.at(1);
        const QString id = parts.at(2);
        const QString content = QString::fromUtf8(QByteArray::fromBase64(parts.at(3).toLatin1()));
        if (state == QStringLiteral("status")) setAiStatus(content);
        else if (state == QStringLiteral("meta")) {
            const QJsonDocument metadata = QJsonDocument::fromJson(content.toUtf8());
            if (metadata.isObject()) aiReplyTargets.insert(id, metadata.object());
        }
        else if (state == QStringLiteral("thinking")) appendAiThinking(id);
        else if (state == QStringLiteral("reply")) {
            removeAiThinking(id);
            appendMessage(QStringLiteral("Vivi"), content, false, QString(), true, id, {}, aiReplyTargets.take(id));
        } else if (state == QStringLiteral("error")) {
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
