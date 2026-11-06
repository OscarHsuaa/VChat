/********************************************************************************
** Form generated from reading UI file 'logindialog.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_LOGINDIALOG_H
#define UI_LOGINDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_LoginDialog
{
public:
    QVBoxLayout *rootLayout;
    QSpacerItem *topSpacer;
    QHBoxLayout *brandRow;
    QSpacerItem *brandLeft;
    QLabel *logoLabel;
    QLabel *titleLabel;
    QSpacerItem *brandRight;
    QLabel *subtitleLabel;
    QSpacerItem *headingGap;
    QWidget *loginCard;
    QVBoxLayout *cardLayout;
    QFrame *serverSummaryRow;
    QHBoxLayout *serverRow;
    QPushButton *serverToggleButton;
    QSpacerItem *serverRowSpacer;
    QLabel *serverAddressLabel;
    QToolButton *serverChevronButton;
    QWidget *serverSettings;
    QFormLayout *formLayout;
    QFrame *userField;
    QHBoxLayout *userInputRow;
    QLabel *userIcon;
    QLineEdit *usernameEdit;
    QFrame *passwordField;
    QHBoxLayout *passwordInputRow;
    QLabel *passwordIcon;
    QLineEdit *passwordEdit;
    QToolButton *eyeButton;
    QHBoxLayout *captchaLayout;
    QFrame *captchaField;
    QHBoxLayout *captchaInputRow;
    QLabel *captchaIcon;
    QLineEdit *captchaEdit;
    QLabel *captchaCodeLabel;
    QPushButton *refreshCaptchaButton;
    QHBoxLayout *optionsRow;
    QCheckBox *rememberAccountCheck;
    QSpacerItem *optionsStretch;
    QPushButton *cancelButton;
    QPushButton *loginButton;
    QSpacerItem *footerGap;
    QLabel *registrationInfoLabel;
    QSpacerItem *bottomSpacer;

    void setupUi(QDialog *LoginDialog)
    {
        if (LoginDialog->objectName().isEmpty())
            LoginDialog->setObjectName("LoginDialog");
        LoginDialog->resize(960, 640);
        LoginDialog->setMinimumSize(QSize(680, 540));
        LoginDialog->setStyleSheet(QString::fromUtf8("\n"
"QWidget#loginCard { background-color: rgba(255,255,255,225); border: 1px solid rgba(255,255,255,245); border-radius: 20px; }\n"
"QWidget#serverSettings QLabel { color: #28233a; }\n"
"QLabel#titleLabel { color: #201c31; }\n"
"QLabel#subtitleLabel { color: #625b74; }\n"
"QLabel#hintLabel { color: #766d8a; font-size: 12px; }\n"
"QLabel#captchaCodeLabel { color: #28233a; background-color: #eeeafa; border-radius: 9px; font: 700 17px Consolas; letter-spacing: 2px; }\n"
"QLineEdit { color: #28233a; background-color: rgba(255,255,255,244); border: 1px solid #ddd8ee; border-radius: 11px; padding: 8px 14px; selection-background-color: #8067d8; }\n"
"QLineEdit:focus { border: 1px solid #a990e7; background-color: #ffffff; }\n"
"QFrame#userField, QFrame#passwordField, QFrame#captchaField { background-color: rgba(255,255,255,244); border: 1px solid #ddd8ee; border-radius: 11px; }\n"
"QFrame#userField[focused=\"true\"], QFrame#passwordField[focused=\"true\"], QFrame#captchaField[focused=\"true\"] { border: 1px solid #a9"
                        "90e7; background-color: #ffffff; }\n"
"QLineEdit#usernameEdit, QLineEdit#passwordEdit, QLineEdit#captchaEdit { background: transparent; border: none; border-radius: 0; padding: 0; }\n"
"QToolButton#eyeButton { background: transparent; border: none; padding: 0; }\n"
"QToolButton#eyeButton:hover { background: transparent; }\n"
"QPushButton#refreshCaptchaButton { color: #514673; background-color: rgba(255,255,255,190); border: 1px solid #e5e0ed; border-radius: 9px; padding: 6px; }\n"
"QPushButton#refreshCaptchaButton:hover { background-color: #eee8fc; }\n"
"QFrame#serverSummaryRow { background-color: rgba(255,255,255,210); border: 1px solid #e3dff0; border-radius: 11px; }\n"
"QPushButton#serverToggleButton { color: #5f587a; background: transparent; border: none; text-align: left; padding: 0; }\n"
"QPushButton#serverToggleButton:hover { color: #6e57cf; background: transparent; }\n"
"QLabel#serverAddressLabel { color: #756d91; }\n"
"QLabel#registrationInfoLabel { color: #756d91; }\n"
"QToolButton#serverChevronButto"
                        "n { color: #716697; background: transparent; border: none; }\n"
"QToolButton#serverChevronButton:hover { color: #6247c3; }\n"
"QPushButton#cancelButton { color: #786e8d; background: transparent; border: none; }\n"
"QPushButton#cancelButton:hover { color: #604cb0; }\n"
"QPushButton#loginButton { color: white; background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #8d6df0,stop:1 #7866df); border: none; border-radius: 12px; }\n"
"QPushButton#loginButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #997bf4,stop:1 #8371e8); }\n"
"QPushButton#loginButton:pressed { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #785cd8,stop:1 #6655c7); }\n"
"QPushButton#loginButton:disabled { color: #f4f0fa; background-color: #c9bee7; border-color: #c9bee7; }\n"
"QCheckBox { color: #514b62; }\n"
"  "));
        rootLayout = new QVBoxLayout(LoginDialog);
        rootLayout->setObjectName("rootLayout");
        rootLayout->setContentsMargins(24, 16, 24, 16);
        topSpacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        rootLayout->addItem(topSpacer);

        brandRow = new QHBoxLayout();
        brandRow->setObjectName("brandRow");
        brandLeft = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        brandRow->addItem(brandLeft);

        logoLabel = new QLabel(LoginDialog);
        logoLabel->setObjectName("logoLabel");
        logoLabel->setMinimumSize(QSize(44, 44));

        brandRow->addWidget(logoLabel);

        titleLabel = new QLabel(LoginDialog);
        titleLabel->setObjectName("titleLabel");

        brandRow->addWidget(titleLabel);

        brandRight = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        brandRow->addItem(brandRight);


        rootLayout->addLayout(brandRow);

        subtitleLabel = new QLabel(LoginDialog);
        subtitleLabel->setObjectName("subtitleLabel");
        subtitleLabel->setAlignment(Qt::AlignCenter);

        rootLayout->addWidget(subtitleLabel);

        headingGap = new QSpacerItem(1, 20, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        rootLayout->addItem(headingGap);

        loginCard = new QWidget(LoginDialog);
        loginCard->setObjectName("loginCard");
        loginCard->setMinimumSize(QSize(450, 0));
        loginCard->setMaximumSize(QSize(450, 16777215));
        cardLayout = new QVBoxLayout(loginCard);
        cardLayout->setSpacing(11);
        cardLayout->setObjectName("cardLayout");
        cardLayout->setContentsMargins(30, 23, 30, 23);
        serverSummaryRow = new QFrame(loginCard);
        serverSummaryRow->setObjectName("serverSummaryRow");
        serverSummaryRow->setMinimumSize(QSize(0, 46));
        serverRow = new QHBoxLayout(serverSummaryRow);
        serverRow->setSpacing(8);
        serverRow->setObjectName("serverRow");
        serverRow->setContentsMargins(12, 0, 10, 0);
        serverToggleButton = new QPushButton(serverSummaryRow);
        serverToggleButton->setObjectName("serverToggleButton");

        serverRow->addWidget(serverToggleButton);

        serverRowSpacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        serverRow->addItem(serverRowSpacer);

        serverAddressLabel = new QLabel(serverSummaryRow);
        serverAddressLabel->setObjectName("serverAddressLabel");

        serverRow->addWidget(serverAddressLabel);

        serverChevronButton = new QToolButton(serverSummaryRow);
        serverChevronButton->setObjectName("serverChevronButton");
        serverChevronButton->setArrowType(Qt::DownArrow);

        serverRow->addWidget(serverChevronButton);


        cardLayout->addWidget(serverSummaryRow);

        serverSettings = new QWidget(loginCard);
        serverSettings->setObjectName("serverSettings");
        formLayout = new QFormLayout(serverSettings);
        formLayout->setObjectName("formLayout");
        formLayout->setContentsMargins(0, 0, 0, 0);

        cardLayout->addWidget(serverSettings);

        userField = new QFrame(loginCard);
        userField->setObjectName("userField");
        userField->setMinimumSize(QSize(0, 46));
        userInputRow = new QHBoxLayout(userField);
        userInputRow->setSpacing(10);
        userInputRow->setObjectName("userInputRow");
        userInputRow->setContentsMargins(19, 0, 12, 0);
        userIcon = new QLabel(userField);
        userIcon->setObjectName("userIcon");
        userIcon->setMinimumSize(QSize(18, 18));
        userIcon->setMaximumSize(QSize(18, 18));

        userInputRow->addWidget(userIcon);

        usernameEdit = new QLineEdit(userField);
        usernameEdit->setObjectName("usernameEdit");

        userInputRow->addWidget(usernameEdit);


        cardLayout->addWidget(userField);

        passwordField = new QFrame(loginCard);
        passwordField->setObjectName("passwordField");
        passwordField->setMinimumSize(QSize(0, 46));
        passwordInputRow = new QHBoxLayout(passwordField);
        passwordInputRow->setSpacing(10);
        passwordInputRow->setObjectName("passwordInputRow");
        passwordInputRow->setContentsMargins(19, 0, 10, 0);
        passwordIcon = new QLabel(passwordField);
        passwordIcon->setObjectName("passwordIcon");
        passwordIcon->setMinimumSize(QSize(18, 18));
        passwordIcon->setMaximumSize(QSize(18, 18));

        passwordInputRow->addWidget(passwordIcon);

        passwordEdit = new QLineEdit(passwordField);
        passwordEdit->setObjectName("passwordEdit");
        passwordEdit->setEchoMode(QLineEdit::Password);

        passwordInputRow->addWidget(passwordEdit);

        eyeButton = new QToolButton(passwordField);
        eyeButton->setObjectName("eyeButton");
        eyeButton->setMinimumSize(QSize(28, 28));
        eyeButton->setMaximumSize(QSize(28, 28));

        passwordInputRow->addWidget(eyeButton);


        cardLayout->addWidget(passwordField);

        captchaLayout = new QHBoxLayout();
        captchaLayout->setSpacing(9);
        captchaLayout->setObjectName("captchaLayout");
        captchaField = new QFrame(loginCard);
        captchaField->setObjectName("captchaField");
        captchaField->setMinimumSize(QSize(0, 46));
        captchaInputRow = new QHBoxLayout(captchaField);
        captchaInputRow->setSpacing(10);
        captchaInputRow->setObjectName("captchaInputRow");
        captchaInputRow->setContentsMargins(19, 0, 10, 0);
        captchaIcon = new QLabel(captchaField);
        captchaIcon->setObjectName("captchaIcon");
        captchaIcon->setMinimumSize(QSize(18, 18));
        captchaIcon->setMaximumSize(QSize(18, 18));

        captchaInputRow->addWidget(captchaIcon);

        captchaEdit = new QLineEdit(captchaField);
        captchaEdit->setObjectName("captchaEdit");

        captchaInputRow->addWidget(captchaEdit);


        captchaLayout->addWidget(captchaField);

        captchaCodeLabel = new QLabel(loginCard);
        captchaCodeLabel->setObjectName("captchaCodeLabel");
        captchaCodeLabel->setMinimumSize(QSize(102, 46));
        captchaCodeLabel->setAlignment(Qt::AlignCenter);

        captchaLayout->addWidget(captchaCodeLabel);

        refreshCaptchaButton = new QPushButton(loginCard);
        refreshCaptchaButton->setObjectName("refreshCaptchaButton");
        refreshCaptchaButton->setMinimumSize(QSize(46, 46));
        refreshCaptchaButton->setMaximumSize(QSize(46, 46));

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
        loginButton->setMinimumSize(QSize(0, 50));

        cardLayout->addWidget(loginButton);

        footerGap = new QSpacerItem(1, 7, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        cardLayout->addItem(footerGap);

        registrationInfoLabel = new QLabel(loginCard);
        registrationInfoLabel->setObjectName("registrationInfoLabel");
        registrationInfoLabel->setAlignment(Qt::AlignCenter);

        cardLayout->addWidget(registrationInfoLabel);


        rootLayout->addWidget(loginCard, 0, Qt::AlignHCenter);

        bottomSpacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        rootLayout->addItem(bottomSpacer);


        retranslateUi(LoginDialog);

        QMetaObject::connectSlotsByName(LoginDialog);
    } // setupUi

    void retranslateUi(QDialog *LoginDialog)
    {
        LoginDialog->setWindowTitle(QCoreApplication::translate("LoginDialog", "VChat - \347\231\273\345\275\225", nullptr));
        titleLabel->setText(QCoreApplication::translate("LoginDialog", "VChat", nullptr));
        subtitleLabel->setText(QCoreApplication::translate("LoginDialog", "\345\256\211\345\205\250 \302\267 \347\250\263\345\256\232 \302\267 \350\275\273\351\207\217\347\232\204\345\215\263\346\227\266\351\200\232\350\256\257", nullptr));
        serverToggleButton->setText(QCoreApplication::translate("LoginDialog", "\346\234\215\345\212\241\345\231\250\350\256\276\347\275\256", nullptr));
#if QT_CONFIG(tooltip)
        serverToggleButton->setToolTip(QCoreApplication::translate("LoginDialog", "\345\261\225\345\274\200\346\234\215\345\212\241\345\231\250\350\256\276\347\275\256", nullptr));
#endif // QT_CONFIG(tooltip)
        serverAddressLabel->setText(QCoreApplication::translate("LoginDialog", "127.0.0.1:8080", nullptr));
#if QT_CONFIG(tooltip)
        serverChevronButton->setToolTip(QCoreApplication::translate("LoginDialog", "\345\261\225\345\274\200\346\234\215\345\212\241\345\231\250\350\256\276\347\275\256", nullptr));
#endif // QT_CONFIG(tooltip)
        usernameEdit->setPlaceholderText(QCoreApplication::translate("LoginDialog", "\350\257\267\350\276\223\345\205\245\350\264\246\345\217\267", nullptr));
        passwordEdit->setPlaceholderText(QCoreApplication::translate("LoginDialog", "\350\257\267\350\276\223\345\205\245\345\257\206\347\240\201", nullptr));
#if QT_CONFIG(tooltip)
        eyeButton->setToolTip(QCoreApplication::translate("LoginDialog", "\346\230\276\347\244\272\345\257\206\347\240\201", nullptr));
#endif // QT_CONFIG(tooltip)
        captchaEdit->setPlaceholderText(QCoreApplication::translate("LoginDialog", "\350\257\267\350\276\223\345\205\245\351\252\214\350\257\201\347\240\201", nullptr));
        captchaCodeLabel->setText(QCoreApplication::translate("LoginDialog", "ABCDE", nullptr));
        refreshCaptchaButton->setText(QString());
#if QT_CONFIG(tooltip)
        refreshCaptchaButton->setToolTip(QCoreApplication::translate("LoginDialog", "\345\210\267\346\226\260\351\252\214\350\257\201\347\240\201", nullptr));
#endif // QT_CONFIG(tooltip)
        rememberAccountCheck->setText(QCoreApplication::translate("LoginDialog", "\350\256\260\344\275\217\350\264\246\345\217\267", nullptr));
        cancelButton->setText(QCoreApplication::translate("LoginDialog", "\345\217\226\346\266\210", nullptr));
        loginButton->setText(QCoreApplication::translate("LoginDialog", "\347\231\273\345\275\225", nullptr));
        registrationInfoLabel->setText(QCoreApplication::translate("LoginDialog", "\350\277\230\346\262\241\346\234\211\350\264\246\345\217\267\357\274\237\350\257\267\350\201\224\347\263\273\347\256\241\347\220\206\345\221\230\345\274\200\351\200\232", nullptr));
    } // retranslateUi

};

namespace Ui {
    class LoginDialog: public Ui_LoginDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_LOGINDIALOG_H
