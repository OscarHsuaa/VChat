#include "roominvitedialog.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {
const char *dialogStyle =
    "QDialog { background:#fcfaff; color:#252044; font-family:'Microsoft YaHei UI'; font-size:13px; }"
    "QLineEdit { background:white; border:1px solid #e4ddf3; border-radius:10px; padding:10px 13px; color:#292342; }"
    "QLineEdit:focus { border:1px solid #8064d9; }"
    "QListWidget { background:transparent; border:0; outline:0; }"
    "QPushButton { min-height:36px; border-radius:9px; padding:0 15px; }"
    "QPushButton#primary { background:#8064d9; color:white; border:0; font-weight:600; }"
    "QPushButton#primary:hover { background:#9275e8; }"
    "QPushButton#primary:disabled { background:#c9bce7; }"
    "QPushButton#secondary { background:white; color:#64577f; border:1px solid #e4ddf3; }"
    "QPushButton#secondary:hover { background:#f3eefb; }";

QLabel *avatar(const QString &name, QWidget *parent)
{
    auto *label = new QLabel(name.left(1), parent);
    label->setFixedSize(36, 36);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(QStringLiteral("background:#e9dffc; color:#7454c9; border-radius:18px; font-size:15px; font-weight:600;"));
    return label;
}
}

RoomInviteDialog::RoomInviteDialog(const QString &roomId, const QString &roomName, QWidget *parent)
    : QDialog(parent), roomId_(roomId)
{
    setWindowTitle(QStringLiteral("邀请成员加入"));
    setWindowModality(Qt::WindowModal);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(500, 510);
    setMinimumWidth(440);
    setStyleSheet(QString::fromLatin1(dialogStyle));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(25, 23, 25, 22);
    root->setSpacing(12);
    auto *title = new QLabel(QStringLiteral("邀请成员加入"), this);
    title->setStyleSheet(QStringLiteral("font-size:20px; font-weight:700; color:#252044;"));
    root->addWidget(title);
    auto *subtitle = new QLabel(QStringLiteral("搜索并邀请已注册的用户加入「%1」。").arg(roomName), this);
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet(QStringLiteral("color:#827799; font-size:12px;"));
    root->addWidget(subtitle);
    search_ = new QLineEdit(this);
    search_->setObjectName(QStringLiteral("inviteSearchInput"));
    search_->setPlaceholderText(QStringLiteral("搜索用户名或昵称"));
    search_->setMaxLength(50);
    search_->setMinimumHeight(44);
    root->addWidget(search_);
    results_ = new QListWidget(this);
    results_->setObjectName(QStringLiteral("inviteSearchResults"));
    results_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    root->addWidget(results_, 1);
    empty_ = new QLabel(QStringLiteral("正在搜索用户…"), this);
    empty_->setAlignment(Qt::AlignCenter);
    empty_->setStyleSheet(QStringLiteral("color:#827799; padding:10px;"));
    root->addWidget(empty_);
    message_ = new QLabel(this);
    message_->setWordWrap(true);
    message_->hide();
    root->addWidget(message_);
    auto *footer = new QHBoxLayout;
    selection_ = new QLabel(QStringLiteral("已选择 0 人"), this);
    selection_->setStyleSheet(QStringLiteral("color:#756a90;"));
    footer->addWidget(selection_, 1);
    auto *cancel = new QPushButton(QStringLiteral("取消"), this);
    cancel->setObjectName(QStringLiteral("secondary"));
    footer->addWidget(cancel);
    send_ = new QPushButton(QStringLiteral("发送邀请"), this);
    send_->setObjectName(QStringLiteral("primary"));
    send_->setEnabled(false);
    footer->addWidget(send_);
    root->addLayout(footer);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(send_, &QPushButton::clicked, this, [this]() {
        if (pending_ || selected_.isEmpty()) return;
        QJsonArray ids;
        for (const QString &id : selected_) ids.append(id);
        setPending(true);
        showError({});
        emit sendRequested(roomId_, ids);
    });
    debounce_ = new QTimer(this);
    debounce_->setSingleShot(true);
    debounce_->setInterval(250);
    connect(search_, &QLineEdit::textChanged, debounce_, qOverload<>(&QTimer::start));
    connect(debounce_, &QTimer::timeout, this, &RoomInviteDialog::requestSearch);
    search_->setFocus();
}

