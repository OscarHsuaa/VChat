#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QString>

class QLineEdit;
class QPaintEvent;
class QEvent;

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog() override;

    // 登录成功后返回用户昵称，主窗口会用这个名字显示聊天身份。
    QString username() const;
    qint64 accountId() const { return loggedInAccountId; }
    QString sessionToken() const { return loginSessionToken; }

    // 返回登录界面填写的服务器地址，主窗口打开后会继续连接同一个服务器。
    QString serverAddress() const;

private slots:
    // 刷新随机验证码。
    void refreshCaptcha();

    // 点击“登录”或按回车时执行完整登录流程。
    void tryLogin();

private:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    Ui::LoginDialog *ui;

    // 服务器地址输入框是运行时动态添加的，所以这里单独保存指针。
    QLineEdit *serverEdit;

    // 当前显示在界面上的验证码。
    QString captchaCode;

    // 服务器登录成功后返回的数据库昵称，例如 张三、李四。
    QString loggedInNickname;
    qint64 loggedInAccountId = 0;
    QString loginSessionToken;
    bool loginInProgress = false;

    // 生成 5 位随机验证码。
    QString createCaptcha() const;

    // 连接 ChatServer，把账号密码发给服务器验证 users 表。
    bool authenticateWithServer(const QString &username, const QString &password, QString *errorMessage);
};

#endif // LOGINDIALOG_H
