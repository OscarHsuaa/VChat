#include "roommanagementdialog.h"
#include "roomdialogchrome.h"

#include <QDateTime>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QScreen>
#include <QGuiApplication>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {
const QString kStyle = QStringLiteral(
    "QDialog { background:transparent; color:#252044; font-family:'Microsoft YaHei UI'; font-size:13px; }"
    "QFrame#roomPanel { background:#fdfbff; border:1px solid #e9e0f7; border-radius:19px; }"
    "QLabel { color:#51466d; background:transparent; }"
    "QLineEdit,QPlainTextEdit { background:white; border:1px solid #ded5f1; border-radius:10px;"
    " padding:9px 12px; color:#292342; selection-background-color:#d8c8f8; }"
    "QLineEdit:focus,QPlainTextEdit:focus { border:1px solid #8064d9; }"
    "QPushButton { min-height:36px; border-radius:10px; padding:0 15px;"
    " background:#f2edfb; color:#54447b; border:0; }"
    "QPushButton:hover { background:#e9e0f9; } QPushButton:pressed { background:#ded0f4; }"
    "QPushButton#primary { background:#8064d9; color:white; font-weight:600; }"
    "QPushButton#primary:hover { background:#9175e6; }"
    "QPushButton#primary:pressed { background:#6c50c0; }"
    "QPushButton#menuRow { min-height:54px; padding:0; text-align:left;"
    " background:transparent; border:0; border-radius:9px; }"
    "QPushButton#menuRow:hover { background:#f1ecfc; }"
    "QPushButton#menuRow:pressed { background:#e8def9; }"
    "QPushButton#menuRow:disabled { background:transparent; }"
    "QPushButton#danger { color:#bf4965; background:#fff3f5; }"
    "QPushButton#danger:hover { background:#fce8ee; }"
    "QPushButton:disabled { color:#aaa2ba; background:#f5f2f9; }"
    "QToolButton#dialogClose { background:transparent; border:0; border-radius:8px; }"
    "QToolButton#dialogClose:hover { background:#f1ecfc; }"
    "QToolButton#memberMore { background:#f0eaff; border:0; border-radius:8px; padding:0; }"
    "QToolButton#memberMore:hover { background:#e4d9ff; }"
    "QToolButton#memberMore:pressed { background:#d8c9ff; }"
    "QScrollArea { background:transparent; border:0; }"
    "QScrollArea QWidget#memberContents { background:transparent; }"
    "QScrollBar:vertical { background:transparent; width:7px; margin:0; }"
    "QScrollBar::handle:vertical { background:#d5c6ed; border-radius:3px; min-height:24px; }"
    "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }"
    "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical { background:transparent; }");

const QString kMenuStyle = QStringLiteral(
    "QMenu { background:white; color:#352b59; border:1px solid #e8def6;"
    " border-radius:11px; padding:5px; font-family:'Microsoft YaHei UI'; font-size:13px; }"
    "QMenu::item { min-height:34px; padding:5px 18px 5px 12px; border-radius:7px; }"
    "QMenu::item:selected { background:#f1ecfc; }");

QLabel *iconLabel(const QString &name, int size, QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setFixedSize(size, size);
    label->setPixmap(QIcon(QStringLiteral(":/login/") + name).pixmap(size, size));
    label->setAlignment(Qt::AlignCenter);
    return label;
}

QIcon memberMoreIcon()
{
    QPixmap pixmap(40, 40);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#6650B5")));
    for (int x : {5, 10, 15}) painter.drawEllipse(QPointF(x, 10), 1.55, 1.55);
    return QIcon(pixmap);
}

QFrame *divider(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setFixedHeight(1);
    line->setStyleSheet(QStringLiteral("background:#eee8f7; border:0;"));
    return line;
}

