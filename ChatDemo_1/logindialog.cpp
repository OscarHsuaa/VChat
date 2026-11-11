#include "logindialog.h"
#include "ui_logindialog.h"

#include <QEvent>
#include <QFrame>
#include <QAction>
#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QStackedWidget>
#include <QSettings>
#include <QSvgRenderer>
#include <QStyle>
#include <QTcpSocket>
#include <QToolButton>
#include <QVBoxLayout>

#include <initializer_list>

namespace {
class ContentSizedScrollArea : public QScrollArea
{
public:
    using QScrollArea::QScrollArea;
    QSize sizeHint() const override
    {
        return widget() ? widget()->sizeHint() + QSize(2, 2) : QScrollArea::sizeHint();
    }
    QSize minimumSizeHint() const override { return QSize(240, 160); }
};

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

    QSettings settings(QStringLiteral("VChat"), QStringLiteral("VChatClient"));
    serverEdit->setText(settings.value(QStringLiteral("server/address"),
                                       QStringLiteral("127.0.0.1:8080")).toString());
    serverEdit->hide();
    auto *settingsButton = new QToolButton(this);
    settingsButton->setObjectName(QStringLiteral("loginSettingsButton"));
    settingsButton->setIcon(svgIcon(QStringLiteral(":/login/settings.svg"), 22));
    settingsButton->setIconSize(QSize(22, 22));
    settingsButton->setFixedSize(38, 38);
    settingsButton->setToolTip(QStringLiteral("设置"));
    settingsButton->setStyleSheet(QStringLiteral(
        "QToolButton { background:#f5f0ff; border:1px solid #d9cdf9; border-radius:13px; }"
        "QToolButton:hover { background:#ebe3ff; }"
        "QToolButton::menu-indicator { image:none; width:0px; }"));
    auto *settingsMenu = new QMenu(settingsButton);
    settingsMenu->setObjectName(QStringLiteral("loginSettingsMenu"));
    settingsMenu->setStyleSheet(QStringLiteral(
        "QMenu#loginSettingsMenu { background:#fffdfd; color:#292345; border:1px solid #e4daf8; "
        "border-radius:12px; padding:7px; }"
        "QMenu#loginSettingsMenu::item { color:#292345; background:transparent; "
        "padding:9px 17px 9px 12px; border-radius:7px; min-width:136px; }"
        "QMenu#loginSettingsMenu::item:selected { background:#f0eaff; color:#6041c3; }"));
    QAction *serverAction = settingsMenu->addAction(svgIcon(QStringLiteral(":/login/server.svg")),
                                                     QStringLiteral("服务器设置"));
    QAction *aboutAction = settingsMenu->addAction(svgIcon(QStringLiteral(":/login/help.svg")),
                                                    QStringLiteral("关于 VChat"));
    settingsButton->setMenu(settingsMenu);
    settingsButton->setPopupMode(QToolButton::InstantPopup);
    auto *settingsRow = new QHBoxLayout;
    settingsRow->addStretch();
    settingsRow->addWidget(settingsButton);
    ui->rootLayout->insertLayout(0, settingsRow);
    connect(serverAction, &QAction::triggered, this, [this]() {
        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("服务器设置"));
        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(24, 20, 24, 20);
        auto *label = new QLabel(QStringLiteral("服务器地址（IP 或主机名:端口）"), &dialog);
        auto *edit = new QLineEdit(serverEdit->text(), &dialog);
        edit->setMinimumWidth(310);
        edit->setMinimumHeight(44);
        auto *error = new QLabel(&dialog);
        error->setStyleSheet(QStringLiteral("color:#b34356;"));
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save, &dialog);
        buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
        buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
        layout->addWidget(label); layout->addWidget(edit); layout->addWidget(error); layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, [this, edit, error, &dialog]() {
            const QString candidate = edit->text().trimmed();
            const int colon = candidate.lastIndexOf(QLatin1Char(':'));
            bool ok = false;
            const int port = candidate.mid(colon + 1).toInt(&ok);
            if (colon <= 0 || !ok || port < 1 || port > 65535 ||
                candidate.left(colon).trimmed().isEmpty()) {
                error->setText(QStringLiteral("请输入有效的主机名或 IP 与端口。"));
                return;
            }
            QSettings settings(QStringLiteral("VChat"), QStringLiteral("VChatClient"));
            settings.setValue(QStringLiteral("server/address"), candidate);
            settings.sync();
            if (settings.status() != QSettings::NoError) {
                error->setText(QStringLiteral("服务器设置保存失败，请检查配置文件权限。"));
                return;
            }
            serverEdit->setText(candidate);
            dialog.accept();
        });
        dialog.exec();
    });
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, QStringLiteral("关于 VChat"),
                           QStringLiteral("VChat · 多人实时聊天与 AI 协作平台"));
    });

    ui->registrationInfoLabel->setText(QStringLiteral(
        "<span style=\"color:#68627f;\">还没有账号？</span> "
        "<a href=\"register\" style=\"color:#7457d8;text-decoration:none;font-weight:600;\">立即注册</a>"));
    ui->registrationInfoLabel->setTextFormat(Qt::RichText);
    ui->registrationInfoLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    ui->registrationInfoLabel->setOpenExternalLinks(false);
    connect(ui->registrationInfoLabel, &QLabel::linkActivated, this,
            [this](const QString &) { showRegistration(true); });

    ui->topSpacer->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Fixed);
    ui->rootLayout->invalidate();
    auto *cardTitle = new QLabel(QStringLiteral("用户登录"), ui->loginCard);
    cardTitle->setObjectName(QStringLiteral("loginCardTitle"));
    cardTitle->setAlignment(Qt::AlignCenter);
    cardTitle->setStyleSheet(QStringLiteral("color:#231a4b; background:transparent;"));
    setLoginFont(cardTitle, 20, QFont::Bold);
    auto *cardAccent = new QFrame(ui->loginCard);
    cardAccent->setObjectName(QStringLiteral("loginCardAccent"));
    cardAccent->setFixedSize(42, 4);
    cardAccent->setStyleSheet(QStringLiteral("background:#7957e8;border:none;border-radius:2px;"));
    ui->cardLayout->insertWidget(0, cardAccent, 0, Qt::AlignHCenter);
    ui->cardLayout->insertWidget(0, cardTitle);
    ui->cardLayout->insertSpacing(2, 5);
    ui->loginCard->setMinimumWidth(400);
    ui->loginCard->setMaximumWidth(560);
    ui->loginCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->cardLayout->setContentsMargins(36, 27, 36, 27);
    ui->cardLayout->setSpacing(13);
    for (QWidget *field : std::initializer_list<QWidget *>{ui->userField, ui->passwordField,
                          ui->captchaField, ui->captchaCodeLabel, ui->refreshCaptchaButton})
        field->setMinimumHeight(54);
    ui->loginButton->setMinimumHeight(58);

    const int cardIndex = ui->rootLayout->indexOf(ui->loginCard);
    ui->rootLayout->removeWidget(ui->loginCard);
    featurePanel = new QWidget(this);
    featurePanel->setObjectName(QStringLiteral("loginFeaturePanel"));
    featurePanel->setFixedWidth(205);
    auto *featureLayout = new QVBoxLayout(featurePanel);
    featureLayout->setContentsMargins(0, 0, 0, 0);
    featureLayout->setSpacing(27);
    const auto addFeature = [this, featureLayout](const QString &icon, const QString &title,
                                                   const QString &detail) {
        auto *row = new QWidget(featurePanel);
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(12);
        auto *iconBox = new QLabel(row);
        iconBox->setFixedSize(48, 48);
        iconBox->setAlignment(Qt::AlignCenter);
        iconBox->setPixmap(svgIcon(icon, 24).pixmap(24, 24));
        iconBox->setStyleSheet(QStringLiteral("background:#eee7fc;border-radius:14px;"));
        auto *copy = new QVBoxLayout;
        copy->setContentsMargins(0, 0, 0, 0);
        copy->setSpacing(2);
        auto *name = new QLabel(title, row);
        name->setStyleSheet(QStringLiteral("color:#241b4d;"));
        setLoginFont(name, 15, QFont::DemiBold);
        auto *description = new QLabel(detail, row);
        description->setStyleSheet(QStringLiteral("color:#69617e;"));
        setLoginFont(description, 12, QFont::Normal);
        copy->addWidget(name);
        copy->addWidget(description);
        layout->addWidget(iconBox);
        layout->addLayout(copy);
        featureLayout->addWidget(row);
    };
    addFeature(QStringLiteral(":/login/clock.svg"), QStringLiteral("即时沟通"),
               QStringLiteral("消息实时送达"));
    addFeature(QStringLiteral(":/login/members.svg"), QStringLiteral("多人协作"),
               QStringLiteral("支持群聊与管理"));
    addFeature(QStringLiteral(":/login/shield.svg"), QStringLiteral("安全可靠"),
               QStringLiteral("权限管理，稳定运行"));
    auto *contentRow = new QHBoxLayout;
    contentRow->setSpacing(20);
    contentRow->addStretch(1);
    contentRow->addWidget(featurePanel, 0, Qt::AlignVCenter);
    contentRow->addWidget(ui->loginCard, 1, Qt::AlignVCenter);
    contentRow->addStretch(1);
    ui->rootLayout->insertLayout(cardIndex, contentRow);
    featurePanel->setVisible(width() >= 850);
    ui->loginCard->setFixedWidth(qMin(560, qMax(480, width() - 345)));

    registrationDialog = new QDialog(this, Qt::Dialog | Qt::FramelessWindowHint);
    registrationDialog->setObjectName(QStringLiteral("registrationDialog"));
    registrationDialog->setWindowTitle(QStringLiteral("VChat - 注册"));
    registrationDialog->setModal(true);
    registrationDialog->setAttribute(Qt::WA_TranslucentBackground);
    auto *dialogLayout = new QVBoxLayout(registrationDialog);
    dialogLayout->setContentsMargins(22, 22, 22, 22);
    auto *registerCard = new QWidget(registrationDialog);
    registerCard->setObjectName(QStringLiteral("registerCard"));
    registerCard->setMinimumWidth(480);
    registerCard->setStyleSheet(QStringLiteral(
        "QWidget#registerCard { background:#fcfaff; border:1px solid #eee7fc; border-radius:22px; }"
        "QLineEdit { background:white; border:1px solid #ddd8ee; border-radius:11px; padding:0 14px; color:#28233a; }"
        "QLineEdit:focus { border:1px solid #a990e7; }"
        "QPushButton#registerButton { color:white; background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #8d6df0,stop:1 #7866df); border:none; border-radius:12px; }"
        "QPushButton#registerButton:hover { background:#997bf4; }"
        "QPushButton#registerButton:pressed { background:#6655c7; }"
        "QPushButton#registerButton:disabled { background:#c9bee7; }"));
    auto *shadow = new QGraphicsDropShadowEffect(registerCard);
    shadow->setBlurRadius(24); shadow->setOffset(0, 8);
    shadow->setColor(QColor(72, 53, 129, 28)); registerCard->setGraphicsEffect(shadow);
    auto *form = new QVBoxLayout(registerCard);
    form->setContentsMargins(32, 20, 32, 28); form->setSpacing(12);
    auto *registerHeader = new QHBoxLayout;
    auto *windowLabel = new QLabel(QStringLiteral("VChat - 注册"), registerCard);
    windowLabel->setStyleSheet(QStringLiteral("color:#292345;"));
    setLoginFont(windowLabel, 13, QFont::DemiBold);
    auto *closeButton = new QToolButton(registerCard);
    closeButton->setObjectName(QStringLiteral("closeRegistrationButton"));
    closeButton->setIcon(svgIcon(QStringLiteral(":/login/dialog-close.svg"), 18));
    closeButton->setIconSize(QSize(18, 18));
    closeButton->setFixedSize(30, 30);
    closeButton->setStyleSheet(QStringLiteral("QToolButton { background:transparent;border:none;border-radius:8px; }"
                                              "QToolButton:hover { background:#eee7fc; }"));
    registerHeader->addWidget(windowLabel);
    registerHeader->addStretch();
    registerHeader->addWidget(closeButton);
    form->addLayout(registerHeader);
    connect(closeButton, &QToolButton::clicked, registrationDialog, &QDialog::reject);
    auto *registerTitle = new QLabel(QStringLiteral("用户注册"), registerCard);
    registerTitle->setAlignment(Qt::AlignCenter);
    registerTitle->setStyleSheet(QStringLiteral("color:#231a4b;"));
    setLoginFont(registerTitle, 20, QFont::Bold);
    form->addSpacing(5);
    form->addWidget(registerTitle);
    auto *registerAccent = new QFrame(registerCard);
    registerAccent->setFixedSize(42, 4);
    registerAccent->setStyleSheet(QStringLiteral("background:#7957e8;border:none;border-radius:2px;"));
    form->addWidget(registerAccent, 0, Qt::AlignHCenter);
    form->addSpacing(12);
    auto makeEdit = [registerCard, form](const QString &placeholder, const QString &icon) {
        auto *edit = new QLineEdit(registerCard);
        edit->setMinimumHeight(52);
        edit->setPlaceholderText(placeholder);
        edit->addAction(svgIcon(icon), QLineEdit::LeadingPosition);
        setLoginFont(edit, 14, QFont::Medium);
        QPalette palette = edit->palette();
        palette.setColor(QPalette::PlaceholderText, QColor("#77718c"));
        edit->setPalette(palette);
        form->addWidget(edit);
        return edit;
    };
    registerNameEdit = makeEdit(QStringLiteral("用户名（4–20 位，支持字母、数字和下划线）"),
                                QStringLiteral(":/login/user.svg"));
    registerNameEdit->setObjectName(QStringLiteral("registerNameEdit"));
    registerPasswordEdit = makeEdit(QStringLiteral("设置密码（6–32 位）"), QStringLiteral(":/login/lock.svg"));
    registerPasswordEdit->setObjectName(QStringLiteral("registerPasswordEdit"));
    registerConfirmEdit = makeEdit(QStringLiteral("确认密码"), QStringLiteral(":/login/lock.svg"));
    registerConfirmEdit->setObjectName(QStringLiteral("registerConfirmEdit"));
    for (QLineEdit *edit : {registerPasswordEdit, registerConfirmEdit}) {
        edit->setEchoMode(QLineEdit::Password);
        QAction *eye = edit->addAction(svgIcon(QStringLiteral(":/login/eye-closed.svg")), QLineEdit::TrailingPosition);
        connect(eye, &QAction::triggered, this, [edit, eye]() {
            const bool hidden = edit->echoMode() == QLineEdit::Password;
            edit->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
            eye->setIcon(svgIcon(hidden ? QStringLiteral(":/login/eye-open.svg")
                                         : QStringLiteral(":/login/eye-closed.svg")));
        });
    }
    auto *captchaRow = new QHBoxLayout;
    captchaRow->setSpacing(9);
    registerCaptchaEdit = new QLineEdit(registerCard);
    registerCaptchaEdit->setObjectName(QStringLiteral("registerCaptchaEdit"));
    registerCaptchaEdit->setMinimumHeight(52);
    registerCaptchaEdit->setPlaceholderText(QStringLiteral("验证码"));
    registerCaptchaEdit->addAction(svgIcon(QStringLiteral(":/login/shield.svg")), QLineEdit::LeadingPosition);
    setLoginFont(registerCaptchaEdit, 14, QFont::Medium);
    registerCaptchaLabel = new QLabel(registerCard);
    registerCaptchaLabel->setObjectName(QStringLiteral("registerCaptchaLabel"));
    registerCaptchaLabel->setFixedSize(102, 52);
    registerCaptchaLabel->setAlignment(Qt::AlignCenter);
    registerCaptchaLabel->setStyleSheet(QStringLiteral("background:#eeeafa;border-radius:9px;color:#28233a;font:bold 17px Consolas;"));
    auto *refresh = new QPushButton(registerCard);
    refresh->setIcon(svgIcon(QStringLiteral(":/login/refresh.svg"), 18));
    refresh->setIconSize(QSize(18, 18)); refresh->setFixedSize(52, 52);
    refresh->setToolTip(QStringLiteral("刷新验证码"));
    refresh->setStyleSheet(QStringLiteral("background:white;border:1px solid #ddd8ee;border-radius:11px;"));
    captchaRow->addWidget(registerCaptchaEdit, 1);
    captchaRow->addWidget(registerCaptchaLabel); captchaRow->addWidget(refresh);
    form->addLayout(captchaRow);
    connect(refresh, &QPushButton::clicked, this, &LoginDialog::refreshRegisterCaptcha);
    registerErrorLabel = new QLabel(registerCard);
    registerErrorLabel->setObjectName(QStringLiteral("registerErrorLabel"));
    registerErrorLabel->setStyleSheet(QStringLiteral("color:#b34356;"));
    registerErrorLabel->setWordWrap(true);
    registerErrorLabel->hide();
    form->addWidget(registerErrorLabel);
    form->addSpacing(10);
    registerButton = new QPushButton(QStringLiteral("注册"), registerCard);
    registerButton->setObjectName(QStringLiteral("registerButton"));
    registerButton->setMinimumHeight(56);
    setLoginFont(registerButton, 16, QFont::DemiBold);
    form->addWidget(registerButton);
    connect(registerButton, &QPushButton::clicked, this, &LoginDialog::tryRegister);
    auto *back = new QLabel(QStringLiteral(
        "<span style=\"color:#68627f;\">已有账号？</span> "
        "<a href=\"login\" style=\"color:#7457d8;text-decoration:none;font-weight:600;\">返回登录</a>"), registerCard);
    back->setObjectName(QStringLiteral("returnLoginLink"));
    back->setAlignment(Qt::AlignCenter);
    back->setTextInteractionFlags(Qt::TextBrowserInteraction);
    back->setOpenExternalLinks(false);
    back->setStyleSheet(QStringLiteral("color:#68627f;"));
    setLoginFont(back, 13, QFont::Normal);
    form->addSpacing(7); form->addWidget(back);
    connect(back, &QLabel::linkActivated, this, [this](const QString &) { showRegistration(false); });
    auto *registrationScroll = new ContentSizedScrollArea(registrationDialog);
    registrationScroll->setObjectName(QStringLiteral("registrationScroll"));
    registrationScroll->setWidgetResizable(true);
    registrationScroll->setFrameShape(QFrame::NoFrame);
    registrationScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    registrationScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    registrationScroll->setStyleSheet(QStringLiteral("QScrollArea#registrationScroll { background:transparent; border:none; }"));
    registrationScroll->setWidget(registerCard);
    dialogLayout->addWidget(registrationScroll);
    refreshRegisterCaptcha();
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
    connect(ui->passwordEdit, &QLineEdit::returnPressed, this, &LoginDialog::tryLogin);
    connect(ui->captchaEdit, &QLineEdit::returnPressed, this, &LoginDialog::tryLogin);
    buildCompactLoginUi();
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::buildCompactLoginUi()
{
    // Keep the existing form widgets and their authentication signal connections.
    auto *formCard = ui->loginCard;
    auto *menuButton = findChild<QToolButton *>(QStringLiteral("loginSettingsButton"));
    ui->logoLabel->hide();
    ui->titleLabel->hide();
    ui->subtitleLabel->hide();
    featurePanel->hide();
    delete ui->rootLayout;

    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    const QRect desktop = (screen() ? screen() : QGuiApplication::primaryScreen())->availableGeometry();
    setMinimumSize(0, 0);
    setFixedSize(qMin(560, desktop.width() - 16), qMin(580, desktop.height() - 16));
    move(desktop.center() - rect().center());
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(2, 2, 2, 2);
    root->setSpacing(0);

    dragBar = new QWidget(this);
    dragBar->setObjectName(QStringLiteral("loginTitleBar"));
    dragBar->setFixedHeight(56);
    dragBar->setStyleSheet(QStringLiteral("QWidget#loginTitleBar { background:#eee8fc; border-top-left-radius:20px; border-top-right-radius:20px; }"));
    auto *titleRow = new QHBoxLayout(dragBar);
    titleRow->setContentsMargins(22, 0, 16, 0);
    titleRow->setSpacing(9);
    auto *logo = new QLabel(dragBar);
    logo->setPixmap(brandLogo().scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setFixedSize(30, 30);
    auto *brand = new QLabel(QStringLiteral("VChat"), dragBar);
    brand->setStyleSheet(QStringLiteral("color:#241c4c;background:transparent;"));
    setLoginFont(brand, 20, QFont::Bold);
    titleRow->addWidget(logo);
    titleRow->addWidget(brand);
    titleRow->addStretch();
    QPixmap bars(24, 24); bars.fill(Qt::transparent);
    { QPainter p(&bars); p.setRenderHint(QPainter::Antialiasing);
      p.setPen(QPen(QColor("#66539d"), 2.3, Qt::SolidLine, Qt::RoundCap));
      for (int y : {6, 12, 18}) p.drawLine(3, y, y == 12 ? 17 : 21, y); }
    menuButton->setParent(dragBar);
    menuButton->setObjectName(QStringLiteral("loginMenuButton"));
    menuButton->setIcon(QIcon(bars));
    menuButton->setIconSize(QSize(24, 24));
    menuButton->setToolTip(QStringLiteral("菜单"));
    menuButton->setStyleSheet(QStringLiteral("QToolButton {background:transparent;border:none;border-radius:9px;} QToolButton:hover {background:#e1d6f8;} QToolButton::menu-indicator {image:none;width:0;}"));
    menuButton->menu()->addAction(svgIcon(QStringLiteral(":/login/room.svg")),
                                  QStringLiteral("问题反馈"), this, [this]() {
        QMessageBox::information(this, QStringLiteral("问题反馈"),
                                 QStringLiteral("问题反馈功能正在开发中，敬请期待。"));
    });
    auto *close = new QToolButton(dragBar);
    close->setObjectName(QStringLiteral("loginCloseButton"));
    close->setIcon(svgIcon(QStringLiteral(":/login/dialog-close.svg"), 20));
    close->setIconSize(QSize(20, 20)); close->setFixedSize(34, 34);
    close->setStyleSheet(QStringLiteral("QToolButton {background:transparent;border:none;border-radius:9px;} QToolButton:hover {background:#e1d6f8;}"));
    connect(close, &QToolButton::clicked, this, &QDialog::reject);
    titleRow->addWidget(menuButton);
    titleRow->addWidget(close);
    for (QWidget *part : std::initializer_list<QWidget *>{dragBar, logo, brand})
        part->installEventFilter(this);
    root->addWidget(dragBar);

    loginPages = new QStackedWidget(this);
    loginPages->setObjectName(QStringLiteral("loginPages"));
    root->addWidget(loginPages, 1);
    auto *savedPage = new QWidget(loginPages);
    savedPage->setObjectName(QStringLiteral("savedAccountPage"));
    auto *savedLayout = new QVBoxLayout(savedPage);
    savedLayout->setContentsMargins(70, 12, 70, 25);
    savedLayout->setSpacing(11);
    savedLayout->addStretch(1);
    auto *avatar = new QLabel(savedPage);
    avatar->setObjectName(QStringLiteral("savedAvatar"));
    avatar->setFixedSize(112, 112);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setPixmap(svgIcon(QStringLiteral(":/login/user.svg"), 48).pixmap(48, 48));
    avatar->setStyleSheet(QStringLiteral("background:#e9dfff;border:1px solid #d7c5fa;border-radius:56px;"));
    savedLayout->addWidget(avatar, 0, Qt::AlignHCenter);
    savedNameLabel = new QLabel(savedPage);
    savedNameLabel->setObjectName(QStringLiteral("savedAccountName"));
    savedNameLabel->setAlignment(Qt::AlignCenter);
    savedNameLabel->setStyleSheet(QStringLiteral("color:#241b4c;"));
    setLoginFont(savedNameLabel, 26, QFont::Bold);
    savedLayout->addWidget(savedNameLabel);
    savedHintLabel = new QLabel(QStringLiteral("已保存的账号"), savedPage);
    savedHintLabel->setAlignment(Qt::AlignCenter);
    savedHintLabel->setStyleSheet(QStringLiteral("color:#766995;"));
    setLoginFont(savedHintLabel, 16, QFont::Normal);
    savedLayout->addWidget(savedHintLabel);
    savedLayout->addSpacing(4);
    savedRememberCheck = new QCheckBox(QStringLiteral("记住账号"), savedPage);
    savedRememberCheck->setObjectName(QStringLiteral("savedRememberCheck"));
    savedRememberCheck->setChecked(true);
    setLoginFont(savedRememberCheck, 15, QFont::Medium);
    savedLayout->addWidget(savedRememberCheck, 0, Qt::AlignHCenter);
    auto *savedLogin = new QPushButton(QStringLiteral("登录"), savedPage);
    savedLogin->setObjectName(QStringLiteral("savedAccountLoginButton"));
    savedLogin->setFixedHeight(52);
    savedLogin->setStyleSheet(ui->loginButton->styleSheet() + QStringLiteral(
        "QPushButton#savedAccountLoginButton {color:white;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #a577ef,stop:1 #7457d8);border:none;border-radius:13px;font-size:18px;font-weight:600;}"
        "QPushButton#savedAccountLoginButton:hover {background:#9472ef;}"
        "QPushButton#savedAccountLoginButton:pressed {background:#684cc5;}"));
    savedLayout->addWidget(savedLogin);
    connect(savedLogin, &QPushButton::clicked, this, [this]() {
        ui->usernameEdit->setText(selectedSavedAccount);
        ui->rememberAccountCheck->setChecked(savedRememberCheck->isChecked());
        showAccountPage(false);
        ui->passwordEdit->setFocus();
    });
    auto *links = new QHBoxLayout;
    links->setSpacing(10);
    auto *other = new QPushButton(QStringLiteral("使用其他账号"), savedPage);
    auto *manage = new QPushButton(QStringLiteral("账号管理"), savedPage);
    for (auto *button : {other, manage}) {
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(QStringLiteral("QPushButton {color:#6844d7;background:transparent;border:none;font-size:15px;} QPushButton:hover {text-decoration:underline;}"));
    }
    links->addStretch(); links->addWidget(other);
    auto *separator = new QLabel(QStringLiteral("│"), savedPage);
    separator->setStyleSheet(QStringLiteral("color:#c5b4e7;"));
    links->addWidget(separator); links->addWidget(manage); links->addStretch();
    savedLayout->addLayout(links);
    savedLayout->addStretch(1);
    connect(other, &QPushButton::clicked, this, [this]() { showAccountPage(false); });
    connect(manage, &QPushButton::clicked, this, &LoginDialog::manageAccounts);
    loginPages->addWidget(savedPage);

    auto *otherPage = new QWidget(loginPages);
    otherPage->setObjectName(QStringLiteral("otherAccountPage"));
    auto *otherLayout = new QVBoxLayout(otherPage);
    otherLayout->setContentsMargins(38, 10, 38, 22);
    otherLayout->setSpacing(0);
    auto *back = new QPushButton(QStringLiteral("←  返回账号选择"), otherPage);
    back->setObjectName(QStringLiteral("backToAccountsButton"));
    back->setCursor(Qt::PointingHandCursor);
    back->setStyleSheet(QStringLiteral("QPushButton {color:#36275e;background:transparent;border:none;text-align:left;font-size:15px;} QPushButton:hover {color:#7655db;}"));
    otherLayout->addWidget(back, 0, Qt::AlignLeft);
    connect(back, &QPushButton::clicked, this, [this]() { showAccountPage(true); });
    otherLayout->addStretch(1);
    formCard->setParent(otherPage);
    formCard->setGraphicsEffect(nullptr);
    formCard->setStyleSheet(QStringLiteral("QWidget#loginCard {background:transparent;border:none;}"));
    formCard->setMinimumWidth(0);
    formCard->setFixedWidth(qMin(460, width() - 60));
    ui->cardLayout->setContentsMargins(6, 4, 6, 4);
    ui->cardLayout->setSpacing(9);
    auto *compactTitle = findChild<QLabel *>(QStringLiteral("loginCardTitle"));
    compactTitle->setText(QStringLiteral("使用其他账号登录"));
    setLoginFont(compactTitle, 22, QFont::Bold);
    for (QLineEdit *edit : {ui->usernameEdit, ui->passwordEdit, ui->captchaEdit})
        setLoginFont(edit, 15, QFont::Normal);
    setLoginFont(ui->rememberAccountCheck, 14, QFont::Medium);
    setLoginFont(ui->registrationInfoLabel, 14, QFont::Normal);
    setLoginFont(ui->loginButton, 18, QFont::DemiBold);
    ui->loginButton->setStyleSheet(QStringLiteral(
        "QPushButton#loginButton {color:white;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #a577ef,stop:1 #7457d8);border:none;border-radius:13px;}"
        "QPushButton#loginButton:hover {background:#9472ef;}"
        "QPushButton#loginButton:pressed {background:#684cc5;}"
        "QPushButton#loginButton:disabled {background:#c9bee7;color:#f7f4fc;}"));
    ui->userField->setFixedHeight(48);
    ui->passwordField->setFixedHeight(48);
    ui->captchaField->setFixedHeight(48);
    ui->captchaCodeLabel->setFixedHeight(48);
    ui->refreshCaptchaButton->setFixedSize(48, 48);
    ui->loginButton->setFixedHeight(52);
    auto *autoLogin = new QCheckBox(QStringLiteral("自动登录"), formCard);
    autoLogin->setObjectName(QStringLiteral("autoLoginCheck"));
    autoLogin->setEnabled(false);
    setLoginFont(autoLogin, 14, QFont::Medium);
    autoLogin->setToolTip(QStringLiteral("自动登录需要服务端安全凭证，当前版本尚未开放。"));
    autoLogin->setAttribute(Qt::WA_AlwaysShowToolTips);
    ui->optionsRow->addWidget(autoLogin);
    const QString checkStyle = QStringLiteral(
        "QCheckBox {color:#30284f;spacing:9px;padding:3px 0;}"
        "QCheckBox::indicator {width:18px;height:18px;border:1px solid #cbbbea;"
        "border-radius:4px;background:#ffffff;}"
        "QCheckBox::indicator:hover {border-color:#8064d9;background:#f7f3ff;}"
        "QCheckBox::indicator:checked {background:#8064d9;border-color:#8064d9;"
        "image:url(:/login/check.svg);}"
        "QCheckBox:disabled {color:#706685;}"
        "QCheckBox::indicator:disabled {background:#f6f3fb;border-color:#d6cde9;}");
    for (QCheckBox *check : {savedRememberCheck, ui->rememberAccountCheck, autoLogin})
        check->setStyleSheet(checkStyle);
    otherLayout->addWidget(formCard, 0, Qt::AlignHCenter);
    otherLayout->addStretch(2);
    loginPages->addWidget(otherPage);
    refreshSavedAccount();
}

void LoginDialog::refreshSavedAccount()
{
    QSettings settings(QStringLiteral("VChat"), QStringLiteral("VChatClient"));
    QStringList accounts = settings.value(QStringLiteral("login/accounts")).toStringList();
    const QString legacy = settings.value(QStringLiteral("login/account")).toString();
    if (!legacy.isEmpty() && !accounts.contains(legacy)) accounts.prepend(legacy);
    selectedSavedAccount = settings.value(QStringLiteral("login/lastAccount")).toString();
    if (!accounts.contains(selectedSavedAccount)) selectedSavedAccount = accounts.value(0);
    if (selectedSavedAccount.isEmpty()) {
        loginPages->setCurrentIndex(1);
        return;
    }
    const QVariantMap names = settings.value(QStringLiteral("login/displayNames")).toMap();
    savedNameLabel->setText(names.value(selectedSavedAccount, selectedSavedAccount).toString());
    savedNameLabel->setToolTip(selectedSavedAccount);
    ui->usernameEdit->setText(selectedSavedAccount);
    loginPages->setCurrentIndex(0);
}

void LoginDialog::showAccountPage(bool saved)
{
    if (saved) refreshSavedAccount();
    else loginPages->setCurrentIndex(1);
    if (!saved) ui->passwordEdit->clear();
}

void LoginDialog::manageAccounts()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("账号管理"));
    dialog.setMinimumWidth(360);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 20, 24, 20);
    auto *caption = new QLabel(QStringLiteral("本地保存的账号"), &dialog);
    caption->setStyleSheet(QStringLiteral("color:#292345;font-size:17px;font-weight:600;"));
    layout->addWidget(caption);
    auto *list = new QListWidget(&dialog);
    list->setStyleSheet(QStringLiteral("QListWidget {background:white;color:#292345;border:1px solid #ded5f4;border-radius:10px;} QListWidget::item {padding:10px;} QListWidget::item:selected {background:#efe8fc;color:#3c286c;}"));
    layout->addWidget(list);
    QSettings settings(QStringLiteral("VChat"), QStringLiteral("VChatClient"));
    QStringList accounts = settings.value(QStringLiteral("login/accounts")).toStringList();
    const QString legacy = settings.value(QStringLiteral("login/account")).toString();
    if (!legacy.isEmpty() && !accounts.contains(legacy)) accounts.prepend(legacy);
    auto reload = [&]() {
        list->clear();
        const QVariantMap names = settings.value(QStringLiteral("login/displayNames")).toMap();
        for (const QString &account : accounts) {
            auto *item = new QListWidgetItem(names.value(account, account).toString(), list);
            item->setData(Qt::UserRole, account);
        }
        if (list->count()) list->setCurrentRow(0);
    };
    reload();
    auto *buttons = new QHBoxLayout;
    auto *choose = new QPushButton(QStringLiteral("使用此账号"), &dialog);
    auto *remove = new QPushButton(QStringLiteral("移除本地账号"), &dialog);
    auto *done = new QPushButton(QStringLiteral("关闭"), &dialog);
    buttons->addWidget(remove); buttons->addStretch(); buttons->addWidget(choose); buttons->addWidget(done);
    layout->addLayout(buttons);
    connect(choose, &QPushButton::clicked, &dialog, [&]() {
        if (!list->currentItem()) return;
        settings.setValue(QStringLiteral("login/lastAccount"), list->currentItem()->data(Qt::UserRole));
        dialog.accept();
    });
    connect(remove, &QPushButton::clicked, &dialog, [&]() {
        if (!list->currentItem()) return;
        const QString account = list->currentItem()->data(Qt::UserRole).toString();
        accounts.removeAll(account);
        settings.setValue(QStringLiteral("login/accounts"), accounts);
        if (settings.value(QStringLiteral("login/account")).toString() == account)
            settings.remove(QStringLiteral("login/account"));
        if (settings.value(QStringLiteral("login/lastAccount")).toString() == account)
            settings.remove(QStringLiteral("login/lastAccount"));
        QVariantMap names = settings.value(QStringLiteral("login/displayNames")).toMap();
        names.remove(account);
        settings.setValue(QStringLiteral("login/displayNames"), names);
        reload();
    });
    connect(done, &QPushButton::clicked, &dialog, &QDialog::reject);
    dialog.exec();
    refreshSavedAccount();
}

