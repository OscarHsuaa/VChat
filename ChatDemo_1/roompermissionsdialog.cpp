#include "roompermissionsdialog.h"
#include "roomdialogchrome.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QStackedWidget>
#include <QStyle>
#include <QScreen>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {
const QString kStyle = QStringLiteral(
    "QDialog { background:transparent; color:#292342; font-family:'Microsoft YaHei UI'; font-size:13px; }"
    "QFrame#roomPanel { background:#fdfbff; border:1px solid #e8def6; border-radius:19px; }"
    "QFrame#permissionsBody { background:#fff; border:1px solid #ebe4f8; border-radius:15px; }"
    "QWidget#permissionsPage { background:#fff; }"
    "QFrame#navigationPanel { background:#faf8ff; border:0; border-right:1px solid #ebe4f8; }"
    "QFrame#choiceCard { background:#fff; border:1px solid #e8e0f5; border-radius:14px; }"
    "QLabel { color:#544b6b; background:transparent; }"
    "QRadioButton,QCheckBox { color:#30264f; background:transparent; spacing:10px; padding:2px; }"
    "QRadioButton::indicator,QCheckBox::indicator { width:17px; height:17px; }"
    "QRadioButton::indicator:checked { image:url(:/login/radio-selected.svg); }"
    "QRadioButton::indicator:unchecked { image:url(:/login/radio-empty.svg); }"
    "QCheckBox::indicator { border:1px solid #b9acd6; border-radius:4px; background:white; }"
    "QCheckBox::indicator:checked { background:#8064d9; border:1px solid #8064d9; image:url(:/login/check.svg); }"
    "QPushButton { min-width:82px; min-height:39px; border-radius:10px; background:#f1ecfa; color:#554676; border:1px solid #e2d8f5; }"
    "QPushButton:hover { background:#e9e0f9; } QPushButton:pressed { background:#ded0f4; }"
    "QPushButton#primary { background:#8064d9; color:white; border:0; font-weight:600; }"
    "QPushButton#primary:hover { background:#9275e6; }"
    "QPushButton#primary:disabled { background:#c6b9e3; color:#f6f3fc; }"
    "QPushButton#permissionsNav { min-width:0; min-height:64px; max-height:64px; border:1px solid transparent; border-radius:12px; background:transparent; padding:0; }"
    "QPushButton#permissionsNav:hover { background:#f0eafd; }"
    "QPushButton#permissionsNav:pressed { background:#e5d9fb; }"
    "QPushButton#permissionsNav[selected='true'] { background:#eee7ff; border:1px solid #c7b4ff; color:#30206e; }"
    "QLabel#permissionsNavText { color:#30264f; font-size:16px; font-weight:600; background:transparent; }"
    "QToolButton#dialogClose { background:transparent; border:0; border-radius:8px; }"
    "QToolButton#dialogClose:hover { background:#f1ecfc; }"
    "QScrollArea { background:transparent; border:0; }"
    "QScrollBar:vertical { width:7px; background:transparent; }"
    "QScrollBar::handle:vertical { background:#d4c6ee; border-radius:3px; }");

QWidget *pageContents(QWidget *parent, QVBoxLayout **layout, const QString &title,
                      const QString &description)
{
    auto *page = new QWidget(parent);
    page->setObjectName(QStringLiteral("permissionsPage"));
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(22, 24, 22, 18);
    root->setSpacing(14);
    auto *label = new QLabel(title, page);
    label->setStyleSheet(QStringLiteral("font-size:18px; font-weight:700; color:#252044;"));
    root->addWidget(label);
    if (!description.isEmpty()) {
        auto *subtitle = new QLabel(description, page);
        subtitle->setWordWrap(true);
        subtitle->setStyleSheet(QStringLiteral("color:#827799; font-size:12px;"));
        root->addWidget(subtitle);
        root->addSpacing(4);
    }
    *layout = root;
    return page;
}

QScrollArea *scrollPage(QWidget *parent, QWidget *page)
{
    auto *scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    scroll->setMinimumWidth(0);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    page->setMinimumWidth(0);
    scroll->viewport()->setAutoFillBackground(true);
    scroll->viewport()->setPalette(QPalette(Qt::white));
    scroll->setWidget(page);
    return scroll;
}

class ChoiceCard final : public QFrame {
public:
    ChoiceCard(QWidget *parent, QRadioButton *button) : QFrame(parent), button_(button) {}
protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && button_->isEnabled()) button_->click();
        QFrame::mousePressEvent(event);
    }
private:
    QRadioButton *button_;
};