QLabel *detailRow(QVBoxLayout *layout, QWidget *parent, const QString &icon,
                  const QString &caption)
{
    auto *row = new QHBoxLayout;
    row->setSpacing(10);
    row->addWidget(iconLabel(icon, 17, parent));
    auto *key = new QLabel(caption, parent);
    key->setFixedWidth(72);
    key->setStyleSheet(QStringLiteral("color:#807696;"));
    row->addWidget(key);
    auto *value = new QLabel(parent);
    value->setTextFormat(Qt::PlainText);
    value->setWordWrap(true);
    value->setStyleSheet(QStringLiteral("color:#332951;"));
    row->addWidget(value, 1);
    layout->addLayout(row);
    return value;
}

QPushButton *actionRow(const QString &text, const QString &icon, QWidget *parent,
                       const QString &hint = {}, bool showArrow = true)
{
    auto *button = new QPushButton(parent);
    button->setObjectName(QStringLiteral("menuRow"));
    button->setCursor(Qt::PointingHandCursor);
    button->setAccessibleName(text);
    button->setFixedHeight(54);
    auto *layout = new QHBoxLayout(button);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(13);
    auto *glyph = iconLabel(icon, 20, button);
    glyph->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(glyph);
    auto *label = new QLabel(text, button);
    label->setStyleSheet(QStringLiteral("font-size:13px; font-weight:600; color:#352b59;"));
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(label, 1);
    if (!hint.isEmpty()) {
        auto *note = new QLabel(hint, button);
        note->setStyleSheet(QStringLiteral("font-size:12px; color:#8d81a6;"));
        note->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(note);
    }
    if (showArrow) {
        auto *chevron = iconLabel(QStringLiteral("chevron-right.svg"), 16, button);
        chevron->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(chevron);
    }
    return button;
}

}