bool LoginDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == dragBar || (dragBar && watched->isWidgetType() &&
                               static_cast<QWidget *>(watched)->parentWidget() == dragBar)) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                dragOffset = mouse->globalPosition().toPoint() - frameGeometry().topLeft();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->buttons() & Qt::LeftButton) {
                move(mouse->globalPosition().toPoint() - dragOffset);
                return true;
            }
        }
    }
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

void LoginDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
}

void LoginDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath outline;
    outline.addRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 20, 20);
    painter.setClipPath(outline);
    QLinearGradient background(0, 0, width(), height());
    background.setColorAt(0, QColor("#f7f3ff"));
    background.setColorAt(0.55, QColor("#fffafe"));
    background.setColorAt(1, QColor("#f5efff"));
    painter.fillRect(rect(), background);

    const qreal w = width();
    const qreal h = height();
    QRadialGradient glow(QPointF(w * 0.84, h * 0.16), w * 0.52);
    glow.setColorAt(0, QColor(241, 226, 255, 65));
    glow.setColorAt(1, QColor(241, 226, 255, 0));
    painter.fillRect(rect(), glow);
    QPainterPath backWave;
    backWave.moveTo(0, h * 0.89);
    backWave.cubicTo(w * 0.16, h * 0.93, w * 0.25, h * 0.97, w * 0.42, h * 0.93);
    backWave.cubicTo(w * 0.67, h * 0.86, w * 0.82, h * 0.85, w, h * 0.91);
    backWave.lineTo(w, h);
    backWave.lineTo(0, h);
    QLinearGradient backTint(0, h * 0.86, 0, h);
    backTint.setColorAt(0, QColor(200, 190, 246, 24));
    backTint.setColorAt(1, QColor(201, 191, 248, 72));
    painter.fillPath(backWave, backTint);

    QPainterPath frontWave;
    frontWave.moveTo(0, h * 0.96);
    frontWave.cubicTo(w * 0.21, h * 0.90, w * 0.31, h * 0.99, w * 0.52, h * 0.96);
    frontWave.cubicTo(w * 0.75, h * 0.93, w * 0.86, h * 0.95, w, h * 0.93);
    frontWave.lineTo(w, h);
    frontWave.lineTo(0, h);
    QLinearGradient frontTint(0, h * 0.91, 0, h);
    frontTint.setColorAt(0, QColor(160, 148, 231, 12));
    frontTint.setColorAt(1, QColor(162, 152, 233, 72));
    painter.fillPath(frontWave, frontTint);
    painter.setClipping(false);
    painter.setPen(QPen(QColor("#d5c4fa"), 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(outline);
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

void LoginDialog::showRegistration(bool registration)
{
    if (registrationInProgress || loginInProgress) return;
    if (registration) {
        registerErrorLabel->hide();
        const QRect available = screen() ? screen()->availableGeometry()
                                         : QGuiApplication::primaryScreen()->availableGeometry();
        auto *card = registrationDialog->findChild<QWidget *>(QStringLiteral("registerCard"));
        card->layout()->activate();
        const QSize desired(qMax(480, card->sizeHint().width()) + 46,
                            qMax(640, card->sizeHint().height() + 104));
        const QSize dialogSize(qMin(desired.width(), available.width()),
                               qMin(desired.height(), available.height()));
        auto *scroll = registrationDialog->findChild<QScrollArea *>(QStringLiteral("registrationScroll"));
        const bool needsScroll = desired.height() > available.height();
        scroll->setVerticalScrollBarPolicy(needsScroll ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
        card->setMinimumWidth(qMin(480, qMax(0, dialogSize.width() - 46 - (needsScroll ? 16 : 0))));
        registrationDialog->resize(dialogSize);
        const QPoint centered = mapToGlobal(rect().center())
            - QPoint(dialogSize.width() / 2, dialogSize.height() / 2);
        registrationDialog->move(qBound(available.left(), centered.x(),
                                         qMax(available.left(), available.right() - dialogSize.width() + 1)),
                                 qBound(available.top(), centered.y(),
                                         qMax(available.top(), available.bottom() - dialogSize.height() + 1)));
        registerNameEdit->setFocus();
        registrationDialog->exec();
    } else {
        registerPasswordEdit->clear();
        registerConfirmEdit->clear();
        registrationDialog->reject();
        ui->usernameEdit->setFocus();
    }
}

void LoginDialog::refreshRegisterCaptcha()
{
    registerCaptchaCode = createCaptcha();
    registerCaptchaLabel->setText(registerCaptchaCode);
    registerCaptchaEdit->clear();
}

bool LoginDialog::parseServerAddress(QString *host, quint16 *port, QString *errorMessage) const
{
    const QString address = serverEdit->text().trimmed();
    const int separatorIndex = address.lastIndexOf(QLatin1Char(':'));
    bool ok = false;
    const int parsedPort = address.mid(separatorIndex + 1).toInt(&ok);
    if (separatorIndex <= 0 || address.left(separatorIndex).trimmed().isEmpty() ||
        !ok || parsedPort < 1 || parsedPort > 65535) {
        *errorMessage = QStringLiteral("服务器地址格式应为 IP:端口，例如 127.0.0.1:8080。");
        return false;
    }
    *host = address.left(separatorIndex).trimmed();
    *port = quint16(parsedPort);
    return true;
}

void LoginDialog::tryRegister()
{
    if (registrationInProgress) return;
    const QString name = registerNameEdit->text().trimmed();
    const QString password = registerPasswordEdit->text();
    const QString confirm = registerConfirmEdit->text();
    auto fail = [this](const QString &message) {
        registerErrorLabel->setText(message);
        registerErrorLabel->show();
    };
    static const QRegularExpression validName(QStringLiteral("^[A-Za-z0-9_]{4,20}$"));
    if (!validName.match(name).hasMatch()) {
        fail(QStringLiteral("用户名须为 4–20 位字母、数字或下划线。")); return;
    }
    if (password.size() < 6 || password.size() > 32 || password.contains(QLatin1Char('|')) ||
        password.contains(QLatin1Char('\n')) || password.contains(QLatin1Char('\r'))) {
        fail(QStringLiteral("密码须为 6–32 位，不能包含 | 或换行。")); return;
    }
    if (password != confirm) {
        fail(QStringLiteral("两次输入的密码不一致。")); registerConfirmEdit->setFocus(); return;
    }
    if (registerCaptchaEdit->text().trimmed().compare(registerCaptchaCode, Qt::CaseInsensitive) != 0) {
        fail(QStringLiteral("验证码错误，请重新输入。"));
        refreshRegisterCaptcha(); return;
    }
    registrationInProgress = true;
    registerButton->setEnabled(false);
    registerButton->setText(QStringLiteral("正在注册..."));
    registerButton->repaint();
    QString error;
    const bool registered = registerWithServer(name, password, &error);
    registrationInProgress = false;
    registerButton->setEnabled(true);
    registerButton->setText(QStringLiteral("注册"));
    if (!registered) {
        fail(error);
        refreshRegisterCaptcha();
        return;
    }
    registerErrorLabel->hide();
    registerPasswordEdit->clear();
    registerConfirmEdit->clear();
    ui->usernameEdit->setText(name);
    ui->passwordEdit->clear();
    refreshCaptcha();
    showRegistration(false);
    showAccountPage(false);
    QMessageBox::information(this, QStringLiteral("注册成功"), QStringLiteral("注册成功，请登录。"));
}

bool LoginDialog::registerWithServer(const QString &username, const QString &password, QString *errorMessage)
{
    QString host;
    quint16 port = 0;
    if (!parseServerAddress(&host, &port, errorMessage)) return false;
    QTcpSocket socket;
    socket.connectToHost(host, port);
    if (!socket.waitForConnected(3000)) {
        *errorMessage = QStringLiteral("无法连接服务器：") + socket.errorString();
        return false;
    }
    const QJsonObject data{{QStringLiteral("username"), username},
                           {QStringLiteral("password"), password}};
    const QByteArray request = "REGISTER|" +
        QJsonDocument(data).toJson(QJsonDocument::Compact).toBase64() + '\n';
    if (socket.write(request) < 0 || !socket.waitForBytesWritten(3000)) {
        *errorMessage = QStringLiteral("注册请求发送失败。"); return false;
    }
    while (socket.waitForReadyRead(3000)) {
        while (socket.canReadLine()) {
            const QString response = QString::fromUtf8(socket.readLine().trimmed());
            if (response == QStringLiteral("REGISTER_OK")) return true;
            if (response.startsWith(QStringLiteral("REGISTER_FAIL|"))) {
                *errorMessage = response.mid(14);
                return false;
            }
        }
    }
    *errorMessage = QStringLiteral("服务器未返回注册结果，请稍后再试。");
    return false;
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
        QStringList accounts = settings.value(QStringLiteral("login/accounts")).toStringList();
        accounts.removeAll(name);
        accounts.prepend(name);
        settings.setValue(QStringLiteral("login/accounts"), accounts);
        settings.setValue(QStringLiteral("login/lastAccount"), name);
        QVariantMap names = settings.value(QStringLiteral("login/displayNames")).toMap();
        names.insert(name, loggedInNickname.isEmpty() ? name : loggedInNickname);
        settings.setValue(QStringLiteral("login/displayNames"), names);
    } else {
        settings.remove(QStringLiteral("login/account"));
        QStringList accounts = settings.value(QStringLiteral("login/accounts")).toStringList();
        accounts.removeAll(name);
        settings.setValue(QStringLiteral("login/accounts"), accounts);
        if (settings.value(QStringLiteral("login/lastAccount")).toString() == name)
            settings.remove(QStringLiteral("login/lastAccount"));
        QVariantMap names = settings.value(QStringLiteral("login/displayNames")).toMap();
        names.remove(name);
        settings.setValue(QStringLiteral("login/displayNames"), names);
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
    QString host;
    quint16 port = 0;
    if (!parseServerAddress(&host, &port, errorMessage)) return false;

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