QFrame *radioCard(QWidget *parent, QRadioButton *button, const QString &iconName,
                  const QString &description)
{
    auto *card = new ChoiceCard(parent, button);
    card->setObjectName(QStringLiteral("choiceCard"));
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *row = new QHBoxLayout(card);
    row->setContentsMargins(18, 16, 18, 16);
    row->setSpacing(14);
    row->addWidget(button, 0, Qt::AlignVCenter);
    auto *glyph = new QLabel(card);
    glyph->setFixedSize(26, 26);
    glyph->setStyleSheet(QStringLiteral("background:transparent;"));
    glyph->setPixmap(QIcon(QStringLiteral(":/login/") + iconName).pixmap(26, 26));
    glyph->setAttribute(Qt::WA_TransparentForMouseEvents);
    row->addWidget(glyph, 0, Qt::AlignVCenter);
    auto *copy = new QVBoxLayout;
    copy->setSpacing(6);
    const QString titleText = button->text();
    auto *title = new QLabel(titleText, card);
    title->setStyleSheet(QStringLiteral("font-size:15px; font-weight:600; color:#252044; background:transparent;"));
    title->setAttribute(Qt::WA_TransparentForMouseEvents);
    copy->addWidget(title);
    auto *hint = new QLabel(description, card);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#706487; font-size:12px; background:transparent;"));
    hint->setAttribute(Qt::WA_TransparentForMouseEvents);
    copy->addWidget(hint);
    row->addLayout(copy, 1);
    button->setText({});
    button->setAccessibleName(titleText);
    return card;
}

void setChosen(QFrame *card, bool chosen)
{
    card->setStyleSheet(chosen
        ? QStringLiteral("QFrame#choiceCard { background:#f2ecff; border:1px solid #bda6fa; border-radius:14px; }")
        : QStringLiteral("QFrame#choiceCard { background:#fff; border:1px solid #e8e0f5; border-radius:14px; }"));
}
}

