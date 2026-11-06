/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "mainwindow.h"
#include "mentioninput.h"

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QAction *actionConnect;
    QAction *actionDisconnect;
    QAction *actionExit;
    QAction *actionChangeNickname;
    QAction *actionAbout;
    QWidget *centralwidget;
    QVBoxLayout *rootLayout;
    HeaderWaveFrame *headerPanel;
    QHBoxLayout *headerLayout;
    QLabel *logoLabel;
    QVBoxLayout *brandLayout;
    QLabel *titleLabel;
    QLabel *subtitleLabel;
    QSpacerItem *headerSpacer;
    QPushButton *settingsButton;
    QPushButton *helpButton;
    QPushButton *exitButton;
    QHBoxLayout *bodyLayout;
    QFrame *chatPanel;
    QVBoxLayout *chatPanelLayout;
    QScrollArea *messageScroll;
    QWidget *messageContainer;
    QVBoxLayout *messageLayout;
    QSpacerItem *messageBottomSpacer;
    QFrame *composer;
    QHBoxLayout *inputLayout;
    MentionInput *messageInput;
    QPushButton *sendButton;
    QFrame *rightPanel;
    QVBoxLayout *rightLayout;
    QHBoxLayout *memberHeader;
    QLabel *memberIcon;
    QLabel *onlineTitle;
    QSpacerItem *memberSpacer;
    RoundedLabel *onlineBadge;
    QListWidget *userList;
    QFrame *infoDivider;
    QLabel *serverStatusLabel;
    QLabel *onlineCountLabel;
    QMenuBar *menubar;
    QMenu *menuFile;
    QMenu *menuSettings;
    QMenu *menuHelp;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(1200, 780);
        MainWindow->setMinimumSize(QSize(830, 560));
        actionConnect = new QAction(MainWindow);
        actionConnect->setObjectName("actionConnect");
        actionDisconnect = new QAction(MainWindow);
        actionDisconnect->setObjectName("actionDisconnect");
        actionExit = new QAction(MainWindow);
        actionExit->setObjectName("actionExit");
        actionChangeNickname = new QAction(MainWindow);
        actionChangeNickname->setObjectName("actionChangeNickname");
        actionAbout = new QAction(MainWindow);
        actionAbout->setObjectName("actionAbout");
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        centralwidget->setStyleSheet(QString::fromUtf8("\n"
"QWidget#centralwidget { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #fbf8ff,stop:0.55 #f8f5ff,stop:1 #f1ebfc); color: #252044; font-family: \"Microsoft YaHei UI\"; }\n"
"HeaderWaveFrame#headerPanel { background: transparent; border: none; }\n"
"QFrame#chatPanel, QFrame#rightPanel { background-color: #ffffff; border: 1px solid #eee8f8; border-radius: 18px; }\n"
"QLabel#titleLabel { color: #252044; font-size: 23px; font-weight: 700; }\n"
"QLabel#subtitleLabel { color: #79718f; font-size: 13px; }\n"
"QLabel#onlineTitle { color: #252044; font-size: 16px; font-weight: 600; }\n"
"QLabel#onlineBadge { color: #6c52bc; padding: 6px 12px; font-size: 12px; font-weight: 600; }\n"
"QLabel#serverStatusLabel, QLabel#onlineCountLabel { color: #756e8c; font-size: 12px; }\n"
"QScrollArea#messageScroll { background-color: transparent; border: none; }\n"
"QScrollBar:vertical { background: transparent; width: 8px; margin: 4px 1px; }\n"
"QScrollBar::handle:vertical { background: #dcd2f2; border-radius: 4px; min-heig"
                        "ht: 26px; }\n"
"QScrollBar::handle:vertical:hover { background: #baa9e7; }\n"
"QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }\n"
"QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }\n"
"QWidget#messageContainer { background-color: transparent; }\n"
"QListWidget#userList { background-color: transparent; border: none; outline: none; }\n"
"QListWidget#userList::item { background-color: transparent; border-radius: 12px; }\n"
"QListWidget#userList::item:hover { background-color: #f7f4fc; }\n"
"QFrame#composer { background-color: #ffffff; border-top: 1px solid #eee8f8; }\n"
"MentionInput#messageInput { color: #252044; background-color: #ffffff; border: 1px solid #e4ddf3; border-radius: 13px; padding: 13px 16px; selection-background-color: #8064d9; font-size: 14px; }\n"
"MentionInput#messageInput:focus { border: 1px solid #9875e8; }\n"
"QPushButton#sendButton { color: white; background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #9068f3,stop:1 #7457"
                        "d8); border: none; border-radius: 13px; padding: 8px 13px; font-size: 15px; font-weight: 600; }\n"
"QPushButton#sendButton:hover { background: #9574ee; }\n"
"QPushButton#sendButton:pressed { background: #694cc8; }\n"
"QPushButton#sendButton:disabled { color:#f8f6fc; background: #c5b7e6; }\n"
"QPushButton#settingsButton, QPushButton#helpButton, QPushButton#exitButton { color: #4d4279; background: transparent; border: none; border-radius: 9px; padding: 7px 9px; font-size: 13px; }\n"
"QPushButton#settingsButton:hover, QPushButton#helpButton:hover, QPushButton#exitButton:hover { background-color: #eee8fb; }\n"
"QFrame#infoDivider { background-color: #eee8f8; border: none; }\n"
"   "));
        rootLayout = new QVBoxLayout(centralwidget);
        rootLayout->setSpacing(14);
        rootLayout->setObjectName("rootLayout");
        rootLayout->setContentsMargins(16, 14, 16, 14);
        headerPanel = new HeaderWaveFrame(centralwidget);
        headerPanel->setObjectName("headerPanel");
        headerPanel->setMinimumSize(QSize(0, 78));
        headerLayout = new QHBoxLayout(headerPanel);
        headerLayout->setSpacing(13);
        headerLayout->setObjectName("headerLayout");
        headerLayout->setContentsMargins(20, 8, 16, 8);
        logoLabel = new QLabel(headerPanel);
        logoLabel->setObjectName("logoLabel");
        logoLabel->setMinimumSize(QSize(50, 50));
        logoLabel->setMaximumSize(QSize(50, 50));

        headerLayout->addWidget(logoLabel);

        brandLayout = new QVBoxLayout();
        brandLayout->setSpacing(2);
        brandLayout->setObjectName("brandLayout");
        titleLabel = new QLabel(headerPanel);
        titleLabel->setObjectName("titleLabel");

        brandLayout->addWidget(titleLabel);

        subtitleLabel = new QLabel(headerPanel);
        subtitleLabel->setObjectName("subtitleLabel");

        brandLayout->addWidget(subtitleLabel);


        headerLayout->addLayout(brandLayout);

        headerSpacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        headerLayout->addItem(headerSpacer);

        settingsButton = new QPushButton(headerPanel);
        settingsButton->setObjectName("settingsButton");

        headerLayout->addWidget(settingsButton);

        helpButton = new QPushButton(headerPanel);
        helpButton->setObjectName("helpButton");

        headerLayout->addWidget(helpButton);

        exitButton = new QPushButton(headerPanel);
        exitButton->setObjectName("exitButton");

        headerLayout->addWidget(exitButton);


        rootLayout->addWidget(headerPanel);

        bodyLayout = new QHBoxLayout();
        bodyLayout->setSpacing(14);
        bodyLayout->setObjectName("bodyLayout");
        chatPanel = new QFrame(centralwidget);
        chatPanel->setObjectName("chatPanel");
        chatPanelLayout = new QVBoxLayout(chatPanel);
        chatPanelLayout->setSpacing(0);
        chatPanelLayout->setObjectName("chatPanelLayout");
        chatPanelLayout->setContentsMargins(0, 0, 0, 0);
        messageScroll = new QScrollArea(chatPanel);
        messageScroll->setObjectName("messageScroll");
        messageScroll->setWidgetResizable(true);
        messageScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        messageContainer = new QWidget();
        messageContainer->setObjectName("messageContainer");
        messageLayout = new QVBoxLayout(messageContainer);
        messageLayout->setSpacing(12);
        messageLayout->setObjectName("messageLayout");
        messageLayout->setContentsMargins(28, 18, 28, 18);
        messageBottomSpacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        messageLayout->addItem(messageBottomSpacer);

        messageScroll->setWidget(messageContainer);

        chatPanelLayout->addWidget(messageScroll);

        composer = new QFrame(chatPanel);
        composer->setObjectName("composer");
        composer->setMinimumSize(QSize(0, 76));
        inputLayout = new QHBoxLayout(composer);
        inputLayout->setSpacing(12);
        inputLayout->setObjectName("inputLayout");
        inputLayout->setContentsMargins(16, 12, 16, 12);
        messageInput = new MentionInput(composer);
        messageInput->setObjectName("messageInput");
        messageInput->setMinimumSize(QSize(0, 50));
        messageInput->setMaximumSize(QSize(16777215, 50));

        inputLayout->addWidget(messageInput);

        sendButton = new QPushButton(composer);
        sendButton->setObjectName("sendButton");
        sendButton->setMinimumSize(QSize(118, 50));

        inputLayout->addWidget(sendButton);


        chatPanelLayout->addWidget(composer);


        bodyLayout->addWidget(chatPanel);

        rightPanel = new QFrame(centralwidget);
        rightPanel->setObjectName("rightPanel");
        rightPanel->setMinimumSize(QSize(230, 0));
        rightLayout = new QVBoxLayout(rightPanel);
        rightLayout->setSpacing(12);
        rightLayout->setObjectName("rightLayout");
        rightLayout->setContentsMargins(16, 18, 16, 20);
        memberHeader = new QHBoxLayout();
        memberHeader->setObjectName("memberHeader");
        memberIcon = new QLabel(rightPanel);
        memberIcon->setObjectName("memberIcon");
        memberIcon->setMinimumSize(QSize(22, 22));

        memberHeader->addWidget(memberIcon);

        onlineTitle = new QLabel(rightPanel);
        onlineTitle->setObjectName("onlineTitle");

        memberHeader->addWidget(onlineTitle);

        memberSpacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        memberHeader->addItem(memberSpacer);

        onlineBadge = new RoundedLabel(rightPanel);
        onlineBadge->setObjectName("onlineBadge");

        memberHeader->addWidget(onlineBadge);


        rightLayout->addLayout(memberHeader);

        userList = new QListWidget(rightPanel);
        userList->setObjectName("userList");

        rightLayout->addWidget(userList);

        infoDivider = new QFrame(rightPanel);
        infoDivider->setObjectName("infoDivider");
        infoDivider->setMinimumSize(QSize(0, 1));
        infoDivider->setMaximumSize(QSize(16777215, 1));

        rightLayout->addWidget(infoDivider);

        serverStatusLabel = new QLabel(rightPanel);
        serverStatusLabel->setObjectName("serverStatusLabel");

        rightLayout->addWidget(serverStatusLabel);

        onlineCountLabel = new QLabel(rightPanel);
        onlineCountLabel->setObjectName("onlineCountLabel");

        rightLayout->addWidget(onlineCountLabel);


        bodyLayout->addWidget(rightPanel);


        rootLayout->addLayout(bodyLayout);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menuFile = new QMenu(menubar);
        menuFile->setObjectName("menuFile");
        menuSettings = new QMenu(menubar);
        menuSettings->setObjectName("menuSettings");
        menuHelp = new QMenu(menubar);
        menuHelp->setObjectName("menuHelp");
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        menubar->addAction(menuFile->menuAction());
        menubar->addAction(menuSettings->menuAction());
        menubar->addAction(menuHelp->menuAction());
        menuFile->addAction(actionConnect);
        menuFile->addAction(actionDisconnect);
        menuFile->addAction(actionExit);
        menuSettings->addAction(actionChangeNickname);
        menuHelp->addAction(actionAbout);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "VChat - \350\201\212\345\244\251\345\256\244", nullptr));
        actionConnect->setText(QCoreApplication::translate("MainWindow", "\350\277\236\346\216\245\346\234\215\345\212\241\345\231\250", nullptr));
        actionDisconnect->setText(QCoreApplication::translate("MainWindow", "\346\226\255\345\274\200\350\277\236\346\216\245", nullptr));
        actionExit->setText(QCoreApplication::translate("MainWindow", "\351\200\200\345\207\272", nullptr));
        actionChangeNickname->setText(QCoreApplication::translate("MainWindow", "\344\277\256\346\224\271\346\230\265\347\247\260", nullptr));
        actionAbout->setText(QCoreApplication::translate("MainWindow", "\345\205\263\344\272\216", nullptr));
        titleLabel->setText(QCoreApplication::translate("MainWindow", "VChat \350\201\212\345\244\251\345\256\244", nullptr));
        subtitleLabel->setText(QCoreApplication::translate("MainWindow", "\350\275\273\346\235\276\350\277\236\346\216\245\346\257\217\344\270\200\346\254\241\344\272\244\346\265\201", nullptr));
        settingsButton->setText(QCoreApplication::translate("MainWindow", "\350\256\276\347\275\256", nullptr));
        helpButton->setText(QCoreApplication::translate("MainWindow", "\345\270\256\345\212\251", nullptr));
        exitButton->setText(QCoreApplication::translate("MainWindow", "\351\200\200\345\207\272", nullptr));
        messageInput->setPlaceholderText(QCoreApplication::translate("MainWindow", "\350\257\267\350\276\223\345\205\245\346\266\210\346\201\257...", nullptr));
        sendButton->setText(QCoreApplication::translate("MainWindow", "\345\217\221\351\200\201", nullptr));
        onlineTitle->setText(QCoreApplication::translate("MainWindow", "\345\234\250\347\272\277\346\210\220\345\221\230", nullptr));
        onlineBadge->setText(QCoreApplication::translate("MainWindow", "0", nullptr));
        serverStatusLabel->setText(QCoreApplication::translate("MainWindow", "\346\234\215\345\212\241\345\231\250\357\274\232\346\234\252\350\277\236\346\216\245", nullptr));
        onlineCountLabel->setText(QCoreApplication::translate("MainWindow", "\345\234\250\347\272\277\344\272\272\346\225\260\357\274\2320", nullptr));
        menuFile->setTitle(QCoreApplication::translate("MainWindow", "\346\226\207\344\273\266", nullptr));
        menuSettings->setTitle(QCoreApplication::translate("MainWindow", "\350\256\276\347\275\256", nullptr));
        menuHelp->setTitle(QCoreApplication::translate("MainWindow", "\345\270\256\345\212\251", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