RoomManagementDialog::RoomManagementDialog(const QString &roomId, qint64 accountId, QWidget *parent)
    : QDialog(parent), id_(roomId), accountId_(accountId)
{
    setWindowTitle(QStringLiteral("聊天室详情"));
    setWindowModality(Qt::WindowModal);
    setAttribute(Qt::WA_DeleteOnClose);
    setFixedWidth(500);
    const auto shell = RoomDialogChrome::makeShell(this, QStringLiteral("聊天室详情"),
                                                   QStringLiteral("members.svg"), kStyle);
    auto *root = shell.layout;
    headerTitle_ = shell.title;
    headerIcon_ = shell.icon;
    error_ = new QLabel(this);
    error_->setWordWrap(true);
    error_->setStyleSheet(QStringLiteral("color:#b54f68; background:#fff3f5; padding:8px; border-radius:8px;"));
    error_->hide();
    root->addWidget(error_);
    pages_ = new QStackedWidget(this);
    root->addWidget(pages_);

    auto *details = new QWidget(this);
    auto *detailLayout = new QVBoxLayout(details);
    detailLayout->setContentsMargins(0, 7, 0, 0);
    detailLayout->setSpacing(12);
    auto *identity = new QHBoxLayout;
    identity->setSpacing(14);
    auto *avatar = new QLabel(details);
    avatar->setFixedSize(60, 60);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet(QStringLiteral("background:#eee6fc; border-radius:30px;"));
    avatar->setPixmap(QIcon(QStringLiteral(":/login/members.svg")).pixmap(29, 29));
    identity->addWidget(avatar, 0, Qt::AlignTop);
    auto *identityText = new QVBoxLayout;
    identityText->setSpacing(5);
    auto *nameRow = new QHBoxLayout;
    nameRow->setSpacing(8);
    nameLabel_ = new QLabel(QStringLiteral("正在加载…"), details);
    nameLabel_->setWordWrap(true);
    nameLabel_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    nameLabel_->setTextFormat(Qt::PlainText);
    nameLabel_->setMaximumWidth(235);
    nameLabel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    nameLabel_->setStyleSheet(QStringLiteral("font-size:19px; font-weight:700; color:#252044;"));
    nameRow->addWidget(nameLabel_);
    auto *typeBadge = new QFrame(details);
    typeBadge->setStyleSheet(QStringLiteral("background:#f0e9fc; border-radius:12px;"));
    auto *badgeLayout = new QHBoxLayout(typeBadge);
    badgeLayout->setContentsMargins(8, 3, 8, 3);
    badgeLayout->setSpacing(4);
    typeIcon_ = iconLabel(QStringLiteral("lock.svg"), 14, typeBadge);
    badgeLayout->addWidget(typeIcon_);
    typeLabel_ = new QLabel(typeBadge);
    typeLabel_->setStyleSheet(QStringLiteral("color:#6c50bd; font-size:11px; font-weight:600;"));
    badgeLayout->addWidget(typeLabel_);
    nameRow->addWidget(typeBadge, 0, Qt::AlignTop);
    nameRow->addStretch();
    identityText->addLayout(nameRow);
    descriptionLabel_ = new QLabel(details);
    descriptionLabel_->setWordWrap(true);
    descriptionLabel_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    descriptionLabel_->setTextFormat(Qt::PlainText);
    descriptionLabel_->setStyleSheet(QStringLiteral("color:#807696; font-size:12px;"));
    identityText->addWidget(descriptionLabel_);
    identity->addLayout(identityText, 1);
    detailLayout->addLayout(identity);
    detailLayout->addSpacing(8);
    ownerLabel_ = detailRow(detailLayout, details, QStringLiteral("user.svg"), QStringLiteral("房主"));
    countLabel_ = detailRow(detailLayout, details, QStringLiteral("members.svg"), QStringLiteral("成员"));
    createdLabel_ = detailRow(detailLayout, details, QStringLiteral("clock.svg"), QStringLiteral("创建时间"));
    detailLayout->addSpacing(5);
    detailLayout->addWidget(divider(details));
    editButton_ = actionRow(QStringLiteral("编辑聊天室信息"), QStringLiteral("edit.svg"), details);
    manageButton_ = actionRow(QStringLiteral("成员管理"), QStringLiteral("members.svg"), details);
    leaveButton_ = new QPushButton(QStringLiteral("退出聊天室"), details);
    dissolveButton_ = new QPushButton(QStringLiteral("解散聊天室"), details);
    leaveButton_->setIcon(QIcon(QStringLiteral(":/login/log-out.svg")));
    dissolveButton_->setIcon(QIcon(QStringLiteral(":/login/trash.svg")));
    leaveButton_->setIconSize(QSize(18, 18));
    dissolveButton_->setIconSize(QSize(18, 18));
    leaveButton_->setCursor(Qt::PointingHandCursor);
    dissolveButton_->setCursor(Qt::PointingHandCursor);
    leaveButton_->setObjectName(QStringLiteral("danger"));
    dissolveButton_->setObjectName(QStringLiteral("danger"));
    detailLayout->addWidget(editButton_);
    detailLayout->addWidget(divider(details));
    detailLayout->addWidget(manageButton_);
    auto *privacyRow = actionRow(QStringLiteral("聊天室权限与隐私"), QStringLiteral("shield.svg"),
                                 details, QStringLiteral("暂未开放"), false);
    privacyRow->setEnabled(false);
    privacyRow->setToolTip(QStringLiteral("当前仅展示房间类型，暂不支持修改加入方式"));
    detailLayout->addWidget(divider(details));
    detailLayout->addWidget(privacyRow);
    detailLayout->addSpacing(16);
    detailLayout->addWidget(leaveButton_);
    detailLayout->addWidget(dissolveButton_);
    pages_->addWidget(details);
    connect(editButton_, &QPushButton::clicked, this, [this]() { showPage(QStringLiteral("edit")); });
    connect(manageButton_, &QPushButton::clicked, this, [this]() { showPage(QStringLiteral("members")); });
    connect(leaveButton_, &QPushButton::clicked, this, [this]() {
        if (confirm(QStringLiteral("退出聊天室"),
                    QStringLiteral("退出后，你将无法继续查看该聊天室的新消息。确定退出吗？")))
            emit leaveRequested(id_);
    });
    connect(dissolveButton_, &QPushButton::clicked, this, [this]() {
        if (confirm(QStringLiteral("解散聊天室"),
                    QStringLiteral("解散「%1」后，所有成员将无法继续访问。历史记录仍会保留，此操作不可直接撤销。")
                        .arg(room_.value(QStringLiteral("room_name")).toString())) &&
            confirm(QStringLiteral("再次确认解散"),
                    QStringLiteral("请输入聊天室名称，确认解散「%1」。")
                        .arg(room_.value(QStringLiteral("room_name")).toString()), true))
            emit dissolveRequested(id_);
    });

    auto *editor = new QWidget(this);
    auto *editLayout = new QVBoxLayout(editor);
    editLayout->setContentsMargins(0, 10, 0, 0);
    editLayout->setSpacing(12);
    auto *nameCaption = new QLabel(QStringLiteral("聊天室名称 *"), editor);
    nameCaption->setStyleSheet(QStringLiteral("color:#332951; font-weight:600;"));
    editLayout->addWidget(nameCaption);
    nameEdit_ = new QLineEdit(editor);
    nameEdit_->setMaxLength(60);
    nameEdit_->setMinimumHeight(44);
    editLayout->addWidget(nameEdit_);
    auto *descriptionCaption = new QLabel(QStringLiteral("聊天室简介"), editor);
    descriptionCaption->setStyleSheet(QStringLiteral("color:#332951; font-weight:600;"));
    editLayout->addWidget(descriptionCaption);
    descriptionEdit_ = new QPlainTextEdit(editor);
    descriptionEdit_->setPlaceholderText(QStringLiteral("简单介绍这个聊天室的任务或用途"));
    descriptionEdit_->setFixedHeight(112);
    editLayout->addWidget(descriptionEdit_);
    descriptionCount_ = new QLabel(QStringLiteral("0 / 200"), editor);
    descriptionCount_->setAlignment(Qt::AlignRight);
    descriptionCount_->setStyleSheet(QStringLiteral("color:#8d81a6; font-size:11px;"));
    editLayout->addWidget(descriptionCount_);
    connect(descriptionEdit_, &QPlainTextEdit::textChanged, this, [this]() {
        const int count = descriptionEdit_->toPlainText().toUcs4().size();
        descriptionCount_->setText(QStringLiteral("%1 / 200").arg(count));
        descriptionCount_->setStyleSheet(count > 200
            ? QStringLiteral("color:#bf4965; font-size:11px;")
            : QStringLiteral("color:#8d81a6; font-size:11px;"));
    });
    editLayout->addSpacing(12);
    auto *editButtons = new QHBoxLayout;
    auto *cancelEdit = new QPushButton(QStringLiteral("取消"), editor);
    saveButton_ = new QPushButton(QStringLiteral("保存修改"), editor);
    saveButton_->setObjectName(QStringLiteral("primary"));
    cancelEdit->setMinimumWidth(105);
    saveButton_->setMinimumWidth(105);
    cancelEdit->setCursor(Qt::PointingHandCursor);
    saveButton_->setCursor(Qt::PointingHandCursor);
    editButtons->addStretch();
    editButtons->addWidget(cancelEdit);
    editButtons->addWidget(saveButton_);
    editLayout->addLayout(editButtons);
    pages_->addWidget(editor);
    connect(cancelEdit, &QPushButton::clicked, this, [this]() { showPage(QStringLiteral("details")); });
    connect(saveButton_, &QPushButton::clicked, this, [this]() {
        const QString name = nameEdit_->text().trimmed();
        const QString description = descriptionEdit_->toPlainText().trimmed();
        const qsizetype nameLength = name.toUcs4().size();
        if (nameLength < 2 || nameLength > 30 || description.toUcs4().size() > 200) {
            showError(QStringLiteral("聊天室名称应为 2～30 字，简介不能超过 200 字。"));
            return;
        }
        setPending(true);
        showError({});
        emit saveRequested(id_, name, description);
    });

    auto *memberPage = new QWidget(this);
    auto *memberPageLayout = new QVBoxLayout(memberPage);
    memberPageLayout->setContentsMargins(0, 8, 0, 0);
    memberTitle_ = new QLabel(QStringLiteral("成员管理"), memberPage);
    memberTitle_->setStyleSheet(QStringLiteral("font-size:13px; color:#807696;"));
    memberPageLayout->addWidget(memberTitle_);
    membersScroll_ = new QScrollArea(memberPage);
    membersScroll_->setWidgetResizable(true);
    membersScroll_->setFrameShape(QFrame::NoFrame);
    membersScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    membersScroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    membersScroll_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *container = new QWidget(membersScroll_);
    container->setObjectName(QStringLiteral("memberContents"));
    membersLayout_ = new QVBoxLayout(container);
    membersLayout_->setSpacing(6);
    membersLayout_->setContentsMargins(0, 3, 0, 3);
    membersScroll_->setWidget(container);
    memberPageLayout->addWidget(membersScroll_);
    auto *back = new QPushButton(QStringLiteral("返回详情"), memberPage);
    memberPageLayout->addWidget(back, 0, Qt::AlignRight);
    connect(back, &QPushButton::clicked, this, [this]() { showPage(QStringLiteral("details")); });
    pages_->addWidget(memberPage);
}