void RoomInviteDialog::requestSearch()
{
    if (!pending_) emit searchRequested(roomId_, search_->text().trimmed());
}

void RoomInviteDialog::setResults(const QJsonArray &users)
{
    results_->clear();
    empty_->setVisible(users.isEmpty());
    empty_->setText(QStringLiteral("没有找到相关用户"));
    for (const QJsonValue &value : users) {
        const QJsonObject user = value.toObject();
        const QString id = user.value(QStringLiteral("user_id")).toString();
        if (id.isEmpty()) continue;
        const QString name = user.value(QStringLiteral("nickname")).toString();
        const bool member = user.value(QStringLiteral("member")).toBool();
        const bool invited = user.value(QStringLiteral("pending")).toBool();
        const bool online = user.value(QStringLiteral("online")).toBool();
        if (member || invited) selected_.remove(id);
        auto *item = new QListWidgetItem(results_);
        item->setSizeHint(QSize(0, 59));
        auto *row = new QFrame(results_);
        row->setObjectName(QStringLiteral("inviteCandidateCard"));
        row->setAttribute(Qt::WA_StyledBackground, true);
        auto refreshCard = [row](bool selected) {
            row->setStyleSheet(selected
                ? QStringLiteral("QFrame#inviteCandidateCard { background:#f0e9fc; border-radius:10px; }")
                : QStringLiteral("QFrame#inviteCandidateCard { background:#f8f5fe; border-radius:10px; }"));
        };
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(7, 4, 7, 4);
        layout->setSpacing(11);
        auto *check = new QCheckBox(row);
        check->setObjectName(QStringLiteral("inviteUserCheck"));
        check->setProperty("userId", id);
        check->setProperty("eligible", !member && !invited);
        check->setChecked(selected_.contains(id));
        check->setEnabled(!member && !invited && !pending_);
        refreshCard(check->isChecked());
        layout->addWidget(check);
        layout->addWidget(avatar(name, row));
        auto *labels = new QVBoxLayout;
        labels->setSpacing(2);
        auto *nickname = new QLabel(name, row);
        nickname->setTextFormat(Qt::PlainText);
        nickname->setStyleSheet(QStringLiteral("color:#292342; font-weight:600;"));
        labels->addWidget(nickname);
        auto *status = new QLabel(member ? QStringLiteral("已在聊天室")
            : invited ? QStringLiteral("已有待处理邀请")
            : online ? QStringLiteral("● 在线") : QStringLiteral("● 离线"), row);
        status->setStyleSheet(online && !member && !invited
            ? QStringLiteral("color:#31a96e; font-size:12px;")
            : QStringLiteral("color:#958ba7; font-size:12px;"));
        labels->addWidget(status);
        layout->addLayout(labels, 1);
        results_->setItemWidget(item, row);
        connect(check, &QCheckBox::toggled, this, [this, id, refreshCard](bool checked) {
            if (checked) selected_.insert(id);
            else selected_.remove(id);
            refreshCard(checked);
            updateSelection();
        });
    }
    updateSelection();
}

void RoomInviteDialog::updateSelection()
{
    selection_->setText(QStringLiteral("已选择 %1 人").arg(selected_.size()));
    send_->setEnabled(!pending_ && !selected_.isEmpty());
}

void RoomInviteDialog::setPending(bool pending)
{
    pending_ = pending;
    search_->setEnabled(!pending);
    for (QCheckBox *check : results_->findChildren<QCheckBox *>())
        check->setEnabled(!pending && check->property("eligible").toBool());
    send_->setText(pending ? QStringLiteral("正在发送…") : QStringLiteral("发送邀请"));
    updateSelection();
}

void RoomInviteDialog::showError(const QString &message)
{
    message_->setStyleSheet(QStringLiteral("color:#b34c68; font-size:12px;"));
    message_->setText(message);
    message_->setVisible(!message.isEmpty());
}

void RoomInviteDialog::showSuccess(int count)
{
    selected_.clear();
    setPending(false);
    message_->setStyleSheet(QStringLiteral("color:#229968; font-size:12px;"));
    message_->setText(QStringLiteral("已向 %1 位用户发送加入邀请。查看结果可稍后重新搜索。 ").arg(count));
    message_->show();
    requestSearch();
}

