/********************************************************************************
** Form generated from reading UI file 'logindialog.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_LOGIN_CHECK_H
#define UI_LOGIN_CHECK_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_LoginDialog
{
public:
    QVBoxLayout *rootLayout;
    QSpacerItem *topSpacer;
    QLabel *titleLabel;
    QLabel *subtitleLabel;
    QSpacerItem *headingGap;
    QWidget *loginCard;
    QVBoxLayout *cardLayout;
    QHBoxLayout *serverRow;
    QPushButton *serverToggleButton;
    QWidget *serverSettings;
    QFormLayout *formLayout;
    QLineEdit *usernameEdit;
    QHBoxLayout *passwordRow;
    QLineEdit *passwordEdit;
    QPushButton *showPasswordButton;
    QHBoxLayout *captchaLayout;
    QLineEdit *captchaEdit;
    QLabel *captchaCodeLabel;
    QPushButton *refreshCaptchaButton;
    QHBoxLayout *optionsRow;
    QCheckBox *rememberAccountCheck;
    QSpacerItem *optionsStretch;
    QPushButton *cancelButton;
    QPushButton *loginButton;
    QLabel *hintLabel;
    QSpacerItem *bottomSpacer;

    void setupUi(QDialog *LoginDialog)
    {
        if (LoginDialog->objectName().isEmpty())
            LoginDialog->setObjectName("LoginDialog");
        LoginDialog->resize(760, 460);
        LoginDialog->setMinimumSize(QSize(600, 430));
        LoginDialog->setStyleSheet(QString::fromUtf8("\n"
"QWidget#loginCard { background-color: rgba(255,255,255,218); border: 1px solid rgba(255,255,255,230); border-radius: 18px; }\n"
"QLabel { color: #28233a; font-family: \"Microsoft YaHei UI\"; font-size: 13px; }\n"
"QLabel#titleLabel { color: #201c31; font-size: 29px; font-weight: 700; }\n"
"QLabel#subtitleLabel { color: #68617b; font-size: 12px; }\n"
"QLabel#hintLabel { color: #766d8a; font-size: 12px; }\n"
"QLabel#captchaCodeLabel { color: #28233a; background-color: #eeeafa; border-radius: 8px; font: 700 16px Consolas; letter-spacing: 2px; }\n"
"QLineEdit { color: #28233a; background-color: rgba(255,255,255,235); border: 1px solid #e2dced; border-radius: 9px; padding: 7px 10px; font-size: 13px; font-family: \"Microsoft YaHei UI\"; selection-background-color: #8067d8; }\n"
"QLineEdit:focus { border: 1px solid #8067d8; }\n"
"QPushButton { color: #514673; background-color: rgba(255,255,255,160); border: 1px solid #e2dced; border-radius: 8px; padding: 6px; font-size: 13px; font-family: \"Microsoft YaHei UI\";"
                        " }\n"
"QPushButton:hover { background-color: #eee8fc; }\n"
"QPushButton#loginButton { color: white; background-color: #8067d8; border: 1px solid #8067d8; border-radius: 10px; font-size: 15px; font-weight: 700; }\n"
"QPushButton#loginButton:hover { background-color: #7056cb; }\n"
"QPushButton#loginButton:pressed { background-color: #6145bc; }\n"
"QCheckBox { color: #514b62; font-size: 12px; font-family: \"Microsoft YaHei UI\"; }\n"
"  "));
        rootLayout = new QVBoxLayout(LoginDialog);
        rootLayout->setObjectName("rootLayout");
        rootLayout->setContentsMargins(24, 16, 24, 16);
        topSpacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        rootLayout->addItem(topSpacer);

        titleLabel = new QLabel(LoginDialog);
        titleLabel->setObjectName("titleLabel");
        titleLabel->setAlignment(Qt::AlignCenter);

        rootLayout->addWidget(titleLabel);

        subtitleLabel = new QLabel(LoginDialog);
        subtitleLabel->setObjectName("subtitleLabel");
        subtitleLabel->setAlignment(Qt::AlignCenter);

        rootLayout->addWidget(subtitleLabel);

        headingGap = new QSpacerItem(1, 14, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        rootLayout->addItem(headingGap);

        loginCard = new QWidget(LoginDialog);
        loginCard->setObjectName("loginCard");
        loginCard->setMinimumSize(QSize(390, 0));
        loginCard->setMaximumSize(QSize(390, 16777215));
        cardLayout = new QVBoxLayout(loginCard);
        cardLayout->setSpacing(8);
        cardLayout->setObjectName("cardLayout");
        cardLayout->setContentsMargins(26, 16, 26, 16);
        serverRow = new QHBoxLayout();
        serverRow->setObjectName("serverRow");
        serverToggleButton = new QPushButton(loginCard);
        serverToggleButton->setObjectName("serverToggleButton");

        serverRow->addWidget(serverToggleButton);


        cardLayout->addLayout(serverRow);

        serverSettings = new QWidget(loginCard);
        serverSettings->setObjectName("serverSettings");
        formLayout = new QFormLayout(serverSettings);
        formLayout->setObjectName("formLayout");
        formLayout->setContentsMargins(0, 0, 0, 0);

        cardLayout->addWidget(serverSettings);

        usernameEdit = new QLineEdit(loginCard);
        usernameEdit->setObjectName("usernameEdit");
        usernameEdit->setMinimumSize(QSize(0, 36));

        cardLayout->addWidget(usernameEdit);

        passwordRow = new QHBoxLayout();
        passwordRow->setObjectName("passwordRow");
        passwordEdit = new QLineEdit(loginCard);
        passwordEdit->setObjectName("passwordEdit");
        passwordEdit->setMinimumSize(QSize(0, 36));
        passwordEdit->setEchoMode(QLineEdit::Password);

        passwordRow->addWidget(passwordEdit);

        showPasswordButton = new QPushButton(loginCard);
        showPasswordButton->setObjectName("showPasswordButton");

        passwordRow->addWidget(showPasswordButton);


        cardLayout->addLayout(passwordRow);

        captchaLayout = new QHBoxLayout();
        captchaLayout->setObjectName("captchaLayout");
        captchaEdit = new QLineEdit(loginCard);
        captchaEdit->setObjectName("captchaEdit");
        captchaEdit->setMinimumSize(QSize(0, 36));

        captchaLayout->addWidget(captchaEdit);

        captchaCodeLabel = new QLabel(loginCard);
        captchaCodeLabel->setObjectName("captchaCodeLabel");
        captchaCodeLabel->setMinimumSize(QSize(88, 36));
        captchaCodeLabel->setAlignment(Qt::AlignCenter);

        captchaLayout->addWidget(captchaCodeLabel);

        refreshCaptchaButton = new QPushButton(loginCard);
        refreshCaptchaButton->setObjectName("refreshCaptchaButton");

        captchaLayout->addWidget(refreshCaptchaButton);


        cardLayout->addLayout(captchaLayout);

        optionsRow = new QHBoxLayout();
        optionsRow->setObjectName("optionsRow");
        rememberAccountCheck = new QCheckBox(loginCard);
        rememberAccountCheck->setObjectName("rememberAccountCheck");

        optionsRow->addWidget(rememberAccountCheck);

        optionsStretch = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        optionsRow->addItem(optionsStretch);

        cancelButton = new QPushButton(loginCard);
        cancelButton->setObjectName("cancelButton");

        optionsRow->addWidget(cancelButton);


        cardLayout->addLayout(optionsRow);

        loginButton = new QPushButton(loginCard);
        loginButton->setObjectName("loginButton");
        loginButton->setMinimumSize(QSize(0, 40));

        cardLayout->addWidget(loginButton);

        hintLabel = new QLabel(loginCard);
        hintLabel->setObjectName("hintLabel");
        hintLabel->setAlignment(Qt::AlignCenter);

        cardLayout->addWidget(hintLabel);


        rootLayout->addWidget(loginCard, 0, Qt::AlignHCenter);

        bottomSpacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        rootLayout->addItem(bottomSpacer);


        retranslateUi(LoginDialog);

        QMetaObject::connectSlotsByName(LoginDialog);
    } // setupUi

    void retranslateUi(QDialog *LoginDialog)
    {
        LoginDialog->setWindowTitle(QCoreApplication::translate("LoginDialog", "VChat - \347\231\273\345\275\225", nullptr));
        titleLabel->setText(QCoreApplication::translate("LoginDialog", "\360\237\222\254 VChat", nullptr));
        subtitleLabel->setText(QCoreApplication::translate("LoginDialog", "\345\256\211\345\205\250 \302\267 \347\250\263\345\256\232 \302\267 \350\275\273\351\207\217\347\232\204\345\215\263\346\227\266\351\200\232\350\256\257", nullptr));
        serverToggleButton->setText(QCoreApplication::translate("LoginDialog", "\346\234\215\345\212\241\345\231\250  127.0.0.1:8080   \342\214\204", nullptr));
#if QT_CONFIG(tooltip)
        serverToggleButton->setToolTip(QCoreApplication::translate("LoginDialog", "\345\261\225\345\274\200\346\234\215\345\212\241\345\231\250\350\256\276\347\275\256", nullptr));
#endif // QT_CONFIG(tooltip)
        usernameEdit->setPlaceholderText(QCoreApplication::translate("LoginDialog", "\350\257\267\350\276\223\345\205\245\350\264\246\345\217\267", nullptr));
        passwordEdit->setPlaceholderText(QCoreApplication::translate("LoginDialog", "\350\257\267\350\276\223\345\205\245\345\257\206\347\240\201", nullptr));
        showPasswordButton->setText(QCoreApplication::translate("LoginDialog", "\346\230\276\347\244\272", nullptr));
#if QT_CONFIG(tooltip)
        showPasswordButton->setToolTip(QCoreApplication::translate("LoginDialog", "\346\230\276\347\244\272\345\257\206\347\240\201", nullptr));
#endif // QT_CONFIG(tooltip)
        captchaEdit->setPlaceholderText(QCoreApplication::translate("LoginDialog", "\350\257\267\350\276\223\345\205\245\351\252\214\350\257\201\347\240\201", nullptr));
        captchaCodeLabel->setText(QCoreApplication::translate("LoginDialog", "ABCDE", nullptr));
        refreshCaptchaButton->setText(QCoreApplication::translate("LoginDialog", "\342\237\263", nullptr));
#if QT_CONFIG(tooltip)
        refreshCaptchaButton->setToolTip(QCoreApplication::translate("LoginDialog", "\345\210\267\346\226\260\351\252\214\350\257\201\347\240\201", nullptr));
#endif // QT_CONFIG(tooltip)
        rememberAccountCheck->setText(QCoreApplication::translate("LoginDialog", "\350\256\260\344\275\217\350\264\246\345\217\267", nullptr));
        cancelButton->setText(QCoreApplication::translate("LoginDialog", "\345\217\226\346\266\210", nullptr));
        loginButton->setText(QCoreApplication::translate("LoginDialog", "\347\231\273\345\275\225", nullptr));
        hintLabel->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class LoginDialog: public Ui_LoginDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_LOGIN_CHECK_H