void RoomManagementDialog::showError(const QString &message)
{
    error_->setText(message);
    error_->setVisible(!message.isEmpty());
    fitCurrentPage();
}

void RoomManagementDialog::setPending(bool pending)
{
    pending_ = pending;
    saveButton_->setEnabled(!pending);
    saveButton_->setText(pending ? QStringLiteral("正在保存…") : QStringLiteral("保存修改"));
    nameEdit_->setEnabled(!pending);
    descriptionEdit_->setEnabled(!pending);
    leaveButton_->setEnabled(!pending);
    dissolveButton_->setEnabled(!pending);
    for (QToolButton *button : pages_->widget(2)->findChildren<QToolButton *>())
        if (button->objectName() == QStringLiteral("memberMore")) button->setEnabled(!pending);
}

void RoomManagementDialog::showPage(const QString &page)
{
    showError({});
    if (page == QStringLiteral("leave") || page == QStringLiteral("dissolve")) {
        queuedAction_ = page;
        headerTitle_->setText(QStringLiteral("聊天室详情"));
        headerIcon_->setPixmap(QIcon(QStringLiteral(":/login/members.svg")).pixmap(21, 21));
        pages_->setCurrentIndex(0);
        fitCurrentPage();
        runQueuedAction();
        return;
    }
    queuedAction_.clear();
    if (page == QStringLiteral("edit")) {
        nameEdit_->setText(room_.value(QStringLiteral("room_name")).toString());
        descriptionEdit_->setPlainText(room_.value(QStringLiteral("description")).toString());
        headerTitle_->setText(QStringLiteral("编辑聊天室信息"));
        headerIcon_->setPixmap(QIcon(QStringLiteral(":/login/edit.svg")).pixmap(21, 21));
        pages_->setCurrentIndex(1);
    } else if (page == QStringLiteral("members")) {
        headerTitle_->setText(QStringLiteral("成员管理"));
        headerIcon_->setPixmap(QIcon(QStringLiteral(":/login/members.svg")).pixmap(21, 21));
        pages_->setCurrentIndex(2);
    } else {
        headerTitle_->setText(QStringLiteral("聊天室详情"));
        headerIcon_->setPixmap(QIcon(QStringLiteral(":/login/members.svg")).pixmap(21, 21));
        pages_->setCurrentIndex(0);
    }
    fitCurrentPage();
}

