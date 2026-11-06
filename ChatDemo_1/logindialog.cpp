#include "logindialog.h"
#include "ui_logindialog.h"

#include <QEvent>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QIcon>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSettings>
#include <QSvgRenderer>
#include <QStyle>
#include <QTcpSocket>
#include <QToolButton>

namespace {
QIcon svgIcon(const QString &path, int logicalSize = 18)
{
    QSvgRenderer renderer(path);
    QPixmap pixmap(logicalSize * 2, logicalSize * 2);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(0, 0, logicalSize, logicalSize));
    return QIcon(pixmap);
}

void setLoginFont(QWidget *widget, int pixels, QFont::Weight weight)
{
    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPixelSize(pixels);
    font.setWeight(weight);
    widget->setFont(font);
}

QPixmap brandLogo()
{
    QPixmap pixmap(96, 96);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath bubble;
    bubble.addRoundedRect(QRectF(7, 8, 78, 68), 22, 22);
    bubble.moveTo(24, 70);
    bubble.lineTo(21, 87);
    bubble.lineTo(43, 75);
    QLinearGradient fill(12, 8, 82, 78);
    fill.setColorAt(0, QColor("#5f63df"));
    fill.setColorAt(1, QColor("#9c6cda"));
    painter.setPen(QPen(QColor("#675fe0"), 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(fill);
    painter.drawPath(bubble);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#ffffff"));
    for (int x : {29, 46, 63})
        painter.drawEllipse(QPointF(x, 42), 5, 5);
    return pixmap;
}
}

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
    , serverEdit(new QLineEdit(this))
{
    ui->setupUi(this);
    setLoginFont(this, 13, QFont::Medium);
    setLoginFont(ui->titleLabel, 32, QFont::Bold);
    setLoginFont(ui->subtitleLabel, 14, QFont::Medium);
    setLoginFont(ui->serverToggleButton, 14, QFont::Medium);
    setLoginFont(ui->serverAddressLabel, 13, QFont::Medium);
    setLoginFont(ui->usernameEdit, 14, QFont::Medium);
    setLoginFont(ui->passwordEdit, 14, QFont::Medium);
    setLoginFont(ui->captchaEdit, 14, QFont::Medium);
    setLoginFont(ui->loginButton, 16, QFont::DemiBold);
    setLoginFont(ui->rememberAccountCheck, 13, QFont::Normal);
    setLoginFont(ui->registrationInfoLabel, 13, QFont::Normal);
    for (QLineEdit *edit : {ui->usernameEdit, ui->passwordEdit, ui->captchaEdit}) {
        QPalette palette = edit->palette();
        palette.setColor(QPalette::PlaceholderText, QColor("#6a627d"));
        edit->setPalette(palette);
    }
    ui->serverToggleButton->setIcon(svgIcon(QStringLiteral(":/login/server.svg")));
    ui->serverToggleButton->setIconSize(QSize(18, 18));
    ui->userIcon->setPixmap(svgIcon(QStringLiteral(":/login/user.svg")).pixmap(18, 18));
    ui->passwordIcon->setPixmap(svgIcon(QStringLiteral(":/login/lock.svg")).pixmap(18, 18));
    ui->captchaIcon->setPixmap(svgIcon(QStringLiteral(":/login/shield.svg")).pixmap(18, 18));
    for (QLineEdit *edit : {ui->usernameEdit, ui->passwordEdit, ui->captchaEdit})
        edit->installEventFilter(this);
    ui->eyeButton->setIcon(svgIcon(QStringLiteral(":/login/eye-closed.svg"), 18));
    ui->eyeButton->setIconSize(QSize(18, 18));
    ui->refreshCaptchaButton->setIcon(svgIcon(QStringLiteral(":/login/refresh.svg"), 18));
    ui->refreshCaptchaButton->setIconSize(QSize(18, 18));
    ui->logoLabel->setPixmap(brandLogo().scaled(44, 44, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    auto *cardShadow = new QGraphicsDropShadowEffect(ui->loginCard);
    cardShadow->setBlurRadius(36);
    cardShadow->setOffset(0, 14);
    cardShadow->setColor(QColor(72, 53, 129, 28));
    ui->loginCard->setGraphicsEffect(cardShadow);

    // 登录时需要先连接 ChatServer，服务器再去查询 vchat_demo.users 表。
    // 默认填写本机地址，自己电脑同时运行客户端和服务器时可以直接使用。
    serverEdit->setText(QStringLiteral("127.0.0.1:8080"));
    serverEdit->setPlaceholderText(QStringLiteral("例如 127.0.0.1:8080"));
    setLoginFont(serverEdit, 14, QFont::Medium);
    ui->formLayout->addRow(QStringLiteral("服务器:"), serverEdit);
    ui->serverSettings->hide();
    auto toggleServerSettings = [this]() {
        const bool expanded = !ui->serverSettings->isVisible();
        ui->serverSettings->setVisible(expanded);
        ui->serverToggleButton->setToolTip(expanded ? QStringLiteral("收起服务器设置") : QStringLiteral("展开服务器设置"));
        ui->serverChevronButton->setArrowType(expanded ? Qt::UpArrow : Qt::DownArrow);
        ui->serverChevronButton->setToolTip(ui->serverToggleButton->toolTip());
    };
    connect(ui->serverToggleButton, &QPushButton::clicked, this, toggleServerSettings);
    connect(ui->serverChevronButton, &QToolButton::clicked, this, toggleServerSettings);
    connect(serverEdit, &QLineEdit::textChanged, this, [this](const QString &address) {
        ui->serverAddressLabel->setText(address);
    });

    QSettings settings(QStringLiteral("VChat"), QStringLiteral("VChatClient"));
    const QString savedAccount = settings.value(QStringLiteral("login/account")).toString();
    if (!savedAccount.isEmpty()) {
        ui->usernameEdit->setText(savedAccount);
        ui->rememberAccountCheck->setChecked(true);
    }
    connect(ui->eyeButton, &QToolButton::clicked, this, [this]() {
        const bool hidden = ui->passwordEdit->echoMode() == QLineEdit::Password;
        ui->passwordEdit->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
        ui->eyeButton->setIcon(svgIcon(hidden ? QStringLiteral(":/login/eye-open.svg") : QStringLiteral(":/login/eye-closed.svg"), 18));
        ui->eyeButton->setToolTip(hidden ? QStringLiteral("隐藏密码") : QStringLiteral("显示密码"));
    });

    refreshCaptcha();

    // 绑定登录界面的按钮事件。
    connect(ui->refreshCaptchaButton, &QPushButton::clicked, this, &LoginDialog::refreshCaptcha);
    connect(ui->loginButton, &QPushButton::clicked, this, &LoginDialog::tryLogin);
    connect(ui->cancelButton, &QPushButton::clicked, this, &LoginDialog::reject);
    connect(ui->passwordEdit, &QLineEdit::returnPressed, this, &LoginDialog::tryLogin);
    connect(ui->captchaEdit, &QLineEdit::returnPressed, this, &LoginDialog::tryLogin);
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

bool LoginDialog::eventFilter(QObject *watched, QEvent *event)
{
    QFrame *field = nullptr;
    if (watched == ui->usernameEdit)
        field = ui->userField;
    else if (watched == ui->passwordEdit)
        field = ui->passwordField;
    else if (watched == ui->captchaEdit)
        field = ui->captchaField;
    if (field && (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)) {
        field->setProperty("focused", event->type() == QEvent::FocusIn);
        field->style()->unpolish(field);
        field->style()->polish(field);
        field->update();
    }
    return QDialog::eventFilter(watched, event);
}

void LoginDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QLinearGradient background(0, 0, width(), height());
    background.setColorAt(0, QColor("#f5f1fd"));
    background.setColorAt(0.44, QColor("#fff6f3"));
    background.setColorAt(1, QColor("#eee9fd"));
    painter.fillRect(rect(), background);

    const qreal w = width();
    const qreal h = height();
    QRadialGradient glow(QPointF(w * 0.84, h * 0.20), w * 0.55);
    glow.setColorAt(0, QColor(255, 229, 241, 105));
    glow.setColorAt(1, QColor(255, 229, 241, 0));
    painter.fillRect(rect(), glow);
    QPainterPath backWave;
    backWave.moveTo(0, h * 0.58);
    backWave.cubicTo(w * 0.13, h * 0.68, w * 0.20, h * 0.53, w * 0.36, h * 0.57);
    backWave.cubicTo(w * 0.62, h * 0.72, w * 0.77, h * 0.43, w, h * 0.55);
    backWave.lineTo(w, h);
    backWave.lineTo(0, h);
    QLinearGradient backTint(0, h * 0.50, 0, h);
    backTint.setColorAt(0, QColor(200, 190, 246, 20));
    backTint.setColorAt(1, QColor(201, 191, 248, 76));
    painter.fillPath(backWave, backTint);

    QPainterPath frontWave;
    frontWave.moveTo(0, h * 0.79);
    frontWave.cubicTo(w * 0.20, h * 0.93, w * 0.33, h * 0.62, w * 0.54, h * 0.73);
    frontWave.cubicTo(w * 0.75, h * 0.87, w * 0.87, h * 0.66, w, h * 0.73);
    frontWave.lineTo(w, h);
    frontWave.lineTo(0, h);
    QLinearGradient frontTint(0, h * 0.65, 0, h);
    frontTint.setColorAt(0, QColor(160, 148, 231, 15));
    frontTint.setColorAt(1, QColor(162, 152, 233, 78));
    painter.fillPath(frontWave, frontTint);

    QPainterPath lowWave;
    lowWave.moveTo(0, h * 0.88);
    lowWave.cubicTo(w * 0.14, h * 0.96, w * 0.22, h * 0.73, w * 0.40, h * 0.82);
    lowWave.cubicTo(w * 0.60, h * 0.94, w * 0.76, h * 0.76, w, h * 0.84);
    lowWave.lineTo(w, h);
    lowWave.lineTo(0, h);
    QLinearGradient lowTint(0, h * 0.76, 0, h);
    lowTint.setColorAt(0, QColor(188, 172, 246, 8));
    lowTint.setColorAt(1, QColor(185, 167, 241, 64));
    painter.fillPath(lowWave, lowTint);
}

QString LoginDialog::username() const
{
    // 登录成功后优先返回数据库里的昵称，例如 张三、李四。
    // 如果服务器没有返回昵称，则退回到用户输入的账号。
    return loggedInNickname.isEmpty() ? ui->usernameEdit->text().trimmed() : loggedInNickname;
}

QString LoginDialog::serverAddress() const
{
    return serverEdit->text().trimmed();
}

void LoginDialog::refreshCaptcha()
{
    // 每次刷新或输入错误后，重新生成验证码并清空输入框。
    captchaCode = createCaptcha();
    ui->captchaCodeLabel->setText(captchaCode);
    ui->captchaEdit->clear();
}

void LoginDialog::tryLogin()
{
    if (loginInProgress)
        return;
    // 1. 读取用户输入。
    const QString name = ui->usernameEdit->text().trimmed();
    const QString password = ui->passwordEdit->text();
    const QString inputCaptcha = ui->captchaEdit->text().trimmed();

    // 2. 先做本地基本检查，账号密码不能为空。
    if (name.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("登录失败"), QStringLiteral("请输入账号和密码。"));
        return;
    }

    // 3. 验证码仍然在客户端本地判断，账号密码交给 ChatServer 查询 users 表。
    // compare 使用 CaseInsensitive，所以验证码不区分大小写。
    if (inputCaptcha.compare(captchaCode, Qt::CaseInsensitive) != 0) {
        QMessageBox::warning(this, QStringLiteral("登录失败"), QStringLiteral("验证码错误，请重新输入。"));
        refreshCaptcha();
        ui->captchaEdit->setFocus();
        return;
    }

    // 4. 验证码通过后，再连接服务器验证数据库 users 表里的账号密码。
    loginInProgress = true;
    ui->loginButton->setEnabled(false);
    ui->loginButton->setText(QStringLiteral("正在登录..."));
    ui->loginButton->repaint();
    QString errorMessage;
    const bool authenticated = authenticateWithServer(name, password, &errorMessage);
    loginInProgress = false;
    ui->loginButton->setText(QStringLiteral("登录"));
    ui->loginButton->setEnabled(true);
    if (!authenticated) {
        QMessageBox::warning(this, QStringLiteral("登录失败"), errorMessage);
        refreshCaptcha();
        ui->passwordEdit->clear();
        ui->passwordEdit->setFocus();
        return;
    }

    // 5. 全部验证成功，关闭登录窗口并进入主聊天窗口。
    QSettings settings(QStringLiteral("VChat"), QStringLiteral("VChatClient"));
    if (ui->rememberAccountCheck->isChecked()) {
        settings.setValue(QStringLiteral("login/account"), name);
    } else {
        settings.remove(QStringLiteral("login/account"));
    }
    accept();
}

QString LoginDialog::createCaptcha() const
{
    // 去掉了容易混淆的字符，例如 I、O、0、1，避免用户看错。
    static const QString chars = QStringLiteral("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
    QString code;
    code.reserve(5);

    for (int i = 0; i < 5; ++i) {
        const int index = QRandomGenerator::global()->bounded(chars.size());
        code.append(chars.at(index));
    }

    return code;
}

bool LoginDialog::authenticateWithServer(const QString &username, const QString &password, QString *errorMessage)
{
    loggedInAccountId = 0;
    loginSessionToken.clear();
    const QString address = serverEdit->text().trimmed();

    // 服务器地址格式约定为 “IP:端口”，例如 192.168.1.20:8080。
    const int separatorIndex = address.lastIndexOf(':');
    if (separatorIndex <= 0 || separatorIndex == address.size() - 1) {
        *errorMessage = QStringLiteral("服务器地址格式应为 IP:端口，例如 127.0.0.1:8080。");
        return false;
    }

    const QString host = address.left(separatorIndex).trimmed();
    bool portOk = false;
    const quint16 port = address.mid(separatorIndex + 1).trimmed().toUShort(&portOk);
    if (!portOk || port == 0) {
        *errorMessage = QStringLiteral("服务器端口不正确。");
        return false;
    }

    QTcpSocket socket;

    // 登录阶段使用一个临时 socket，只负责账号验证。
    // 验证成功进入主窗口后，主窗口会再建立正式聊天连接。
    socket.connectToHost(host, port);
    if (!socket.waitForConnected(3000)) {
        *errorMessage = QStringLiteral("连接登录服务器失败：") + socket.errorString();
        return false;
    }

    // 简单登录协议：LOGIN|username|password\n
    // ChatServer 收到后查询 vchat_demo.users 表。
    const QString request = QStringLiteral("LOGIN|%1|%2\n").arg(username, password);
    socket.write(request.toUtf8());
    if (!socket.waitForBytesWritten(3000)) {
        *errorMessage = QStringLiteral("发送登录请求失败：") + socket.errorString();
        return false;
    }

    // 服务器连接成功时可能先返回 [System] 提示，所以这里循环读取，直到读到 LOGIN_OK 或 LOGIN_FAIL。
    // LOGIN_OK 后面带的是昵称，例如 LOGIN_OK|张三。
    QString lastResponse;
    while (socket.waitForReadyRead(3000)) {
        while (socket.canReadLine()) {
            const QString response = QString::fromUtf8(socket.readLine().trimmed());
            lastResponse = response;

            if (response.startsWith(QStringLiteral("LOGIN_META|"))) {
                const QStringList metadata = response.split(QLatin1Char('|'));
                if (metadata.size() == 3) {
                    bool valid = false;
                    const qint64 id = metadata.at(1).toLongLong(&valid);
                    if (valid && id > 0) {
                        loggedInAccountId = id;
                        loginSessionToken = metadata.at(2);
                    }
                }
                continue;
            }

            if (response.startsWith(QStringLiteral("LOGIN_OK|"))) {
                loggedInNickname = response.mid(QStringLiteral("LOGIN_OK|").size()).trimmed();
                if (loggedInNickname.isEmpty()) {
                    loggedInNickname = username;
                }
                return true;
            }

            if (response.startsWith(QStringLiteral("LOGIN_FAIL|"))) {
                *errorMessage = response.mid(QStringLiteral("LOGIN_FAIL|").size()).trimmed();
                if (errorMessage->isEmpty()) {
                    *errorMessage = QStringLiteral("账号或密码错误。");
                }
                return false;
            }
        }
    }

    if (lastResponse.isEmpty()) {
        *errorMessage = QStringLiteral("服务器没有返回登录结果。");
        return false;
    }

    *errorMessage = QStringLiteral("服务器返回了无法识别的登录结果：") + lastResponse;
    return false;
}

