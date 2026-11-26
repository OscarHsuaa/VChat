#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAbstractSocket>
#include <QColor>
#include <QDateTime>
#include <QFrame>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QSet>
#include <QLineEdit>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPalette>
#include <QPointer>
#include <QScrollBar>
#include <QStringList>
#include <QTcpSocket>
#include <QList>
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE
class QResizeEvent;
class QWidget;
class QLabel;
class AiMarkdownWidget;
class SummarySection;
class QScrollArea;
class QToolButton;
class QListWidget;
class QEvent;
class QTimer;
class RoomSidebarWidget;
class RoomWelcomeToast;
class RoomMessagePlaceholder;
class RoomCreateDialog;
class RoomInviteDialog;
class RoomInviteInboxDialog;
class RoomManagementDialog;
class RoomPermissionsDialog;
class QPushButton;
class QGridLayout;
class QGraphicsOpacityEffect;
class QVariantAnimation;
class QTextBrowser;

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
                        QWidget *parent = nullptr,
                        qint64 accountId = 0,
                        const QString &joinToken = QString());
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
    bool eventFilter(QObject *watched, QEvent *event) override;
    Ui::MainWindow *ui;

    // 当前登录用户的昵称，用于显示消息发送者和在线用户列表。
    QString nickname;
    qint64 ownAccountId = 0;
    QString sessionToken;
    bool mentionProtocol = false;
    bool allMentionSupported = false;
    QSet<qint64> roomHumanMemberIds;
    enum class MentionKind { User, Ai, All };
    struct MentionCandidate { qint64 id = 0; QString name; MentionKind kind = MentionKind::User; };
    struct SelectedMention { int start = 0; int length = 0; MentionCandidate candidate; };
    QVector<MentionCandidate> onlineMentionMembers;
    QVector<MentionCandidate> visibleMentions;
    QVector<SelectedMention> selectedMentions;
    QFrame *mentionPopup = nullptr;
    QListWidget *mentionList = nullptr;
    QLabel *mentionEmpty = nullptr;
    QFrame *viviMenu = nullptr;
    QFrame *composerSurface = nullptr;
    QGridLayout *composerGrid = nullptr;
    QGridLayout *viviMenuItems = nullptr;
    QToolButton *viviToggleButton = nullptr;
    QGraphicsOpacityEffect *viviMenuOpacity = nullptr;
    QPointer<QVariantAnimation> viviMenuAnimation;
    QList<QPushButton *> viviMenuCards;
    bool viviMenuExpanded = false;
    bool viviMenuVertical = false;
    QFrame *viviSummaryPanel = nullptr;
    QLabel *viviSummaryRoom = nullptr;
    QLabel *viviSummaryRange = nullptr;
    QLabel *viviSummaryStatus = nullptr;
    QScrollArea *viviSummaryScroll = nullptr;
    QVector<SummarySection *> viviSummarySections;
    QTimer *viviSummaryRenderTimer = nullptr;
    QPushButton *viviSummaryCopy = nullptr;
    QPushButton *viviSummaryRegenerate = nullptr;
    QPushButton *viviSummaryShare = nullptr;
    QHash<QWidget *, bool> viviSummaryMemberVisibility;
    QString viviSummaryRequestId;
    QString viviSummaryRoomId;
    QString viviSummaryText;
    QString viviSummaryGeneratedAt;
    int viviSummaryMessageCount = 0;
    QString viviSummaryFirstSequence;
    QString viviSummaryLastSequence;
    QString viviSummaryShareRequestId;
    QString viviSummaryViewedText;
    bool viviSummaryReadOnly = false;
    bool viviSummarySharePending = false;
    bool viviSummaryShared = false;
    bool viviSummaryPending = false;
    bool viviSummaryCompleted = false;
    bool viviSummarySupported = false;
    bool viviSummaryWasCollapsed = false;

    // 记录当前是否已经连上聊天服务器，发送消息前会先检查它。
    bool isConnected;

    // 客户端和 ChatServer 之间的 TCP 连接对象，所有聊天消息都通过它收发。
    QTcpSocket *socket;

    // TCP 数据可能会分多次到达，这里先缓存，再按换行符拆成完整消息。
    QByteArray receiveBuffer;
    QStringList onlineNames;
    QString renderedUserListOwner;
    QList<QWidget *> bubbleColumns;
    QList<QFrame *> messageBubbles;
    QList<QLabel *> messageBodies;
    QList<QWidget *> aiColumns;
    QList<QFrame *> aiBubbles;
    QList<AiMarkdownWidget *> aiBodies;
    QString lastMessageKey;
    QList<QFrame *> systemPills;
    QHash<QString, QWidget *> aiThinkingRows;
    QHash<QString, QJsonObject> aiReplyTargets;
    struct AiStreamState {
        AiMarkdownWidget *body = nullptr;
        QList<QToolButton *> actions;
        QToolButton *cancel = nullptr;
        QString requestId;
        QString originalRequestId;
        QString generationId;
        QString previousContent;
        bool canRegenerate = false;
        bool retryAwaiting = false;
        bool interrupted = false;
        QString raw;
        QString displayed;
        QString errorText;
        int sequence = 0;
        bool finished = false;
        bool completeReceived = false;
        bool errorReceived = false;
        int chunksReceived = 0;
        int paints = 0;
        qint64 firstChunkMs = 0;
        qint64 lastChunkMs = 0;
        qint64 maxChunkGapMs = 0;
    };
    QHash<QString, AiStreamState> aiStreams;
    QTimer *aiTypewriterTimer = nullptr;
    QSet<QString> streamedRequestIds;
    QSet<QString> renderedUserMessageIds;
    QSet<QString> renderedSummaryMessageIds;
    QList<QFrame *> summaryCards;
    QLabel *viviStatusLabel = nullptr;
    QLabel *viviStatusDot = nullptr;
    QDateTime lastTimelineTime;
    QList<QLabel *> timelineLabels;
    RoomSidebarWidget *roomSidebar = nullptr;
    QPointer<RoomCreateDialog> roomCreateDialog;
    QTimer *roomCreateTimer = nullptr;
    QToolButton *memberCollapseButton = nullptr;
    QToolButton *memberExpandButton = nullptr;
    bool createRoomInFlight = false;
    bool roomRestorePending = true;
    bool membersCollapsed = false;
    QString currentRoomId = QStringLiteral("class-room");
    QString pendingRoomId;
    QString queuedRoomId;
    QString roomSettingsKey;
    QJsonArray joinedRooms;
    struct RoomMember { qint64 id = 0; QString name; bool online = false; QString role; };
    QVector<RoomMember> roomMembers;
    QFrame *roomInfoBar = nullptr;
    QLabel *roomNameLabel = nullptr;
    QLabel *roomDescriptionLabel = nullptr;
    QPushButton *inviteMembersButton = nullptr;
    QToolButton *inviteInboxButton = nullptr;
    QPointer<RoomInviteDialog> roomInviteDialog;
    QPointer<RoomInviteInboxDialog> roomInviteInboxDialog;
    QPointer<RoomManagementDialog> roomManagementDialog;
    QPointer<RoomPermissionsDialog> roomPermissionsDialog;
    QTimer *permissionsTimer = nullptr;
    QString permissionsReadId;
    QString permissionsSaveId;
    QToolButton *roomMoreButton = nullptr;
    QTimer *managementTimer = nullptr;
    QString managementRequestId;
    QString managementAction;
    QString managementRoomId;
    QJsonArray pendingRoomInvites;
    QString inviteSearchRequestId;
    QString inviteSendRequestId;
    QString inviteDecisionRequestId;
    QTimer *inviteSendTimer = nullptr;
    QTimer *inviteDecisionTimer = nullptr;
    bool inviteSendInFlight = false;
    bool inviteDecisionInFlight = false;
    QSet<qint64> onlineRoomUserIds;
    bool roomListRequested = false;
    bool roomListLoaded = false;
    bool switchInFlight = false;
    bool roomHistoryLoading = false;
    bool initialHistoryDone = false;
    bool roomHasMessages = false;
    bool roomHistoryLoadFailed = false;
    RoomMessagePlaceholder *roomMessagePlaceholder = nullptr;
    struct UnreadState {
        qint64 count = 0;
        qint64 mentions = 0;
        qint64 readThrough = 0;
        qint64 observedThrough = 0;
    };
    QHash<QString, UnreadState> unreadRooms;
    QPushButton *newMessagesButton = nullptr;
    QTimer *unreadViewTimer = nullptr;
    QTimer *notificationRetryTimer = nullptr;
    bool notificationsSupported = false;
    bool messageSoundEnabled = true;
    bool mentionAlertsEnabled = true;
    qint64 displayedUserSequence = 0;
    qint64 lastLiveUserSequence = 0;
    qint64 requestedReadSequence = 0;
    bool followedLastLiveMessage = false;
    bool roomWelcomePending = false;
    RoomWelcomeToast *roomWelcomeToast = nullptr;

    // 初始化聊天窗口样式、状态栏、输入框颜色等界面内容。
    void setupUI();

    // 连接 socket 的网络信号，例如 connected、disconnected、readyRead。
    void connectSlots();

    // 根据 “IP:端口” 形式的地址连接服务器，例如 127.0.0.1:8080。
    bool connectToServerAddress(const QString &serverAddress, bool showErrorMessage);

    // 告诉服务器“我已经进入聊天界面”，服务器据此更新在线用户列表。
    void sendJoinMessage();
    void sendRoomRequest(const QString &action, const QJsonObject &arguments = {});
    void setupMessageNotifications();
    void sendNotificationRequest(const QString &action, const QJsonObject &arguments = {});
    void handleNotification(const QString &event, const QJsonObject &payload);
    void updateUnreadBadge(const QString &roomId);
    void scheduleUnreadViewUpdate();
    void refreshUnreadView();
    void positionNewMessagesButton();
    bool atLatestMessage() const;
    void requestRoomSwitch(const QString &roomId);
    QJsonObject currentRoomData() const;
    void showRoomWelcomeIfReady();
    void openCreateRoomDialog();
    void setMembersCollapsed(bool collapsed);
    void openInviteDialog();
    void openInviteInbox();
    void openRoomManagement(const QString &page = QStringLiteral("details"));
    void openRoomPermissions(const QString &roomId);
    void submitRoomManagement(const QString &action, const QString &roomId,
                              const QJsonObject &arguments = {});
    void applyRoomUpdate(const QJsonObject &room);
    void revokeRoomAccess(const QString &roomId);
    void refreshPendingInvites();
    void updateInviteBadge();
    bool canInviteToCurrentRoom() const;
    void handleRoomNotice(const QJsonObject &notice);
    void handleRoomResult(const QJsonObject &result);
    void clearMessageView();
    void updateRoomMessagePlaceholder();
    void noteRoomMessage();
    void updateRoomTitle();
    void renderRoomMembers();
    void requestRoomMembers();

    // 在聊天框里显示普通聊天消息。
    void appendMessage(const QString &user, const QString &message, bool own = false,
                       const QString &displayTime = QString(), bool ai = false,
                       const QString &aiRequestId = QString(),
                       const QJsonArray &mentions = {},
                       const QJsonObject &replyTarget = {},
                       const QString &streamMessageId = {},
                       const QString &userMessageId = {});
    void appendAiStreamEvent(const QJsonObject &event);
    void appendSharedSummary(const QJsonObject &message);
    QDateTime appendTimeLabelIfNeeded(const QString &timestamp);
    void insertTimeLabel(int index, const QDateTime &sent);
    void rebuildTimeLabels();
    void refreshTimeLabels();
    void scheduleTimeLabelRefresh();
    void openSharedSummary(const QJsonObject &message);
    void handleSummaryShare(const QJsonObject &event);
    void flushAiStream(const QString &messageId);
    void advanceAiTypewriter();
    void finalizeAiStream(const QString &messageId);
    void appendAiThinking(const QString &id);
    void removeAiThinking(const QString &id);
    void setAiStatus(const QString &status);

    // 在聊天框里显示系统提示，例如连接成功、断开连接。
    void appendSystemMessage(const QString &message);
    void appendPresenceNotice(const QString &name, bool joined);

    // 处理服务器发来的单行数据，区分在线列表、系统消息和普通聊天消息。
    void appendIncomingLine(const QString &line);

    // 刷新右侧在线用户列表。
    void updateUserList(const QStringList &users = QStringList());
    void updateMemberListHeight();
    void updateBubbleWidths();
    void setupMentionPopup();
    void setupViviComposer();
    void setupViviSummary();
    void showViviSummaryPanel();
    void openViviSummary();
    void closeViviSummary();
    void requestViviSummary();
    void handleViviSummary(const QJsonObject &event);
    void renderViviSummary(bool completed);
    void setViviMenuExpanded(bool expanded, bool animate = true);
    void layoutViviComposer(bool expanded);
    void updateViviMenuLayout();
    int viviMenuTargetHeight() const;
    void updateMentionPopup();
    void positionMentionPopup();
    void acceptMention();
    bool canMentionAll() const;
    bool currentMentionQuery(int *start, QString *query) const;
    void updateSelectedMentionRanges(int position, int removed, int added);
};

#endif // MAINWINDOW_H