void RoomManagementDialog::setRoom(const QJsonObject &room)
{
    if (room.value(QStringLiteral("room_id")).toString() != id_) return;
    room_ = room;
    refreshDetails();
    runQueuedAction();
}

void RoomManagementDialog::runQueuedAction()
{
    if (queuedAction_.isEmpty() || room_.value(QStringLiteral("room_name")).toString().isEmpty())
        return;
    const QString action = queuedAction_;
    queuedAction_.clear();
    QTimer::singleShot(0, this, [this, action]() {
        QPushButton *button = action == QStringLiteral("dissolve") ? dissolveButton_ : leaveButton_;
        if (button->isVisible() && button->isEnabled()) button->click();
    });
}

void RoomManagementDialog::refreshDetails()
{
    nameLabel_->setText(room_.value(QStringLiteral("room_name")).toString(QStringLiteral("聊天室")));
    nameLabel_->setFixedWidth(qMin(235, nameLabel_->fontMetrics().horizontalAdvance(nameLabel_->text()) + 5));
    const int nameHeight = nameLabel_->fontMetrics()
        .boundingRect(QRect(0, 0, 230, 10000), Qt::TextWordWrap, nameLabel_->text()).height();
    nameLabel_->setMinimumHeight(nameHeight + 4);
    const bool publicRoom = room_.value(QStringLiteral("room_type")).toString() == QStringLiteral("public");
    typeLabel_->setText(publicRoom ? QStringLiteral("公开聊天室") : QStringLiteral("私密聊天室"));
    typeIcon_->setPixmap(QIcon(publicRoom ? QStringLiteral(":/login/room.svg")
                                         : QStringLiteral(":/login/lock.svg")).pixmap(14, 14));
    ownerLabel_->setText(room_.value(QStringLiteral("owner_name")).toString(QStringLiteral("未知")));
    const int total = room_.value(QStringLiteral("member_count")).toInt();
    if (membersLoaded_) {
        int online = 0;
        for (const QJsonValue &value : members_)
            if (value.toObject().value(QStringLiteral("online")).toBool()) ++online;
        countLabel_->setText(QStringLiteral("%1 人（%2 人在线）").arg(total).arg(online));
    } else countLabel_->setText(QStringLiteral("%1 人（在线状态加载中）").arg(total));
    const QDateTime created = QDateTime::fromString(room_.value(QStringLiteral("created_at")).toString(), Qt::ISODateWithMs);
    createdLabel_->setText(created.isValid()
        ? created.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
        : QStringLiteral("未知"));
    const QString description = room_.value(QStringLiteral("description")).toString();
    descriptionLabel_->setText(description.isEmpty() ? QStringLiteral("暂无简介") : description);
    // QLabel's height-for-width can understate long wrapped text inside QStackedWidget.
    // Reserve its measured text height before sizing the dialog.
    const int textHeight = descriptionLabel_->fontMetrics()
        .boundingRect(QRect(0, 0, 330, 10000), Qt::TextWordWrap,
                      descriptionLabel_->text()).height();
    descriptionLabel_->setMinimumHeight(textHeight + 4);
    const bool owner = room_.value(QStringLiteral("owner_id")).toString().toLongLong() == accountId_;
    editButton_->setVisible(owner);
    manageButton_->setAccessibleName(owner ? QStringLiteral("成员管理") : QStringLiteral("查看成员"));
    for (QLabel *label : manageButton_->findChildren<QLabel *>())
        if (label->text() == QStringLiteral("成员管理") || label->text() == QStringLiteral("查看成员"))
            label->setText(owner ? QStringLiteral("成员管理") : QStringLiteral("查看成员"));
    leaveButton_->setVisible(!owner && id_ != QStringLiteral("class-room"));
    dissolveButton_->setVisible(owner && id_ != QStringLiteral("class-room"));
    refreshMembers();
}