RoomPermissionsDialog::RoomPermissionsDialog(const QString &roomId, bool owner, QWidget *parent)
    : QDialog(parent), roomId_(roomId), owner_(owner)
{
    setWindowModality(Qt::WindowModal);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(900, 570);
    setMinimumSize(520, 420);
    auto shell = RoomDialogChrome::makeShell(this, QStringLiteral("聊天室权限与隐私"),
        QStringLiteral("shield.svg"), kStyle);
    auto *root = shell.layout;
    error_ = new QLabel(this);
    error_->setWordWrap(true);
    error_->setStyleSheet(QStringLiteral("color:#b54f68; background:#fff3f5; padding:8px; border-radius:8px;"));
    error_->hide();
    root->addWidget(error_);
    auto *body = new QFrame(this);
    body->setObjectName(QStringLiteral("permissionsBody"));
    auto *columns = new QHBoxLayout(body);
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setSpacing(0);
    auto *nav = new QFrame(body);
    nav->setObjectName(QStringLiteral("navigationPanel"));
    nav->setFixedWidth(226);
    auto *navLayout = new QVBoxLayout(nav);
    navLayout->setContentsMargins(12, 18, 12, 16);
    navLayout->setSpacing(10);
    const QStringList titles = {QStringLiteral("聊天室隐私"), QStringLiteral("加入规则"),
                                QStringLiteral("成员权限")};
    const QStringList glyphs = {QStringLiteral("lock.svg"), QStringLiteral("members.svg"),
                                QStringLiteral("shield.svg")};
    for (int i = 0; i < 3; ++i) {
        auto *button = new QPushButton(nav);
        button->setObjectName(QStringLiteral("permissionsNav"));
        button->setAccessibleName(titles.at(i));
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setFixedHeight(64);
        button->setCursor(Qt::PointingHandCursor);
        button->setProperty("selected", i == 0);
        auto *navRow = new QHBoxLayout(button);
        navRow->setContentsMargins(15, 0, 12, 0);
        navRow->setSpacing(13);
        auto *navIcon = new QLabel(button);
        navIcon->setFixedSize(26, 26);
        QPixmap navPixmap = QIcon(QStringLiteral(":/login/") + glyphs.at(i)).pixmap(26, 26);
        {
            QPainter painter(&navPixmap);
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter.fillRect(navPixmap.rect(), QColor(QStringLiteral("#7054cf")));
        }
        navIcon->setPixmap(navPixmap);
        navIcon->setAlignment(Qt::AlignCenter);
        navIcon->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto *navText = new QLabel(titles.at(i), button);
        navText->setObjectName(QStringLiteral("permissionsNavText"));
        navText->setAttribute(Qt::WA_TransparentForMouseEvents);
        navRow->addWidget(navIcon, 0, Qt::AlignVCenter);
        navRow->addWidget(navText, 0, Qt::AlignVCenter);
        navRow->addStretch();
        navLayout->addWidget(button);
        navigation_[i] = button;
        connect(button, &QPushButton::clicked, this, [this, i]() { selectPage(i); });
    }
    navLayout->addStretch();
    columns->addWidget(nav);
    pages_ = new QStackedWidget(body);
    pages_->setMinimumWidth(0);
    pages_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVBoxLayout *privacyLayout;
    auto *privacyPage = pageContents(pages_, &privacyLayout, QStringLiteral("聊天室隐私"), {});
    private_ = new QRadioButton(QStringLiteral("私密聊天室（推荐）"), privacyPage);
    public_ = new QRadioButton(QStringLiteral("公开聊天室"), privacyPage);
    auto *privacyGroup = new QButtonGroup(this);
    privacyGroup->addButton(private_); privacyGroup->addButton(public_);
    privateCard_ = radioCard(privacyPage, private_, QStringLiteral("lock.svg"),
        QStringLiteral("仅房间成员可以进入，不显示在“发现公开聊天室”列表。新成员必须通过邀请加入，适合团队协作。"));
    publicCard_ = radioCard(privacyPage, public_, QStringLiteral("members.svg"),
        QStringLiteral("可以在“发现公开聊天室”中被找到，其他用户可根据加入规则申请或直接加入。"));
    privacyLayout->addWidget(privateCard_);
    privacyLayout->addWidget(publicCard_);
    privacyLayout->addStretch();
    pages_->addWidget(scrollPage(pages_, privacyPage));

    QVBoxLayout *joinLayout;
    auto *joinPage = pageContents(pages_, &joinLayout, QStringLiteral("加入规则"),
        QStringLiteral("设置公开聊天室的加入方式；私密聊天室始终通过邀请加入。"));
    direct_ = new QRadioButton(QStringLiteral("允许直接加入"), joinPage);
    approval_ = new QRadioButton(QStringLiteral("需要房主审核（暂未开放）"), joinPage);
    auto *joinGroup = new QButtonGroup(this);
    joinGroup->addButton(direct_); joinGroup->addButton(approval_);
    directCard_ = radioCard(joinPage, direct_, QStringLiteral("members.svg"),
        QStringLiteral("服务端验证后，用户可直接加入公开聊天室。"));
    approvalCard_ = radioCard(joinPage, approval_, QStringLiteral("shield.svg"),
        QStringLiteral("申请与房主审核流程尚未实现，此项暂不可选。"));
    approval_->setEnabled(false);
    joinLayout->addWidget(directCard_);
    joinLayout->addWidget(approvalCard_);
    joinLayout->addStretch();
    pages_->addWidget(scrollPage(pages_, joinPage));

    QVBoxLayout *memberLayout;
    auto *memberPage = pageContents(pages_, &memberLayout, QStringLiteral("成员权限"),
        QStringLiteral("配置普通成员可执行的管理操作。"));
    invite_ = new QCheckBox(QStringLiteral("允许成员邀请他人"), memberPage);
    edit_ = new QCheckBox(QStringLiteral("允许成员修改聊天室信息"), memberPage);
    kick_ = new QCheckBox(QStringLiteral("允许成员移除其他普通成员"), memberPage);
    manage_ = new QCheckBox(QStringLiteral("允许成员管理权限与隐私（仅房主可操作）"), memberPage);
    manage_->setEnabled(false);
    invite_->setToolTip(QStringLiteral("允许成员创建邀请并邀请已注册用户"));
    edit_->setToolTip(QStringLiteral("允许成员编辑名称和简介"));
    kick_->setToolTip(QStringLiteral("允许成员移除其他普通成员，不能移除房主"));
    for (QCheckBox *box : {invite_, edit_, kick_, manage_}) {
        box->setMinimumHeight(43);
        memberLayout->addWidget(box);
    }
    memberLayout->addStretch();
    pages_->addWidget(scrollPage(pages_, memberPage));
    columns->addWidget(pages_, 1);
    root->addWidget(body, 1);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *cancel = new QPushButton(QStringLiteral("取消"), this);
    save_ = new QPushButton(QStringLiteral("保存设置"), this);
    save_->setObjectName(QStringLiteral("primary"));
    cancel->setMinimumWidth(112);
    save_->setMinimumWidth(126);
    buttons->addWidget(cancel); buttons->addWidget(save_);
    root->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(save_, &QPushButton::clicked, this, [this]() {
        setPending(true);
        emit saveRequested(roomId_, currentSettings());
    });
    for (auto *radio : {private_, public_, direct_, approval_})
        connect(radio, &QRadioButton::toggled, this, &RoomPermissionsDialog::updateState);
    for (auto *box : {invite_, edit_, kick_})
        connect(box, &QCheckBox::toggled, this, &RoomPermissionsDialog::updateState);
    direct_->setChecked(true);
    selectPage(0);
    updateState();
}

