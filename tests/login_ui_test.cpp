#include "logindialog.h"

#include <QApplication>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
    LoginDialog dialog;
    dialog.show();
    app.processEvents();

    auto *pages = dialog.findChild<QStackedWidget *>(QStringLiteral("loginPages"));
    auto *menu = dialog.findChild<QToolButton *>(QStringLiteral("loginMenuButton"));
    auto *registration = dialog.findChild<QDialog *>(QStringLiteral("registrationDialog"));
    auto *link = dialog.findChild<QLabel *>(QStringLiteral("registrationInfoLabel"));
    if (!pages || pages->count() != 2 || !menu || !menu->menu() ||
        menu->menu()->actions().size() != 3 || !registration || !registration->isModal() ||
        !link || dialog.width() > 560 || dialog.height() > 580) return 1;
    if (menu->menu()->actions().at(0)->text() != QStringLiteral("服务器设置") ||
        menu->menu()->actions().at(1)->text() != QStringLiteral("关于 VChat") ||
        menu->menu()->actions().at(2)->text() != QStringLiteral("问题反馈")) return 2;
    // Render the saved-account state without writing to the user's real Windows registry.
    pages->setCurrentIndex(0);
    dialog.findChild<QLabel *>(QStringLiteral("savedAccountName"))->setText(QStringLiteral("测试用户"));
    if (argc > 1 && !dialog.grab().save(QString::fromLocal8Bit(argv[1]))) return 3;

    if (pages->currentIndex() == 0) {
        auto *savedLogin = dialog.findChild<QPushButton *>(QStringLiteral("savedAccountLoginButton"));
        if (!savedLogin || link->isVisible()) return 4;
        savedLogin->click();
    }
    if (pages->currentIndex() != 1 || !link->isVisible() ||
        !dialog.findChild<QLineEdit *>(QStringLiteral("passwordEdit"))->text().isEmpty()) return 5;
    if (argc > 2 && !dialog.grab().save(QString::fromLocal8Bit(argv[2]))) return 6;

    int modalResult = 0;
    QTimer::singleShot(0, [&]() {
        if (!registration->isVisible()) { modalResult = 7; registration->reject(); return; }
        if (argc > 3 && !registration->grab().save(QString::fromLocal8Bit(argv[3]))) {
            modalResult = 8; registration->reject(); return;
        }
        auto *returnLink = dialog.findChild<QLabel *>(QStringLiteral("returnLoginLink"));
        if (!returnLink || !returnLink->text().contains(QStringLiteral("已有账号？"))) modalResult = 9;
        registration->reject();
    });
    if (!QMetaObject::invokeMethod(link, "linkActivated", Q_ARG(QString, QStringLiteral("register")))) return 10;
    return modalResult;
}