RoomInviteInboxDialog::RoomInviteInboxDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("聊天室邀请"));
    setWindowModality(Qt::WindowModal);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(470, 440);
    setMinimumWidth(420);
    setStyleSheet(QString::fromLatin1(dialogStyle));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 22);
    root->setSpacing(12);
    auto *title = new QLabel(QStringLiteral("聊天室邀请"), this);
    title->setStyleSheet(QStringLiteral("font-size:19px; font-weight:700;"));
    root->addWidget(title);
    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("pendingInviteList"));
    list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    root->addWidget(list_, 1);
    empty_ = new QLabel(QStringLiteral("暂无待处理邀请"), this);
    empty_->setAlignment(Qt::AlignCenter);
    empty_->setStyleSheet(QStringLiteral("color:#827799; padding:12px;"));
    root->addWidget(empty_);
    error_ = new QLabel(this);
    error_->setWordWrap(true);
    error_->setStyleSheet(QStringLiteral("color:#b34c68; font-size:12px;"));
    error_->hide();
    root->addWidget(error_);
    auto *close = new QPushButton(QStringLiteral("关闭"), this);
    close->setObjectName(QStringLiteral("secondary"));
    root->addWidget(close, 0, Qt::AlignRight);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
}

void RoomInviteInboxDialog::setInvites(const QJsonArray &invites)
{
    list_->clear();
    empty_->setVisible(invites.isEmpty());
    for (const QJsonValue &value : invites) {
        const QJsonObject invite = value.toObject();
        const QString id = invite.value(QStringLiteral("invite_id")).toString();
        if (id.isEmpty()) continue;
        auto *item = new QListWidgetItem(list_);
        item->setSizeHint(QSize(0, 130));
        auto *row = new QFrame(list_);
        row->setObjectName(QStringLiteral("pendingInviteCard"));
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setStyleSheet(QStringLiteral("QFrame#pendingInviteCard { background:#f5f0fe;"
            " border-radius:12px; }"));
        auto *content = new QVBoxLayout(row);
        content->setContentsMargins(10, 9, 10, 9);
        content->setSpacing(5);
        auto *headline = new QLabel(QStringLiteral("%1 邀请你加入聊天室")
            .arg(invite.value(QStringLiteral("inviter")).toString()), row);
        headline->setTextFormat(Qt::PlainText);
        headline->setStyleSheet(QStringLiteral("color:#292342; font-weight:600;"));
        content->addWidget(headline);
        auto *room = new QLabel(invite.value(QStringLiteral("room_name")).toString(), row);
        room->setTextFormat(Qt::PlainText);
        room->setStyleSheet(QStringLiteral("color:#7956d5; font-weight:600;"));
        content->addWidget(room);
        auto *description = new QLabel(invite.value(QStringLiteral("description")).toString(), row);
        description->setTextFormat(Qt::PlainText);
        description->setWordWrap(true);
        description->setStyleSheet(QStringLiteral("color:#827799; font-size:12px;"));
        content->addWidget(description);
        auto *buttons = new QHBoxLayout;
        buttons->addStretch();
        auto *decline = new QPushButton(QStringLiteral("拒绝"), row);
        decline->setObjectName(QStringLiteral("secondary"));
        decline->setEnabled(!pending_);
        auto *accept = new QPushButton(QStringLiteral("接受"), row);
        accept->setObjectName(QStringLiteral("primary"));
        accept->setEnabled(!pending_);
        buttons->addWidget(decline);
        buttons->addWidget(accept);
        content->addLayout(buttons);
        list_->setItemWidget(item, row);
        connect(accept, &QPushButton::clicked, this, [this, id]() { emit decisionRequested(id, true); });
        connect(decline, &QPushButton::clicked, this, [this, id]() { emit decisionRequested(id, false); });
    }
}

void RoomInviteInboxDialog::setPending(bool pending)
{
    pending_ = pending;
    for (QPushButton *button : list_->findChildren<QPushButton *>()) button->setEnabled(!pending);
}

void RoomInviteInboxDialog::showError(const QString &message)
{
    error_->setText(message);
    error_->setVisible(!message.isEmpty());
}