void RoomManagementDialog::setMembers(const QJsonArray &members)
{
    members_ = members;
    membersLoaded_ = true;
    if (!room_.isEmpty()) refreshDetails();
    else refreshMembers();
}

void RoomManagementDialog::refreshMembers()
{
    if (!membersLayout_) return;
    while (auto *item = membersLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    QVector<QJsonObject> sorted;
    for (const QJsonValue &value : members_) sorted.append(value.toObject());
    std::stable_sort(sorted.begin(), sorted.end(), [](const QJsonObject &a, const QJsonObject &b) {
        const int rankA = a.value(QStringLiteral("role")).toString() == QStringLiteral("owner") ? 0
            : a.value(QStringLiteral("online")).toBool() ? 1 : 2;
        const int rankB = b.value(QStringLiteral("role")).toString() == QStringLiteral("owner") ? 0
            : b.value(QStringLiteral("online")).toBool() ? 1 : 2;
        return rankA < rankB;
    });
    const bool owner = room_.value(QStringLiteral("owner_id")).toString().toLongLong() == accountId_;
    memberTitle_->setText(QStringLiteral("%1（%2 人）")
        .arg(owner ? QStringLiteral("成员管理") : QStringLiteral("查看成员"))
        .arg(sorted.size()));
    membersScroll_->setVerticalScrollBarPolicy(sorted.size() > 5
        ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    for (const QJsonObject &member : sorted) {
        const QString id = member.value(QStringLiteral("user_id")).toString();
        const QString name = member.value(QStringLiteral("nickname")).toString();
        const bool memberOwner = member.value(QStringLiteral("role")).toString() == QStringLiteral("owner");
        auto *card = new QFrame(pages_->widget(2));
        card->setObjectName(QStringLiteral("memberCard"));
        card->setStyleSheet(QStringLiteral(
            "QFrame#memberCard { background:#faf8fe; border-radius:11px; }"
            "QFrame#memberCard:hover { background:#f2ecfc; }"));
        card->setMinimumHeight(66);
        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(12, 8, 12, 8);
        row->setSpacing(10);
        auto *avatar = new QLabel(name.left(1), card);
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setFixedSize(40, 40);
        avatar->setStyleSheet(QStringLiteral(
            "background:#e9defb; color:#7856ce; border-radius:20px; font-size:15px; font-weight:600;"));
        row->addWidget(avatar);
        auto *nameLabel = new QLabel(card);
        nameLabel->setObjectName(QStringLiteral("memberName"));
        nameLabel->setTextFormat(Qt::PlainText);
        nameLabel->setStyleSheet(QStringLiteral("color:#302747; font-size:13px; font-weight:600;"));
        nameLabel->setFixedWidth(145);
        nameLabel->ensurePolished();
        nameLabel->setText(nameLabel->fontMetrics().elidedText(name, Qt::ElideRight, nameLabel->width()));
        nameLabel->setToolTip(name);
        row->addWidget(nameLabel);
        auto *role = new QLabel(memberOwner ? QStringLiteral("房主") : QStringLiteral("成员"), card);
        role->setObjectName(QStringLiteral("memberRole"));
        role->setFixedSize(54, 23);
        role->setAlignment(Qt::AlignCenter);
        role->setStyleSheet(memberOwner
            ? QStringLiteral("color:#a87524; background:#fff0d6; border-radius:9px; padding:2px 7px; font-size:11px;")
            : QStringLiteral("color:#795fbd; background:#efe8fc; border-radius:9px; padding:2px 7px; font-size:11px;"));
        row->addWidget(role, 0, Qt::AlignVCenter);
        const bool online = member.value(QStringLiteral("online")).toBool();
        auto *status = new QWidget(card);
        status->setFixedWidth(66);
        auto *statusLayout = new QHBoxLayout(status);
        statusLayout->setContentsMargins(0, 0, 0, 0);
        statusLayout->setSpacing(6);
        auto *dot = new QFrame(status);
        dot->setObjectName(QStringLiteral("memberStatusDot"));
        dot->setFixedSize(8, 8);
        dot->setStyleSheet(online
            ? QStringLiteral("background:#31b875; border-radius:4px;")
            : QStringLiteral("background:#9a92a8; border-radius:4px;"));
        statusLayout->addWidget(dot);
        auto *statusText = new QLabel(online ? QStringLiteral("在线") : QStringLiteral("离线"), status);
        statusText->setStyleSheet(online
            ? QStringLiteral("color:#31b875;") : QStringLiteral("color:#9a92a8;"));
        statusLayout->addWidget(statusText);
        statusLayout->addStretch();
        row->addWidget(status, 0, Qt::AlignVCenter);
        auto *actionSlot = new QWidget(card);
        actionSlot->setObjectName(QStringLiteral("memberActionSlot"));
        actionSlot->setFixedSize(32, 32);
        auto *actionLayout = new QHBoxLayout(actionSlot);
        actionLayout->setContentsMargins(0, 0, 0, 0);
        if (owner && !memberOwner && id.toLongLong() != accountId_) {
            auto *more = new QToolButton(actionSlot);
            more->setObjectName(QStringLiteral("memberMore"));
            more->setIcon(memberMoreIcon());
            more->setIconSize(QSize(20, 20));
            more->setToolButtonStyle(Qt::ToolButtonIconOnly);
            more->setToolTip(QStringLiteral("管理 %1").arg(name));
            more->setCursor(Qt::PointingHandCursor);
            more->setFixedSize(32, 32);
            more->setEnabled(!pending_);
            actionLayout->addWidget(more);
            connect(more, &QToolButton::clicked, this, [this, more, id, name]() {
                if (pending_) return;
                auto *menu = new QMenu(more);
                menu->setAttribute(Qt::WA_DeleteOnClose);
                menu->setStyleSheet(kMenuStyle);
                QAction *remove = menu->addAction(QIcon(QStringLiteral(":/login/trash.svg")),
                                                  QStringLiteral("移除成员"));
                connect(remove, &QAction::triggered, this, [this, id, name]() {
                    if (pending_) return;
                    if (confirm(QStringLiteral("移除成员"),
                                QStringLiteral("确定将「%1」移出聊天室吗？").arg(name))) {
                        setPending(true);
                        emit removeRequested(id_, id);
                    }
                });
                menu->popup(more->mapToGlobal(QPoint(0, more->height())));
            });
        }
        row->addWidget(actionSlot, 0, Qt::AlignVCenter);
        membersLayout_->addWidget(card);
    }
    const int visibleRows = qMin(sorted.size(), 5);
    membersScroll_->setFixedHeight(visibleRows > 0 ? visibleRows * 66 + (visibleRows - 1) * 6 + 8 : 48);
    fitCurrentPage();
}

void RoomManagementDialog::fitCurrentPage()
{
    if (!pages_ || !pages_->currentWidget()) return;
    QWidget *page = pages_->currentWidget();
    if (!page->layout()) return;
    page->layout()->invalidate();
    pages_->setFixedHeight(page->layout()->sizeHint().height());
    pages_->updateGeometry();
    auto *panel = findChild<QFrame *>(QStringLiteral("roomPanel"));
    panel->layout()->invalidate();
    panel->updateGeometry();
    layout()->invalidate();
    layout()->activate();
    setFixedHeight(layout()->sizeHint().height());
    if (isVisible()) RoomDialogChrome::centerOnParent(this);
}

bool RoomManagementDialog::confirm(const QString &title, const QString &message, bool requireName)
{
    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setMinimumWidth(410);
    const auto shell = RoomDialogChrome::makeShell(&dialog, title,
        title.contains(QStringLiteral("解散")) || title.contains(QStringLiteral("移除"))
            ? QStringLiteral("trash.svg") : QStringLiteral("log-out.svg"), kStyle);
    auto *layout = shell.layout;
    layout->setSpacing(15);
    auto *text = new QLabel(message, &dialog);
    text->setWordWrap(true);
    layout->addWidget(text);
    QLineEdit *typed = nullptr;
    if (requireName) {
        typed = new QLineEdit(&dialog);
        typed->setPlaceholderText(QStringLiteral("输入聊天室名称"));
        layout->addWidget(typed);
    }
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *cancel = new QPushButton(QStringLiteral("取消"), &dialog);
    auto *yes = new QPushButton(QStringLiteral("确认"), &dialog);
    yes->setObjectName(QStringLiteral("danger"));
    buttons->addWidget(cancel);
    buttons->addWidget(yes);
    layout->addLayout(buttons);
    if (typed) {
        yes->setEnabled(false);
        connect(typed, &QLineEdit::textChanged, &dialog, [this, typed, yes]() {
            yes->setEnabled(typed->text() == room_.value(QStringLiteral("room_name")).toString());
        });
    }
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(yes, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.adjustSize();
    RoomDialogChrome::centerOnParent(&dialog);
    return dialog.exec() == QDialog::Accepted;
}

void RoomManagementDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    fitCurrentPage();
    RoomDialogChrome::centerOnParent(this);
}
