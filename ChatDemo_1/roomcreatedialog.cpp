#include "roomcreatedialog.h"
#include "roomdialogchrome.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QShowEvent>
#include <QVBoxLayout>

RoomCreateDialog::RoomCreateDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("创建聊天室"));
    setWindowModality(Qt::WindowModal);
    setAttribute(Qt::WA_DeleteOnClose);
    setFixedWidth(500);
    const QString style = QStringLiteral(
        "QDialog { background:transparent; color:#252044; font-family:'Microsoft YaHei UI'; font-size:13px; }"
        "QFrame#roomPanel { background:#fdfbff; border:1px solid #e9e0f7; border-radius:19px; }"
        "QLabel { color:#51466d; background:transparent; }"
        "QLabel#formCaption { color:#332951; font-size:13px; font-weight:600; }"
        "QLineEdit,QPlainTextEdit { background:white; border:1px solid #e4ddf3; border-radius:11px;"
        " padding:10px 13px; color:#292342; selection-background-color:#d9c8f9; }"
        "QLineEdit:focus,QPlainTextEdit:focus { border:1px solid #8064d9; }"
        "QRadioButton { color:#51466d; spacing:9px; padding:4px 0; }"
        "QRadioButton::indicator { width:17px; height:17px; }"
        "QRadioButton::indicator:checked { background:#8064d9; border:3px solid #e6dcfa; border-radius:9px; }"
        "QRadioButton::indicator:unchecked { background:white; border:1px solid #bcb0d5; border-radius:9px; }"
        "QPushButton { min-height:38px; border-radius:10px; padding:0 18px; }"
        "QPushButton#createRoomSubmit { background:#8064d9; color:white; font-weight:600; border:0; }"
        "QPushButton#createRoomSubmit:hover { background:#9175e6; }"
        "QPushButton#createRoomSubmit:pressed { background:#6f52c5; }"
        "QPushButton#createRoomSubmit:disabled { background:#c9bce7; }"
        "QPushButton#cancel { background:white; color:#695d80; border:1px solid #e4ddf3; }"
        "QPushButton#cancel:hover { background:#f3eefb; }"
        "QToolButton#dialogClose { background:transparent; border:0; border-radius:8px; }"
        "QToolButton#dialogClose:hover { background:#f1ecfc; }");

    auto *layout = RoomDialogChrome::makeShell(this, QStringLiteral("创建聊天室"),
        QStringLiteral("members.svg"), style).layout;
    layout->setSpacing(14);
    auto *subtitle = new QLabel(QStringLiteral("为你的团队创建一个专属协作空间。"), this);
    subtitle->setStyleSheet(QStringLiteral("color:#827799; font-size:13px;"));
    layout->addWidget(subtitle);
    layout->addSpacing(6);

    auto *nameLabel = new QLabel(QStringLiteral("聊天室名称 *"), this);
    nameLabel->setObjectName(QStringLiteral("formCaption"));
    name_ = new QLineEdit(this);
    name_->setObjectName(QStringLiteral("roomNameInput"));
    name_->setMaxLength(100); // Matches RoomStore::create.
    name_->setPlaceholderText(QStringLiteral("例如：C++ 项目开发组"));
    name_->setFixedHeight(44);
    auto *nameField = new QVBoxLayout;
    nameField->setSpacing(6);
    nameField->addWidget(nameLabel);
    nameField->addWidget(name_);
    layout->addLayout(nameField);

    auto *descriptionLabel = new QLabel(QStringLiteral("聊天室简介（可选）"), this);
    descriptionLabel->setObjectName(QStringLiteral("formCaption"));
    description_ = new QPlainTextEdit(this);
    description_->setObjectName(QStringLiteral("roomDescriptionInput"));
    description_->setPlaceholderText(QStringLiteral("简单介绍这个聊天室的任务或用途"));
    description_->setFixedHeight(82);
    auto *descriptionField = new QVBoxLayout;
    descriptionField->setSpacing(6);
    descriptionField->addWidget(descriptionLabel);
    descriptionField->addWidget(description_);
    layout->addLayout(descriptionField);

    auto *typeLabel = new QLabel(QStringLiteral("加入方式"), this);
    typeLabel->setObjectName(QStringLiteral("formCaption"));
    auto *typeField = new QVBoxLayout;
    typeField->setSpacing(5);
    typeField->addWidget(typeLabel);
    invite_ = new QRadioButton(QStringLiteral("邀请加入"), this);
    invite_->setChecked(true);
    typeField->addWidget(invite_);
    auto *inviteDetail = new QLabel(QStringLiteral("只有获得邀请的用户才能加入，适合私人团队和项目协作。"), this);
    inviteDetail->setWordWrap(true);
    inviteDetail->setStyleSheet(QStringLiteral("color:#827799; font-size:12px;"));
    auto *inviteDescription = new QHBoxLayout;
    inviteDescription->setContentsMargins(26, 0, 0, 0);
    inviteDescription->addWidget(inviteDetail);
    typeField->addLayout(inviteDescription);
    auto *publicOption = new QRadioButton(QStringLiteral("公开聊天室"), this);
    publicOption->setObjectName(QStringLiteral("publicRoomOption"));
    typeField->addWidget(publicOption);
    auto *publicDetail = new QLabel(QStringLiteral("其他用户可通过发现功能搜索并加入。"), this);
    publicDetail->setStyleSheet(QStringLiteral("color:#827799; font-size:12px;"));
    auto *publicDescription = new QHBoxLayout;
    publicDescription->setContentsMargins(26, 0, 0, 0);
    publicDescription->addWidget(publicDetail);
    typeField->addLayout(publicDescription);
    layout->addLayout(typeField);

    error_ = new QLabel(this);
    error_->setWordWrap(true);
    error_->setStyleSheet(QStringLiteral("color:#b34c68; font-size:12px;"));
    error_->hide();
    layout->addWidget(error_);
    layout->addSpacing(6);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    cancel_ = new QPushButton(QStringLiteral("取消"), this);
    cancel_->setObjectName(QStringLiteral("cancel"));
    submit_ = new QPushButton(QStringLiteral("创建聊天室"), this);
    submit_->setObjectName(QStringLiteral("createRoomSubmit"));
    cancel_->setFixedSize(105, 42);
    submit_->setFixedSize(120, 42);
    buttons->addWidget(cancel_);
    buttons->addWidget(submit_);
    layout->addLayout(buttons);
    connect(cancel_, &QPushButton::clicked, this, &QDialog::reject);
    connect(submit_, &QPushButton::clicked, this, [this]() {
        const QString name = name_->text().trimmed();
        const QString description = description_->toPlainText().trimmed();
        if (name.isEmpty()) { showError(QStringLiteral("请输入聊天室名称。")); name_->setFocus(); return; }
        if (description.size() > 500) { showError(QStringLiteral("聊天室简介不能超过 500 字。")); description_->setFocus(); return; }
        showError({});
        setPending(true);
        emit createSubmitted(name, description, invite_->isChecked()
            ? QStringLiteral("invite_only") : QStringLiteral("public"));
    });
    name_->setFocus();
    adjustSize();
    resize(width(), this->layout()->sizeHint().height());
}

void RoomCreateDialog::setPending(bool pending)
{
    pending_ = pending;
    name_->setEnabled(!pending);
    description_->setEnabled(!pending);
    invite_->setEnabled(!pending);
    for (QRadioButton *radio : findChildren<QRadioButton *>()) radio->setEnabled(!pending);
    submit_->setEnabled(!pending);
    submit_->setText(pending ? QStringLiteral("正在创建…") : QStringLiteral("创建聊天室"));
    cancel_->setEnabled(!pending);
}

void RoomCreateDialog::showError(const QString &message)
{
    error_->setText(message);
    error_->setVisible(!message.isEmpty());
    layout()->activate();
    resize(width(), layout()->sizeHint().height());
    if (isVisible()) RoomDialogChrome::centerOnParent(this);
}

void RoomCreateDialog::reject()
{
    if (!pending_) QDialog::reject();
}

void RoomCreateDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    layout()->activate();
    resize(width(), layout()->sizeHint().height());
    RoomDialogChrome::centerOnParent(this);
}