QJsonObject RoomPermissionsDialog::currentSettings() const
{
    return {{QStringLiteral("room_type"), public_->isChecked() ? QStringLiteral("public") : QStringLiteral("invite_only")},
        {QStringLiteral("join_policy"), QStringLiteral("direct")},
        {QStringLiteral("allow_member_invite"), invite_->isChecked()},
        {QStringLiteral("allow_member_edit"), edit_->isChecked()},
        {QStringLiteral("allow_member_kick"), kick_->isChecked()},
        {QStringLiteral("allow_member_manage_permissions"), false}};
}

void RoomPermissionsDialog::setPermissions(const QJsonObject &settings)
{
    if (settings.isEmpty()) return;
    loaded_ = false;
    (settings.value(QStringLiteral("room_type")).toString() == QStringLiteral("public")
        ? public_ : private_)->setChecked(true);
    direct_->setChecked(true);
    invite_->setChecked(settings.value(QStringLiteral("allow_member_invite")).toBool());
    edit_->setChecked(settings.value(QStringLiteral("allow_member_edit")).toBool());
    kick_->setChecked(settings.value(QStringLiteral("allow_member_kick")).toBool());
    baseline_ = currentSettings();
    loaded_ = true;
    showError({});
    updateState();
}

void RoomPermissionsDialog::setPending(bool pending)
{
    pending_ = pending;
    save_->setText(pending ? QStringLiteral("保存中…") : QStringLiteral("保存设置"));
    updateState();
}

void RoomPermissionsDialog::showError(const QString &message)
{
    error_->setText(message);
    error_->setVisible(!message.isEmpty());
}

void RoomPermissionsDialog::selectPage(int index)
{
    pages_->setCurrentIndex(index);
    for (int i = 0; i < 3; ++i) {
        navigation_[i]->setProperty("selected", i == index);
        navigation_[i]->style()->unpolish(navigation_[i]);
        navigation_[i]->style()->polish(navigation_[i]);
    }
}

void RoomPermissionsDialog::refreshCardStyles()
{
    setChosen(privateCard_, private_->isChecked());
    setChosen(publicCard_, public_->isChecked());
    setChosen(directCard_, direct_->isChecked());
    setChosen(approvalCard_, approval_->isChecked());
}

void RoomPermissionsDialog::updateState()
{
    const bool editable = owner_ && loaded_ && !pending_;
    private_->setEnabled(editable);
    public_->setEnabled(editable);
    direct_->setEnabled(editable && public_->isChecked());
    approval_->setEnabled(false);
    invite_->setEnabled(editable);
    edit_->setEnabled(editable);
    kick_->setEnabled(editable);
    save_->setVisible(owner_);
    save_->setEnabled(editable && currentSettings() != baseline_);
    refreshCardStyles();
}

void RoomPermissionsDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    QWidget *anchor = parentWidget();
    while (anchor && anchor->parentWidget()) anchor = anchor->parentWidget();
    const QPoint center = anchor ? anchor->mapToGlobal(anchor->rect().center()) : pos();
    const QScreen *targetScreen = QGuiApplication::screenAt(center);
    const QRect bounds = targetScreen ? targetScreen->availableGeometry()
                                      : QRect(0, 0, 1024, 768);
    const QSize available = anchor ? anchor->size().boundedTo(bounds.size()) : bounds.size();
    resize(qMin(900, qMax(minimumWidth(), qMin(bounds.width(), available.width()) - 24)),
           qMin(570, qMax(minimumHeight(), qMin(bounds.height(), available.height()) - 24)));
    if (auto *nav = findChild<QFrame *>(QStringLiteral("navigationPanel")))
        nav->setFixedWidth(width() < 760 ? 186 : 226);
    QPoint topLeft = center - QPoint(width() / 2, height() / 2);
    topLeft.setX(qBound(bounds.left(), topLeft.x(), bounds.right() - width() + 1));
    topLeft.setY(qBound(bounds.top(), topLeft.y(), bounds.bottom() - height() + 1));
    move(topLeft);
}
