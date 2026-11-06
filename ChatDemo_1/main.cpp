#include "mainwindow.h"
#include "logindialog.h"

#include <QApplication>
#include <QDialog>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 程序启动后先显示登录窗口。
    // 只有登录窗口返回 Accepted，也就是账号、密码、验证码都验证通过后，才继续打开聊天主界面。
    LoginDialog loginDialog;
    if (loginDialog.exec() != QDialog::Accepted) {
        return 0;
    }

    // 把登录时输入的账号传给主窗口，主窗口会把它作为当前用户昵称显示。
    MainWindow w(loginDialog.username(), loginDialog.serverAddress(), nullptr,
                 loginDialog.accountId(), loginDialog.sessionToken());
    w.show();
    return a.exec();
}
