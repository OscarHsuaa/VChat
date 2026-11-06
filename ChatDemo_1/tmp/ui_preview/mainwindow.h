#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAbstractSocket>
#include <QColor>
#include <QDateTime>
#include <QFrame>
#include <QInputDialog>
#include <QLineEdit>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPalette>
#include <QScrollBar>
#include <QStringList>
#include <QTcpSocket>
#include <QList>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE
class QResizeEvent;
class QWidget;
class QLabel;

class HeaderWaveFrame : public QFrame
{
public:
    using QFrame::QFrame;

protected:
    void paintEvent(QPaintEvent *event) override;
};

class RoundedLabel : public QLabel
{
public:
    using QLabel::QLabel;
    void setFillColor(const QColor &color) { fillColor = color; update(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor fillColor = QColor("#f0e9fc");
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // loginName 是登录成功后显示在聊天窗口里的昵称。
    // serverAddress 是登录界面填写的服务器地址，传进来后主窗口会自动连接。
    explicit MainWindow(const QString &loginName = QString(),
                        const QString &serverAddress = QString(),
                        QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // 下面这些 on_xxx 函数和 mainwindow.ui 里的控件名称对应，
    // Qt 会在 setupUi() 时自动把按钮、菜单、输入框事件连接到这些槽函数。
    void on_sendButton_clicked();
    void on_messageInput_returnPressed();
    void on_actionConnect_triggered();
    void on_actionDisconnect_triggered();
    void on_actionExit_triggered();
    void on_actionChangeNickname_triggered();
    void on_actionAbout_triggered();
    void on_socketReadyRead();

private:
    void resizeEvent(QResizeEvent *event) override;
    Ui::MainWindow *ui;

    // 当前登录用户的昵称，用于显示消息发送者和在线用户列表。
    QString nickname;

    // 记录当前是否已经连上聊天服务器，发送消息前会先检查它。
    bool isConnected;

    // 客户端和 ChatServer 之间的 TCP 连接对象，所有聊天消息都通过它收发。
    QTcpSocket *socket;

    // TCP 数据可能会分多次到达，这里先缓存，再按换行符拆成完整消息。
    QByteArray receiveBuffer;
    QStringList onlineNames;
    QList<QWidget *> bubbleColumns;
    QList<QFrame *> messageBubbles;
    QList<QLabel *> messageBodies;
    QList<QFrame *> systemPills;
    QDate lastSystemDate;

    // 初始化聊天窗口样式、状态栏、输入框颜色等界面内容。
    void setupUI();

    // 连接 socket 的网络信号，例如 connected、disconnected、readyRead。
    void connectSlots();

    // 根据 “IP:端口” 形式的地址连接服务器，例如 127.0.0.1:8080。
    bool connectToServerAddress(const QString &serverAddress, bool showErrorMessage);

    // 告诉服务器“我已经进入聊天界面”，服务器据此更新在线用户列表。
    void sendJoinMessage();

    // 在聊天框里显示普通聊天消息。
    void appendMessage(const QString &user, const QString &message, bool own = false,
                       const QString &displayTime = QString());

    // 在聊天框里显示系统提示，例如连接成功、断开连接。
    void appendSystemMessage(const QString &message);

    // 处理服务器发来的单行数据，区分在线列表、系统消息和普通聊天消息。
    void appendIncomingLine(const QString &line);

    // 刷新右侧在线用户列表。
    void updateUserList(const QStringList &users = QStringList());
    void updateMemberListHeight();
    void updateBubbleWidths();
};

#endif // MAINWINDOW_H
