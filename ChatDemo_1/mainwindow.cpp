#include "mainwindow.h"
#include "aimarkdownwidget.h"
#include "mentioninput.h"
#include "roomsidebarwidget.h"
#include "roomcreatedialog.h"
#include "roominvitedialog.h"
#include "roommanagementdialog.h"
#include "roompermissionsdialog.h"
#include "ui_mainwindow.h"

#include <QFont>
#include <QFrame>
#include <QApplication>
#include <QEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QGuiApplication>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStyle>
#include <QSvgRenderer>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QTextCursor>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextList>
#include <QTextFragment>
#include <QTextBoundaryFinder>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QUuid>
#include <QVariantAnimation>
#include <algorithm>

namespace {
QIcon iconFromSvg(const QString &path, int size = 20)
{
    QSvgRenderer renderer(path);
    QPixmap pixmap(size * 2, size * 2);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(0, 0, size, size));
    return QIcon(pixmap);
}

QIcon inviteBellIcon(bool hasPending)
{
    QPixmap pixmap(44, 44);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QSvgRenderer renderer(QStringLiteral(":/login/bell.svg"));
    renderer.render(&painter, QRectF(1, 2, 18, 18));
    if (hasPending) {
        painter.setBrush(QColor("#d85d83"));
        painter.setPen(QPen(Qt::white, 1.2));
        painter.drawEllipse(QPointF(17, 4), 3.3, 3.3);
    }
    return QIcon(pixmap);
}

QIcon memberChevron(bool pointsLeft)
{
    QPixmap pixmap(40, 40);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor("#7956d5"), 2.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    if (pointsLeft) {
        painter.drawLine(QPointF(12, 5), QPointF(7, 10));
        painter.drawLine(QPointF(7, 10), QPointF(12, 15));
    } else {
        painter.drawLine(QPointF(8, 5), QPointF(13, 10));
        painter.drawLine(QPointF(13, 10), QPointF(8, 15));
    }
    return QIcon(pixmap);
}

QPixmap chatLogo()
{
    QPixmap pixmap(100, 100);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath bubble;
    bubble.addRoundedRect(QRectF(8, 7, 83, 71), 23, 23);
    bubble.moveTo(25, 72);
    bubble.lineTo(20, 91);
    bubble.lineTo(45, 77);
    QLinearGradient fill(10, 8, 88, 80);
    fill.setColorAt(0, QColor("#8765e9"));
    fill.setColorAt(1, QColor("#a875e6"));
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawPath(bubble);
    painter.setBrush(Qt::white);
    for (int x : {31, 50, 69})
        painter.drawEllipse(QPointF(x, 43), 5, 5);
    return pixmap;
}

QLabel *avatarLabel(const QString &name, QWidget *parent, int size = 36)
{
    auto *avatar = new QLabel(name.left(1).isEmpty() ? QStringLiteral("我") : name.left(1), parent);
    avatar->setTextFormat(Qt::PlainText);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setFixedSize(size, size);
    avatar->setStyleSheet(QStringLiteral("color:#694ac5; background:#eee5fc; border-radius:%1px; font-size:%2px; font-weight:600;")
                              .arg(size / 2).arg(size <= 36 ? 16 : 17));
    return avatar;
}

class SystemPill : public QFrame
{
public:
    explicit SystemPill(QWidget *parent, const QColor &color = QColor("#f1ecfc"))
        : QFrame(parent), fillColor(color) {}

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fillColor);
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 20, 20);
    }
private:
    QColor fillColor;
};
}

void HeaderWaveFrame::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(rect(), 18, 18);
    painter.setClipPath(clip);
    QLinearGradient base(0, 0, width(), height());
    base.setColorAt(0, QColor("#fefbfd"));
    base.setColorAt(0.55, QColor("#faf5ff"));
    base.setColorAt(1, QColor("#fffafd"));
    painter.fillRect(rect(), base);

    const qreal w = width();
    const qreal h = height();
    QPainterPath back;
    back.moveTo(0, h * 0.55);
    back.cubicTo(w * 0.16, h * 0.15, w * 0.24, h * 1.05, w * 0.43, h * 0.48);
    back.cubicTo(w * 0.61, h * -0.01, w * 0.75, h * 0.9, w, h * 0.30);
    back.lineTo(w, h);
    back.lineTo(0, h);
    back.closeSubpath();
    painter.fillPath(back, QColor(240, 232, 252, 145));

    QPainterPath front;
    front.moveTo(0, h * 0.86);
    front.cubicTo(w * 0.16, h * 0.34, w * 0.29, h * 1.26, w * 0.48, h * 0.74);
    front.cubicTo(w * 0.68, h * 0.30, w * 0.83, h * 1.17, w, h * 0.61);
    front.lineTo(w, h);
    front.lineTo(0, h);
    front.closeSubpath();
    painter.fillPath(front, QColor(237, 227, 250, 110));
}

void RoundedLabel::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);
    painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), height() / 2.0, height() / 2.0);
    painter.end();
    QLabel::paintEvent(event);
}

// The summary is one Markdown response, but its four known sections need
// independent layout in the narrow member-panel column. The raw response is
// retained by MainWindow for copying; this scanner only locates section bounds.
static QVector<QString> splitSummarySections(const QString &markdown)
{
    const QStringList headings{QStringLiteral("讨论概览"), QStringLiteral("已达成共识"),
                               QStringLiteral("待解决问题"), QStringLiteral("下一步行动")};
    QVector<QString> sections(4);
    int current = 0;
    QChar fenceMarker;
    int fenceLength = 0;
    QString normalized = markdown;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    for (const QString &line : normalized.split(QLatin1Char('\n'), Qt::KeepEmptyParts)) {
        int pos = 0;
        while (pos < line.size() && line.at(pos) == QLatin1Char(' ') && pos < 3) ++pos;
        const QChar marker = pos < line.size() ? line.at(pos) : QChar();
        if (marker == QLatin1Char('`') || marker == QLatin1Char('~')) {
            const int start = pos;
            while (pos < line.size() && line.at(pos) == marker) ++pos;
            const int count = pos - start;
            if (fenceLength && marker == fenceMarker && count >= fenceLength &&
                line.mid(pos).trimmed().isEmpty()) {
                fenceLength = 0;
                fenceMarker = {};
            } else if (!fenceLength && count >= 3) {
                fenceLength = count;
                fenceMarker = marker;
            }
        }
        if (!fenceLength && marker == QLatin1Char('#')) {
            const int start = pos;
            while (pos < line.size() && line.at(pos) == QLatin1Char('#')) ++pos;
            const int level = pos - start;
            if (level >= 1 && level <= 6 && pos < line.size() && line.at(pos).isSpace()) {
                QString heading = line.mid(pos).trimmed();
                while (heading.endsWith(QLatin1Char('#'))) heading.chop(1);
                const int index = headings.indexOf(heading.trimmed());
                if (index >= 0) { current = index; continue; }
            }
        }
        sections[current] += line + QLatin1Char('\n');
    }
    for (QString &section : sections) section = section.trimmed();
    return sections;
}

static QString summaryPreview(const QString &markdown, int maximum = 82)
{
    QTextDocument document;
    document.setMarkdown(markdown, QTextDocument::MarkdownFeatures(
        QTextDocument::MarkdownDialectGitHub | QTextDocument::MarkdownNoHTML));
    QString plain = document.toPlainText().simplified();
    if (plain.isEmpty()) return QStringLiteral("本次讨论暂无明确记录");
    if (plain.size() <= maximum) return plain;
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, plain);
    finder.setPosition(maximum);
    const int end = finder.toPreviousBoundary();
    int cut = end > 0 ? end : maximum;
    // Prefer a completed thought when the overview has a sentence boundary.
    for (int i = cut - 1; i >= cut / 2; --i) {
        if (QStringLiteral("。！？!?；;").contains(plain.at(i))) {
            cut = i + 1;
            break;
        }
    }
    return plain.left(cut).trimmed() + QChar(0x2026);
}

static QStringList summaryPreviewItems(const QString &markdown)
{
    QTextDocument document;
    document.setMarkdown(markdown, QTextDocument::MarkdownFeatures(
        QTextDocument::MarkdownDialectGitHub | QTextDocument::MarkdownNoHTML));
    QStringList items;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        if (block.textList() && block.textList()->format().indent() == 1 &&
            !block.text().trimmed().isEmpty())
            items.append(block.text().trimmed());
    }
    return items;
}

static QString localSummaryTime(const QString &isoTime)
{
    QDateTime time = QDateTime::fromString(isoTime, Qt::ISODateWithMs);
    if (!time.isValid()) time = QDateTime::fromString(isoTime, Qt::ISODate);
    return time.isValid() ? time.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                          : QStringLiteral("时间未知");
}

// Summary preview and simple-list rows stay plain text and use QLabel's
// height-for-width wrapping. The outer chat/panel scroll areas own scrolling.
class SummaryBodyLabel final : public QLabel
{
public:
    explicit SummaryBodyLabel(const QString &text, QWidget *parent = nullptr)
        : QLabel(text, parent)
    {
        setTextFormat(Qt::PlainText);
        setWordWrap(true);
        QFont body(QStringLiteral("Microsoft YaHei UI"));
        body.setPixelSize(14);
        setFont(body);
        setStyleSheet(QStringLiteral("color:#30234e; background:transparent;"));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
};

class SummarySection final : public QFrame
{
public:
    SummarySection(const QString &title, const QString &symbol, const QString &accent,
                   const QString &background, QWidget *parent)
        : QFrame(parent), titleText(title), symbolText(symbol)
    {
        setObjectName(QStringLiteral("viviSummarySection") + title);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        setStyleSheet(QStringLiteral(
            "QFrame#%1 { background:transparent; border:0; }"
            "QLabel#viviSectionTitle { background:transparent; color:#282043;"
            " font-size:15px; font-weight:700; }"
            "QLabel#viviSectionCount { color:#8b75ad; background:transparent; font-size:12px; }"
            "QLabel#viviSectionEmpty { background:transparent; color:#817696;"
            " font-size:14px; }")
            .arg(objectName()));
        column = new QVBoxLayout(this);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(8);
        auto *header = new QHBoxLayout;
        header->setContentsMargins(0, 0, 0, 0);
        header->setSpacing(8);
        auto *badge = new QLabel(symbol, this);
        badge->setAlignment(Qt::AlignCenter);
        badge->setFixedSize(21, 21);
        badge->setStyleSheet(QStringLiteral("background:%1; color:%2; border-radius:7px;"
                                            "font-size:13px; font-weight:700;")
                             .arg(background, accent));
        header->addWidget(badge);
        auto *heading = new QLabel(title, this);
        heading->setObjectName(QStringLiteral("viviSectionTitle"));
        header->addWidget(heading, 1);
        count = new QLabel(this);
        count->setObjectName(QStringLiteral("viviSectionCount"));
        count->hide();
        header->addWidget(count);
        column->addLayout(header);
        rows = new QWidget(this);
        rows->setObjectName(QStringLiteral("viviSummaryRows"));
        rows->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        rowLayout = new QVBoxLayout(rows);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(11);
        column->addWidget(rows);
        rows->hide();
        empty = new QLabel(QStringLiteral("等待整理…"), this);
        empty->setObjectName(QStringLiteral("viviSectionEmpty"));
        empty->setWordWrap(true);
        column->addWidget(empty);
    }

    void reset()
    {
        if (body) { body->beginStreaming(); body->hide(); }
        rows->hide();
        count->hide();
        empty->setText(QStringLiteral("等待整理…"));
        empty->show();
        hasContent = false;
    }

    void showMarkdown(const QString &markdown, bool completed)
    {
        if (markdown.trimmed().isEmpty()) {
            if (body) body->hide();
            rows->hide();
            count->hide();
            empty->setText(completed ? QStringLiteral("本次讨论暂无明确记录")
                                     : QStringLiteral("等待整理…"));
            empty->show();
            return;
        }
        if (completed && titleText != QStringLiteral("讨论概览")) {
            const SimpleList parsed = parseSimpleList(markdown);
            if (parsed.count > 0) {
                count->setText(QString::number(parsed.count));
                count->show();
            } else count->hide();
            if (parsed.simple && !parsed.items.isEmpty()) {
                showRows(parsed.items);
                if (body) body->hide();
                empty->hide();
                return;
            }
        } else count->hide();
        rows->hide();
        if (!body) {
            body = new AiMarkdownWidget({}, this, true);
            body->setObjectName(QStringLiteral("viviSummarySectionBody"));
            column->insertWidget(1, body);
        }
        if (!hasContent) { body->beginStreaming(); hasContent = true; }
        if (completed) body->finishStreaming(markdown);
        else body->setStreamingText(markdown);
        empty->hide();
        body->show();
        updateBodyWidth(true);
    }

    AiMarkdownWidget *markdownBody() const { return body; }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QFrame::resizeEvent(event);
        updateBodyWidth();
        updateRowHeights();
        QTimer::singleShot(0, this, [this] {
            QWidget *content = parentWidget();
            if (content && content->layout()) {
                setMinimumHeight(column->sizeHint().height());
                content->setMinimumHeight(content->layout()->sizeHint().height());
            }
        });
    }

private:
    struct SimpleList {
        QStringList items;
        int count = 0;
        bool simple = true;
    };

    static SimpleList parseSimpleList(const QString &markdown)
    {
        QTextDocument document;
        document.setMarkdown(markdown,
            QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub)
                | QTextDocument::MarkdownFeatures(QTextDocument::MarkdownNoHTML));
        SimpleList result;
        for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
            const QString text = block.text().trimmed();
            if (text.isEmpty()) continue;
            QTextList *list = block.textList();
            if (!list) { result.simple = false; continue; }
            ++result.count;
            const QTextListFormat::Style style = list->format().style();
            if (list->format().indent() != 1 ||
                (style != QTextListFormat::ListDisc &&
                 style != QTextListFormat::ListCircle &&
                 style != QTextListFormat::ListSquare) ||
                text.contains(QChar::ObjectReplacementCharacter)) result.simple = false;
            for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
                const QTextCharFormat format = fragment.fragment().charFormat();
                if (format.isAnchor() || format.fontWeight() >= QFont::Bold ||
                    format.fontItalic() || format.fontFixedPitch() ||
                    format.fontUnderline() || format.fontStrikeOut()) result.simple = false;
            }
            result.items.append(text);
        }
        return result;
    }

    void showRows(const QStringList &items)
    {
        while (QLayoutItem *item = rowLayout->takeAt(0)) {
            delete item->widget();
            delete item;
        }
        for (const QString &itemText : items) {
            auto *row = new QWidget(rows);
            row->setObjectName(QStringLiteral("viviSummaryRow"));
            row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            auto *line = new QHBoxLayout(row);
            line->setContentsMargins(0, 0, 0, 0);
            line->setSpacing(8);
            auto *mark = new QLabel(symbolText, row);
            mark->setAlignment(Qt::AlignCenter);
            mark->setFixedSize(17, 17);
            mark->setStyleSheet(QStringLiteral("background:#efe8ff; color:#7954d4;"
                                               "border-radius:8px; font-size:11px; font-weight:700;"));
            line->addWidget(mark, 0, Qt::AlignTop);
            auto *text = new SummaryBodyLabel(itemText, row);
            text->setObjectName(QStringLiteral("viviSummaryRowText"));
            line->addWidget(text, 1);
            rowLayout->addWidget(row);
        }
        rows->show();
        rowLayout->invalidate();
        column->invalidate();
        updateRowHeights();
        rows->updateGeometry();
        updateGeometry();
    }

    void updateRowHeights()
    {
        if (rows->isHidden()) return;
        const int textWidth = qMax(40, width() - 17 - 8);
        bool changed = false;
        for (QWidget *row : rows->findChildren<QWidget *>(
                 QStringLiteral("viviSummaryRow"), Qt::FindDirectChildrenOnly)) {
            QLabel *text = row->findChild<QLabel *>(QStringLiteral("viviSummaryRowText"));
            if (!text) continue;
            const int needed = qMax(17, text->heightForWidth(textWidth));
            if (row->minimumHeight() == needed) continue;
            row->setMinimumHeight(needed);
            changed = true;
        }
        rowLayout->invalidate();
        const int totalHeight = rowLayout->sizeHint().height();
        if (rows->minimumHeight() != totalHeight) {
            rows->setMinimumHeight(totalHeight);
            changed = true;
        }
        if (changed) {
            rows->updateGeometry();
            column->invalidate();
            updateGeometry();
        }
    }

    void updateBodyWidth(bool force = false)
    {
        if (!body) return;
        const int available = qMax(100, width() - column->contentsMargins().left()
                                        - column->contentsMargins().right());
        if (!force && available == lastBodyWidth) return;
        lastBodyWidth = available;
        body->setContentWidth(available);
        // The section may shrink with the member panel even though its child
        // measures Markdown at an explicit text width.
        body->setMinimumWidth(0);
        body->setMaximumWidth(QWIDGETSIZE_MAX);
        body->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        body->resize(available, body->height());
    }

    AiMarkdownWidget *body = nullptr;
    QLabel *empty = nullptr;
    QLabel *count = nullptr;
    QWidget *rows = nullptr;
    QVBoxLayout *rowLayout = nullptr;
    QVBoxLayout *column = nullptr;
    QString titleText;
    QString symbolText;
    bool hasContent = false;
    int lastBodyWidth = -1;
};

MainWindow::MainWindow(const QString &loginName, const QString &serverAddress, QWidget *parent,
                       qint64 accountId, const QString &joinToken)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , nickname(loginName.trimmed().isEmpty() ? QStringLiteral("User") : loginName.trimmed())
    , ownAccountId(accountId)
    , sessionToken(joinToken)
    , isConnected(false)
    , socket(new QTcpSocket(this))
{
    ui->setupUi(this);
    roomSettingsKey = QStringLiteral("rooms/last/%1/%2").arg(ownAccountId).arg(
        QString::fromLatin1(QCryptographicHash::hash(serverAddress.toUtf8(), QCryptographicHash::Sha256).toHex()));
    aiTypewriterTimer = new QTimer(this);
    aiTypewriterTimer->setInterval(20);
    connect(aiTypewriterTimer, &QTimer::timeout, this, &MainWindow::advanceAiTypewriter);

    // 主窗口创建后，先设置界面，再连接网络信号，最后根据登录界面的服务器地址自动连接。
    setupUI();
    connectSlots();
    updateUserList();

    appendSystemMessage(QStringLiteral("欢迎来到 VChat 聊天室"));

    if (serverAddress.trimmed().isEmpty()) {
        appendSystemMessage(QStringLiteral("请在设置中连接聊天服务器"));
    } else {
        connectToServerAddress(serverAddress, false);
    }
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);
    // QTcpSocket may emit disconnected while its QObject parent is destroyed.
    // Stop UI slots before destroying the widgets they access.
    socket->disconnect(this);
    socket->abort();
    delete ui;
}

void MainWindow::setupUI()
{
    setWindowTitle(QStringLiteral("VChat - 聊天室"));
    menuBar()->hide();
    statusBar()->hide();
    ui->logoLabel->setPixmap(chatLogo().scaled(50, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    roomSidebar = new RoomSidebarWidget(ui->centralwidget);
    ui->bodyLayout->insertWidget(0, roomSidebar);
    ui->bodyLayout->setStretch(0, 0);
    ui->bodyLayout->setStretch(1, 3);
    ui->bodyLayout->setStretch(2, 1);
    QSettings roomSettings(QStringLiteral("VChat"), QStringLiteral("VChatClient"));
    roomSidebar->setCollapsed(roomSettings.value(QStringLiteral("rooms/sidebarCollapsed"), false).toBool());
    connect(roomSidebar, &RoomSidebarWidget::collapsedChanged, this, [this](bool collapsed) {
        QSettings settings(QStringLiteral("VChat"), QStringLiteral("VChatClient"));
        settings.setValue(QStringLiteral("rooms/sidebarCollapsed"), collapsed);
        QTimer::singleShot(0, this, &MainWindow::updateBubbleWidths);
    });
    connect(roomSidebar, &RoomSidebarWidget::roomSelected, this, &MainWindow::requestRoomSwitch);
    connect(roomSidebar, &RoomSidebarWidget::createRequested, this, &MainWindow::openCreateRoomDialog);
    roomCreateTimer = new QTimer(this);
    roomCreateTimer->setSingleShot(true);
    connect(roomCreateTimer, &QTimer::timeout, this, [this]() {
        if (!createRoomInFlight) return;
        if (roomCreateDialog) {
            roomCreateDialog->setPending(false);
            roomCreateDialog->showError(QStringLiteral("服务器响应超时，创建结果尚未确认。请检查聊天室列表；若持续无响应，请重新连接后再试。"));
        }
        sendRoomRequest(QStringLiteral("list_mine"));
    });
    memberCollapseButton = new QToolButton(ui->rightPanel);
    memberCollapseButton->setObjectName(QStringLiteral("memberCollapseButton"));
    memberCollapseButton->setIcon(memberChevron(false));
    memberCollapseButton->setIconSize(QSize(20, 20));
    memberCollapseButton->setToolTip(QStringLiteral("收起成员列表"));
    memberCollapseButton->setCursor(Qt::PointingHandCursor);
    memberCollapseButton->setFixedSize(28, 28);
    memberCollapseButton->setStyleSheet(QStringLiteral(
        "QToolButton { color:#7956d5; background:transparent; border:0; border-radius:8px; font-size:16px; }"
        "QToolButton:hover { background:#f0e9fc; }"));
    ui->memberHeader->addWidget(memberCollapseButton);
    connect(memberCollapseButton, &QToolButton::clicked, this, [this]() { setMembersCollapsed(true); });
    memberExpandButton = new QToolButton(ui->centralwidget);
    memberExpandButton->setObjectName(QStringLiteral("memberExpandButton"));
    memberExpandButton->setIcon(memberChevron(true));
    memberExpandButton->setIconSize(QSize(20, 20));
    memberExpandButton->setToolTip(QStringLiteral("展开成员列表"));
    memberExpandButton->setCursor(Qt::PointingHandCursor);
    memberExpandButton->setFixedWidth(40);
    memberExpandButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    memberExpandButton->setStyleSheet(QStringLiteral(
        "QToolButton { color:#7956d5; background:#fcfaff; border:1px solid #eee8f8; border-radius:13px; font-size:17px; }"
        "QToolButton:hover { background:#f0e9fc; }"));
    ui->bodyLayout->insertWidget(2, memberExpandButton);
    connect(memberExpandButton, &QToolButton::clicked, this, [this]() { setMembersCollapsed(false); });
    memberExpandButton->hide();
    ui->bodyLayout->setStretch(3, 1);
    setMembersCollapsed(roomSettings.value(QStringLiteral("members/collapsed/%1").arg(ownAccountId), false).toBool());
    ui->memberHeader->setSpacing(4);
    ui->memberSpacer->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum);
    ui->memberHeader->invalidate();
    ui->rightLayout->setContentsMargins(12, 18, 12, 20);
    ui->onlineBadge->setStyleSheet(QStringLiteral(
        "color:#6c52bc; padding:4px 6px; font-size:11px; font-weight:600;"));
    roomInfoBar = new QFrame(ui->chatPanel);
    roomInfoBar->setObjectName(QStringLiteral("roomInfoBar"));
    roomInfoBar->setStyleSheet(QStringLiteral(
        "QFrame#roomInfoBar { background:#fdfbff; border-bottom:1px solid #eee8f8; }"
        "QPushButton#inviteMembersButton { background:#8064d9; color:white; border:0;"
        " border-radius:9px; padding:8px 13px; font-weight:600; }"
        "QPushButton#inviteMembersButton:hover { background:#9275e8; }"
        "QPushButton#inviteMembersButton:disabled { background:#d8d1e7; }"));
    auto *roomBarLayout = new QHBoxLayout(roomInfoBar);
    roomBarLayout->setContentsMargins(18, 10, 18, 10);
    roomBarLayout->setSpacing(10);
    auto *roomText = new QVBoxLayout;
    roomText->setSpacing(2);
    auto *roomTitleRow = new QHBoxLayout;
    roomTitleRow->setSpacing(8);
    roomNameLabel = new QLabel(roomInfoBar);
    roomNameLabel->setObjectName(QStringLiteral("roomNameLabel"));
    roomNameLabel->setStyleSheet(QStringLiteral("color:#252044; font-size:15px; font-weight:600;"));
    roomTitleRow->addWidget(roomNameLabel);
    roomTitleRow->addStretch();
    roomText->addLayout(roomTitleRow);
    roomDescriptionLabel = new QLabel(roomInfoBar);
    roomDescriptionLabel->setStyleSheet(QStringLiteral("color:#827799; font-size:11px;"));
    roomDescriptionLabel->setTextFormat(Qt::PlainText);
    roomText->addWidget(roomDescriptionLabel);
    roomBarLayout->addLayout(roomText, 1);
    inviteMembersButton = new QPushButton(QStringLiteral("邀请成员"), roomInfoBar);
    inviteMembersButton->setObjectName(QStringLiteral("inviteMembersButton"));
    inviteMembersButton->setCursor(Qt::PointingHandCursor);
    roomBarLayout->addWidget(inviteMembersButton);
    connect(inviteMembersButton, &QPushButton::clicked, this, &MainWindow::openInviteDialog);
    roomMoreButton = new QToolButton(roomInfoBar);
    roomMoreButton->setObjectName(QStringLiteral("roomMoreButton"));
    roomMoreButton->setText(QStringLiteral("···"));
    roomMoreButton->setToolTip(QStringLiteral("聊天室管理"));
    roomMoreButton->setCursor(Qt::PointingHandCursor);
    roomMoreButton->setFixedSize(32, 32);
    roomMoreButton->setStyleSheet(QStringLiteral(
        "QToolButton { color:#705aab; background:#f4effb; border:1px solid #e8def6;"
        " border-radius:9px; font-size:18px; } QToolButton:hover { background:#e9def9; }"));
    roomBarLayout->addWidget(roomMoreButton);
    connect(roomMoreButton, &QToolButton::clicked, this, [this]() {
        if (!isConnected || switchInFlight || roomHistoryLoading ||
            currentRoomId == QStringLiteral("class-room")) return;
        auto *menu = new QMenu(roomMoreButton);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->setStyleSheet(QStringLiteral(
            "QMenu { background:white; border:1px solid #e8def6; border-radius:11px;"
            " padding:5px 0px; color:#352b59; font-family:'Microsoft YaHei UI'; font-size:13px; }"
            "QPushButton#roomMenuRow { background:transparent; border:0; border-radius:7px; padding:0; }"
            "QPushButton#roomMenuRow:hover { background:#f0e9fc; }"
            "QPushButton#roomMenuRow:pressed { background:#e5daf9; }"
            "QPushButton#roomMenuRow[danger='true']:hover { background:#fff0f3; }"
            "QPushButton#roomMenuRow[danger='true']:pressed { background:#fce3ea; }"));
        auto *shadow = new QGraphicsDropShadowEffect(menu);
        shadow->setBlurRadius(18);
        shadow->setOffset(0, 5);
        shadow->setColor(QColor(73, 47, 129, 35));
        menu->setGraphicsEffect(shadow);
        auto addRow = [this, menu](const QString &text, const QString &page,
                                   const QString &icon, bool danger = false) {
            auto *button = new QPushButton(menu);
            button->setObjectName(QStringLiteral("roomMenuRow"));
            button->setProperty("danger", danger);
            button->setAccessibleName(text);
            button->setCursor(Qt::PointingHandCursor);
            button->setFixedHeight(44);
            button->setMinimumWidth(194);
            auto *row = new QHBoxLayout(button);
            row->setContentsMargins(16, 0, 16, 0);
            row->setSpacing(12);
            auto *iconArea = new QLabel(button);
            iconArea->setObjectName(QStringLiteral("roomMenuIcon"));
            iconArea->setFixedSize(20, 20);
            iconArea->setAlignment(Qt::AlignCenter);
            iconArea->setPixmap(iconFromSvg(QStringLiteral(":/login/") + icon, 18).pixmap(18, 18));
            iconArea->setAttribute(Qt::WA_TransparentForMouseEvents);
            row->addWidget(iconArea);
            auto *caption = new QLabel(text, button);
            caption->setObjectName(QStringLiteral("roomMenuText"));
            caption->setStyleSheet(danger
                ? QStringLiteral("color:#c44764; font-size:13px; background:transparent;")
                : QStringLiteral("color:#352b59; font-size:13px; background:transparent;"));
            caption->setAttribute(Qt::WA_TransparentForMouseEvents);
            row->addWidget(caption, 1);
            auto *action = new QWidgetAction(menu);
            action->setText(text);
            action->setDefaultWidget(button);
            menu->addAction(action);
            connect(button, &QPushButton::clicked, action, &QAction::trigger);
            connect(action, &QAction::triggered, this, [this, menu, page]() {
                menu->close();
                openRoomManagement(page);
            });
        };
        auto addDivider = [menu]() {
            auto *container = new QWidget(menu);
            container->setFixedHeight(11);
            auto *lineLayout = new QHBoxLayout(container);
            lineLayout->setContentsMargins(16, 5, 16, 5);
            auto *line = new QFrame(container);
            line->setFixedHeight(1);
            line->setStyleSheet(QStringLiteral("background:#eee8f7; border:0;"));
            lineLayout->addWidget(line);
            auto *action = new QWidgetAction(menu);
            action->setDefaultWidget(container);
            menu->addAction(action);
        };
        addRow(QStringLiteral("聊天室详情"), QStringLiteral("details"), QStringLiteral("room.svg"));
        bool owner = false;
        bool canKick = false;
        for (const QJsonValue &value : joinedRooms) {
            const QJsonObject room = value.toObject();
            if (room.value(QStringLiteral("room_id")).toString() != currentRoomId) continue;
            owner = room.value(QStringLiteral("owner_id")).toString().toLongLong() == ownAccountId;
            canKick = owner || room.value(QStringLiteral("allow_member_kick")).toBool();
            break;
        }
        addRow(canKick ? QStringLiteral("成员管理") : QStringLiteral("查看成员"),
            QStringLiteral("members"), QStringLiteral("members.svg"));
        addDivider();
        addRow(owner ? QStringLiteral("解散聊天室") : QStringLiteral("退出聊天室"),
            owner ? QStringLiteral("dissolve") : QStringLiteral("leave"),
            owner ? QStringLiteral("trash.svg") : QStringLiteral("log-out.svg"), true);
        const QSize menuSize = menu->sizeHint();
        const QRect bounds = screen() ? screen()->availableGeometry()
                                      : QRect(mapToGlobal(QPoint(0, 0)), size());
        QPoint pos = roomMoreButton->mapToGlobal(QPoint(roomMoreButton->width() - menuSize.width(),
                                                       roomMoreButton->height() + 5));
        pos.setX(qBound(bounds.left(), pos.x(), bounds.right() - menuSize.width() + 1));
        if (pos.y() + menuSize.height() > bounds.bottom())
            pos.setY(roomMoreButton->mapToGlobal(QPoint(0, -menuSize.height() - 5)).y());
        pos.setY(qBound(bounds.top(), pos.y(), bounds.bottom() - menuSize.height() + 1));
        menu->popup(pos);
    });
    managementTimer = new QTimer(this);
    managementTimer->setSingleShot(true);
    connect(managementTimer, &QTimer::timeout, this, [this]() {
        if (!managementRequestId.isEmpty() && roomManagementDialog)
            roomManagementDialog->showError(QStringLiteral(
                "服务器尚未确认操作结果。请刷新聊天室列表核对状态，避免重复提交。"));
    });
    permissionsTimer = new QTimer(this);
    permissionsTimer->setSingleShot(true);
    connect(permissionsTimer, &QTimer::timeout, this, [this]() {
        if (!permissionsSaveId.isEmpty() && roomPermissionsDialog) {
            roomPermissionsDialog->setPending(false);
            roomPermissionsDialog->showError(QStringLiteral(
                "服务器尚未确认保存结果，请刷新权限配置后再修改，避免覆盖未知状态。"));
        }
        permissionsSaveId.clear();
    });
    ui->chatPanelLayout->insertWidget(0, roomInfoBar);
    inviteInboxButton = new QToolButton(ui->headerPanel);
    inviteInboxButton->setObjectName(QStringLiteral("inviteInboxButton"));
    inviteInboxButton->setText(QStringLiteral("邀请"));
    inviteInboxButton->setIcon(inviteBellIcon(false));
    inviteInboxButton->setIconSize(QSize(22, 22));
    inviteInboxButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    inviteInboxButton->setToolTip(QStringLiteral("查看待处理的聊天室邀请"));
    inviteInboxButton->setCursor(Qt::PointingHandCursor);
    inviteInboxButton->setStyleSheet(QStringLiteral("QToolButton { color:#5f4a95; background:transparent;"
        " border:0; border-radius:9px; padding:7px 9px; font-size:13px; }"
        "QToolButton:hover { background:#eee8fb; }"));
    ui->headerLayout->insertWidget(ui->headerLayout->count() - 3, inviteInboxButton);
    connect(inviteInboxButton, &QToolButton::clicked, this, &MainWindow::openInviteInbox);
    inviteSendTimer = new QTimer(this);
    inviteSendTimer->setSingleShot(true);
    connect(inviteSendTimer, &QTimer::timeout, this, [this]() {
        if (inviteSendInFlight && roomInviteDialog)
            roomInviteDialog->showError(QStringLiteral("服务器尚未确认邀请结果。为避免重复发送，请重新连接后刷新结果。"));
    });
    inviteDecisionTimer = new QTimer(this);
    inviteDecisionTimer->setSingleShot(true);
    connect(inviteDecisionTimer, &QTimer::timeout, this, [this]() {
        if (inviteDecisionInFlight && roomInviteInboxDialog)
            roomInviteInboxDialog->showError(QStringLiteral("服务器尚未确认处理结果，请重新连接后刷新邀请列表。"));
    });
    updateRoomTitle();
    // Let the composer follow the input's content height while the message
    // area continues to take all remaining vertical space.
    ui->messageScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->composer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->composer->setMinimumHeight(76);
    ui->composer->setMaximumHeight(QWIDGETSIZE_MAX);
    ui->chatPanelLayout->setStretch(0, 0);
    ui->chatPanelLayout->setStretch(1, 1);
    ui->chatPanelLayout->setStretch(2, 0);
    ui->messageInput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->messageInput->setMinimumHeight(50);
    ui->messageInput->setMaximumHeight(QWIDGETSIZE_MAX);
    ui->memberIcon->setPixmap(iconFromSvg(QStringLiteral(":/login/members.svg"), 22).pixmap(22, 22));
    ui->settingsButton->setIcon(iconFromSvg(QStringLiteral(":/login/settings.svg")));
    ui->helpButton->setIcon(iconFromSvg(QStringLiteral(":/login/help.svg")));
    ui->exitButton->setIcon(iconFromSvg(QStringLiteral(":/login/exit.svg")));
    ui->sendButton->setIcon(iconFromSvg(QStringLiteral(":/login/send.svg"), 19));
    ui->sendButton->setIconSize(QSize(19, 19));
    setupViviComposer();
    ui->settingsButton->setMenu(ui->menuSettings);
    ui->menuSettings->addAction(ui->actionConnect);
    ui->menuSettings->addAction(ui->actionDisconnect);
    connect(ui->helpButton, &QPushButton::clicked, ui->actionAbout, &QAction::trigger);
    connect(ui->exitButton, &QPushButton::clicked, ui->actionExit, &QAction::trigger);

    QFont bodyFont(QStringLiteral("Microsoft YaHei UI"));
    bodyFont.setPixelSize(14);
    setFont(bodyFont);
    QPalette inputPalette = ui->messageInput->palette();
    inputPalette.setColor(QPalette::PlaceholderText, QColor("#837a9a"));
    ui->messageInput->setPalette(inputPalette);
    ui->messageInput->setFocus();
    ui->sendButton->setEnabled(false);
    connect(ui->messageInput, &QTextEdit::textChanged, this, [this]() {
        ui->sendButton->setEnabled(!ui->messageInput->toPlainText().trimmed().isEmpty());
        updateMentionPopup();
    });
    connect(ui->messageInput, &MentionInput::inputHeightChanged, this, [this]() {
        const QScrollBar *scroll = ui->messageScroll->verticalScrollBar();
        const bool followBottom = scroll->value() >= scroll->maximum() - 24;
        ui->chatPanelLayout->activate();
        QTimer::singleShot(0, this, [this, followBottom]() {
            if (followBottom) {
                QScrollBar *bar = ui->messageScroll->verticalScrollBar();
                bar->setValue(bar->maximum());
            }
            positionMentionPopup();
        });
    });
    connect(ui->messageInput, &QTextEdit::cursorPositionChanged, this, &MainWindow::updateMentionPopup);
    connect(ui->messageInput, &MentionInput::compositionFinished, this, &MainWindow::updateMentionPopup);
    connect(ui->messageInput->document(), &QTextDocument::contentsChange,
            this, &MainWindow::updateSelectedMentionRanges);
    ui->messageInput->installEventFilter(this);
    ui->centralwidget->installEventFilter(this);
    setupMentionPopup();
    ui->userList->setSpacing(4);
    ui->userList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    ui->userList->setFixedHeight(0);
    auto *viviDivider = new QFrame(ui->rightPanel);
    viviDivider->setObjectName(QStringLiteral("viviDivider"));
    viviDivider->setFixedHeight(1);
    viviDivider->setStyleSheet(QStringLiteral("background:#eee8f8; border:none;"));
    ui->rightLayout->insertWidget(2, viviDivider);

    auto *viviCard = new QFrame(ui->rightPanel);
    viviCard->setObjectName(QStringLiteral("viviCard"));
    viviCard->setAttribute(Qt::WA_StyledBackground, true);
    viviCard->setAttribute(Qt::WA_Hover, true);
    viviCard->setStyleSheet(QStringLiteral(
        "QFrame#viviCard { background:#f5f0fe; border-radius:13px; } "
        "QFrame#viviCard:hover { background:#eee6fb; }"));
    viviCard->setToolTip(QStringLiteral("VChat AI 智能助手，功能开发中"));
    auto *viviLayout = new QHBoxLayout(viviCard);
    viviLayout->setContentsMargins(11, 11, 11, 11);
    viviLayout->setSpacing(12);
    auto *viviAvatar = new QLabel(viviCard);
    viviAvatar->setFixedSize(44, 44);
    viviAvatar->setAlignment(Qt::AlignCenter);
    viviAvatar->setPixmap(iconFromSvg(QStringLiteral(":/login/vivi-robot.svg"), 25).pixmap(25, 25));
    viviAvatar->setStyleSheet(QStringLiteral("background:#e8defc; border-radius:22px;"));
    viviLayout->addWidget(viviAvatar);
    auto *viviDetails = new QVBoxLayout;
    viviDetails->setContentsMargins(0, 0, 0, 0);
    viviDetails->setSpacing(5);
    auto *viviTitleRow = new QHBoxLayout;
    viviTitleRow->setContentsMargins(0, 0, 0, 0);
    viviTitleRow->setSpacing(7);
    auto *viviName = new QLabel(QStringLiteral("Vivi"), viviCard);
    viviName->setStyleSheet(QStringLiteral("color:#252044; font-size:14px; font-weight:600;"));
    viviTitleRow->addWidget(viviName);
    auto *viviTag = new RoundedLabel(QStringLiteral("AI 助手"), viviCard);
    viviTag->setFillColor(QColor("#e8defc"));
    viviTag->setMinimumHeight(20);
    viviTag->setAlignment(Qt::AlignCenter);
    viviTag->setStyleSheet(QStringLiteral("color:#7956d5; padding:2px 9px; font-size:11px;"));
    viviTitleRow->addWidget(viviTag);
    viviTitleRow->addStretch();
    viviDetails->addLayout(viviTitleRow);
    auto *viviStatusRow = new QHBoxLayout;
    viviStatusRow->setContentsMargins(0, 0, 0, 0);
    viviStatusRow->setSpacing(7);
    viviStatusDot = new QLabel(viviCard);
    viviStatusDot->setFixedSize(8, 8);
    viviStatusRow->addWidget(viviStatusDot);
    viviStatusLabel = new QLabel(viviCard);
    viviStatusRow->addWidget(viviStatusLabel);
    viviStatusRow->addStretch();
    viviDetails->addLayout(viviStatusRow);
    viviLayout->addLayout(viviDetails, 1);
    ui->rightLayout->insertWidget(3, viviCard);
    setAiStatus(QStringLiteral("unavailable"));
    ui->rightLayout->insertStretch(4, 1);
    ui->messageLayout->setSpacing(8);
    ui->onlineBadge->setMinimumHeight(32);
    ui->onlineBadge->setAlignment(Qt::AlignCenter);
    for (QFrame *card : {ui->chatPanel, ui->rightPanel}) {
        auto *shadow = new QGraphicsDropShadowEffect(card);
        shadow->setBlurRadius(22);
        shadow->setOffset(0, 4);
        shadow->setColor(QColor(97, 69, 157, 24));
        card->setGraphicsEffect(shadow);
    }
    ui->serverStatusLabel->setText(QStringLiteral("● 服务器未连接"));
    ui->serverStatusLabel->setStyleSheet("color:#9b6b7d; font-size:12px;");
    ui->onlineCountLabel->setText(QStringLiteral("在线人数：0"));
    ui->onlineBadge->setText(QStringLiteral("0 人在线"));
    setupViviSummary();
}
void MainWindow::setupViviComposer()
{
    // The original MentionInput and send button move into one grid. Switching
    // modes changes only their layout positions, never their document or cursor.
    ui->inputLayout->removeWidget(ui->messageInput);
    ui->inputLayout->removeWidget(ui->sendButton);
    ui->inputLayout->setContentsMargins(12, 9, 12, 9);
    ui->inputLayout->setSpacing(0);

    auto *content = new QWidget(ui->composer);
    content->setObjectName(QStringLiteral("composerContent"));
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(8);
    ui->inputLayout->addWidget(content);

    viviMenu = new QFrame(content);
    viviMenu->setObjectName(QStringLiteral("viviFeatureMenu"));
    viviMenu->setAttribute(Qt::WA_StyledBackground, true);
    viviMenu->setStyleSheet(QStringLiteral(
        "QFrame#viviFeatureMenu { background:#fdfbff; border:1px solid #e5dbf7; border-radius:13px; }"
        "QPushButton#viviActionCard { background:#ffffff; border:1px solid #e8def6; border-radius:10px; }"
        "QPushButton#viviActionCard:hover { background:#f2ebff; border-color:#c4aff4; }"
        "QPushButton#viviActionCard:pressed { background:#e9ddfb; }"
        "QPushButton#viviActionCard[primary='true'] { background:#f0e7ff; border-color:#d8c5fa; }"
        "QPushButton#viviActionCard[primary='true']:hover { background:#e7d8ff; }"));
    auto *menuLayout = new QVBoxLayout(viviMenu);
    menuLayout->setContentsMargins(12, 9, 12, 9);
    menuLayout->setSpacing(8);
    auto *heading = new QHBoxLayout;
    heading->setSpacing(7);
    auto *robot = new QLabel(viviMenu);
    robot->setFixedSize(21, 21);
    robot->setPixmap(iconFromSvg(QStringLiteral(":/login/vivi-robot.svg"), 19).pixmap(19, 19));
    heading->addWidget(robot);
    auto *title = new QLabel(QStringLiteral("Vivi · AI 协作助手"), viviMenu);
    title->setStyleSheet(QStringLiteral("color:#2d2450; font-size:13px; font-weight:700; border:0;"));
    heading->addWidget(title);
    heading->addStretch();
    auto *close = new QToolButton(viviMenu);
    close->setObjectName(QStringLiteral("viviMenuClose"));
    close->setIcon(iconFromSvg(QStringLiteral(":/login/dialog-close.svg"), 16));
    close->setIconSize(QSize(16, 16));
    close->setFixedSize(25, 25);
    close->setCursor(Qt::PointingHandCursor);
    close->setFocusPolicy(Qt::NoFocus);
    close->setStyleSheet(QStringLiteral("QToolButton { background:transparent; border:0; border-radius:6px; }"
        "QToolButton:hover { background:#eee6fa; }"));
    heading->addWidget(close);
    connect(close, &QToolButton::clicked, this, [this] { setViviMenuExpanded(false); });
    menuLayout->addLayout(heading);
    viviMenuItems = new QGridLayout;
    viviMenuItems->setContentsMargins(0, 0, 0, 0);
    viviMenuItems->setHorizontalSpacing(8);
    viviMenuItems->setVerticalSpacing(6);
    menuLayout->addLayout(viviMenuItems);

    auto addCard = [this](const QString &caption, const QString &description,
                          const QString &iconPath, bool primary) {
        auto *card = new QPushButton(viviMenu);
        card->setObjectName(QStringLiteral("viviActionCard"));
        card->setAccessibleName(caption);
        card->setProperty("primary", primary);
        card->setCursor(Qt::PointingHandCursor);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(9, 5, 8, 5);
        row->setSpacing(7);
        auto *icon = new QLabel(card);
        icon->setFixedSize(21, 21);
        icon->setAlignment(Qt::AlignCenter);
        icon->setPixmap(iconFromSvg(iconPath, 19).pixmap(19, 19));
        icon->setAttribute(Qt::WA_TransparentForMouseEvents);
        row->addWidget(icon);
        auto *text = new QVBoxLayout;
        text->setSpacing(1);
        auto *name = new QLabel(caption, card);
        name->setStyleSheet(QStringLiteral("color:#2d2450; font-size:12px; font-weight:600; border:0;"));
        name->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto *subtitle = new QLabel(description, card);
        subtitle->setStyleSheet(QStringLiteral("color:#74698e; font-size:10px; border:0;"));
        subtitle->setWordWrap(true);
        subtitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        subtitle->setAttribute(Qt::WA_TransparentForMouseEvents);
        text->addWidget(name);
        text->addWidget(subtitle);
        row->addLayout(text, 1);
        viviMenuCards.append(card);
        connect(card, &QPushButton::clicked, this, [this, card, caption] {
            const QPoint noticeAt = card->mapToGlobal(QPoint(card->width() / 2, card->height()));
            setViviMenuExpanded(false);
            if (caption == QStringLiteral("总结当前讨论")) {
                openViviSummary();
                return;
            }
            const QString notice = caption == QStringLiteral("Vivi 参与设置")
                ? QStringLiteral("Vivi 参与设置将在后续阶段开放")
                : QStringLiteral("%1将在后续阶段开放").arg(caption);
            QToolTip::showText(noticeAt, notice, this);
        });
    };
    addCard(QStringLiteral("总结当前讨论"), QStringLiteral("整理讨论重点、共识与行动"),
            QStringLiteral(":/login/vivi-sparkle.svg"), true);
    addCard(QStringLiteral("历史总结"), QStringLiteral("查看已保存的讨论成果"),
            QStringLiteral(":/login/clock.svg"), false);
    addCard(QStringLiteral("Vivi 参与设置"), QStringLiteral("管理回复方式与参与范围"),
            QStringLiteral(":/login/settings.svg"), false);
    viviMenuOpacity = new QGraphicsOpacityEffect(viviMenu);
    viviMenuOpacity->setOpacity(0);
    viviMenu->setGraphicsEffect(viviMenuOpacity);
    viviMenu->setFixedHeight(0);
    viviMenu->hide();
    contentLayout->addWidget(viviMenu);

    composerSurface = new QFrame(content);
    composerSurface->setObjectName(QStringLiteral("composerSurface"));
    composerSurface->setStyleSheet(QStringLiteral("QFrame#composerSurface { background:transparent; border:0; }"));
    composerSurface->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    composerGrid = new QGridLayout(composerSurface);
    composerGrid->setContentsMargins(0, 0, 0, 0);
    composerGrid->setHorizontalSpacing(8);
    composerGrid->setVerticalSpacing(6);
    contentLayout->addWidget(composerSurface);

    auto iconButton = [this](const QString &name, const QString &resource, const QString &tip) {
        auto *button = new QToolButton(composerSurface);
        button->setObjectName(name);
        button->setIcon(iconFromSvg(resource, 20));
        button->setIconSize(QSize(20, 20));
        button->setFixedSize(34, 34);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        button->setToolTip(tip);
        button->setStyleSheet(QStringLiteral(
            "QToolButton { background:transparent; border:0; border-radius:9px; }"
            "QToolButton:hover { background:#eee6fb; }"
            "QToolButton:pressed { background:#e0d2f7; }"));
        return button;
    };
    viviToggleButton = iconButton(QStringLiteral("viviComposerButton"),
        QStringLiteral(":/login/vivi-sparkle.svg"), QStringLiteral("Vivi AI 功能"));
    viviToggleButton->setCheckable(true);
    viviToggleButton->setStyleSheet(QStringLiteral(
        "QToolButton { background:#f0e6ff; border:0; border-radius:9px; }"
        "QToolButton:hover { background:#e7d9ff; }"
        "QToolButton:checked { background:#dec9ff; }"));
    auto *emoji = iconButton(QStringLiteral("emojiComposerButton"),
        QStringLiteral(":/login/emoji.svg"), QStringLiteral("表情（即将开放）"));
    auto *attachment = iconButton(QStringLiteral("attachmentComposerButton"),
        QStringLiteral(":/login/attachment.svg"), QStringLiteral("附件（即将开放）"));
    connect(viviToggleButton, &QToolButton::clicked, this, [this] {
        setViviMenuExpanded(!viviMenuExpanded);
    });
    for (QToolButton *button : {emoji, attachment})
        connect(button, &QToolButton::clicked, this, [this, button] {
            QToolTip::showText(button->mapToGlobal(QPoint(0, button->height())), button->toolTip(), this);
        });
    ui->messageInput->setParent(composerSurface);
    ui->sendButton->setParent(composerSurface);
    ui->sendButton->setMinimumWidth(0);
    ui->sendButton->setFixedSize(104, 44);
    composerGrid->setColumnStretch(3, 1);
    layoutViviComposer(false);
    updateViviMenuLayout();
    qApp->installEventFilter(this);
}

void MainWindow::layoutViviComposer(bool expanded)
{
    auto *emoji = composerSurface->findChild<QToolButton *>(QStringLiteral("emojiComposerButton"));
    auto *attachment = composerSurface->findChild<QToolButton *>(QStringLiteral("attachmentComposerButton"));
    for (QWidget *widget : {static_cast<QWidget *>(viviToggleButton), static_cast<QWidget *>(emoji),
                            static_cast<QWidget *>(attachment), static_cast<QWidget *>(ui->messageInput),
                            static_cast<QWidget *>(ui->sendButton)})
        composerGrid->removeWidget(widget);
    ui->messageInput->setProperty("compact", expanded);
    ui->messageInput->style()->unpolish(ui->messageInput);
    ui->messageInput->style()->polish(ui->messageInput);
    ui->messageInput->setCompactMode(expanded);
    if (expanded) {
        composerGrid->addWidget(viviToggleButton, 0, 0, Qt::AlignVCenter);
        composerGrid->addWidget(emoji, 0, 1, Qt::AlignVCenter);
        composerGrid->addWidget(attachment, 0, 2, Qt::AlignVCenter);
        composerGrid->addWidget(ui->messageInput, 0, 3);
        composerGrid->addWidget(ui->sendButton, 0, 4, Qt::AlignVCenter);
    } else {
        composerGrid->addWidget(ui->messageInput, 0, 0, 1, 5);
        composerGrid->addWidget(viviToggleButton, 1, 0, Qt::AlignVCenter);
        composerGrid->addWidget(emoji, 1, 1, Qt::AlignVCenter);
        composerGrid->addWidget(attachment, 1, 2, Qt::AlignVCenter);
        composerGrid->addWidget(ui->sendButton, 1, 4, Qt::AlignVCenter);
    }
    composerGrid->invalidate();
    if (mentionPopup) mentionPopup->hide();
}

int MainWindow::viviMenuTargetHeight() const
{
    return viviMenuVertical ? 234 : 116;
}

void MainWindow::updateViviMenuLayout()
{
    if (!viviMenuItems || viviMenuCards.size() != 3) return;
    const bool vertical = ui->chatPanel->width() < 590;
    const bool narrow = ui->chatPanel->width() < 490;
    const QString placeholder = viviMenuExpanded && narrow
        ? QStringLiteral("消息...") : QStringLiteral("请输入消息...");
    if (ui->messageInput->placeholderText() != placeholder)
        ui->messageInput->setPlaceholderText(placeholder);
    ui->sendButton->setFixedWidth(narrow ? 80 : 104);
    ui->inputLayout->setContentsMargins(narrow ? 8 : 12, 9, narrow ? 8 : 12, 9);
    composerGrid->setHorizontalSpacing(narrow ? 4 : 8);
    for (const QString &name : {QStringLiteral("viviComposerButton"),
                                 QStringLiteral("emojiComposerButton"),
                                 QStringLiteral("attachmentComposerButton")}) {
        auto *button = composerSurface->findChild<QToolButton *>(name);
        button->setFixedSize(narrow ? 28 : 34, narrow ? 28 : 34);
        button->setIconSize(QSize(narrow ? 18 : 20, narrow ? 18 : 20));
    }
    if (viviMenuVertical == vertical && viviMenuItems->count() == 3) return;
    viviMenuVertical = vertical;
    for (QPushButton *card : viviMenuCards) viviMenuItems->removeWidget(card);
    for (int i = 0; i < viviMenuCards.size(); ++i) {
        viviMenuCards[i]->setFixedHeight(vertical ? 58 : 66);
        viviMenuItems->addWidget(viviMenuCards[i], vertical ? i : 0, vertical ? 0 : i);
    }
    for (int i = 0; i < 3; ++i) viviMenuItems->setColumnStretch(i, vertical ? (i == 0) : 1);
    viviMenuItems->invalidate();
    if (viviMenuExpanded && !viviMenuAnimation) viviMenu->setFixedHeight(viviMenuTargetHeight());
}

void MainWindow::setViviMenuExpanded(bool expanded, bool animate)
{
    if (!viviMenu || (viviMenuExpanded == expanded && !viviMenuAnimation)) return;
    if (viviMenuAnimation) {
        viviMenuAnimation->stop();
        viviMenuAnimation->deleteLater();
        viviMenuAnimation = nullptr;
    }
    const int surfaceStart = qMax(44, composerSurface->height());
    const int menuStart = viviMenu->isVisible() ? viviMenu->height() : 0;
    const qreal opacityStart = viviMenuOpacity->opacity();
    viviMenuOpacity->setEnabled(true);
    composerSurface->setFixedHeight(surfaceStart);
    if (expanded) viviMenu->show();
    layoutViviComposer(expanded);
    updateViviMenuLayout();
    composerGrid->activate();
    const int surfaceEnd = qMax(44, composerGrid->sizeHint().height());
    const int menuEnd = expanded ? viviMenuTargetHeight() : 0;
    viviMenuExpanded = expanded;
    const QString placeholder = expanded && ui->chatPanel->width() < 490
        ? QStringLiteral("消息...") : QStringLiteral("请输入消息...");
    if (ui->messageInput->placeholderText() != placeholder)
        ui->messageInput->setPlaceholderText(placeholder);
    viviToggleButton->setChecked(expanded);
    auto finish = [this, expanded, surfaceEnd] {
        if (expanded) {
            composerSurface->setFixedHeight(surfaceEnd);
            viviMenu->setFixedHeight(viviMenuTargetHeight());
            viviMenuOpacity->setOpacity(1);
            viviMenuOpacity->setEnabled(false);
        } else {
            viviMenu->hide();
            viviMenu->setFixedHeight(0);
            viviMenuOpacity->setOpacity(0);
            composerSurface->setMinimumHeight(0);
            composerSurface->setMaximumHeight(QWIDGETSIZE_MAX);
        }
        ui->composer->updateGeometry();
        ui->messageInput->ensureCursorVisible();
        positionMentionPopup();
    };
    if (!animate || !isVisible()) {
        finish();
        return;
    }
    auto *motion = new QVariantAnimation(this);
    viviMenuAnimation = motion;
    motion->setDuration(240);
    motion->setEasingCurve(QEasingCurve::OutCubic);
    motion->setStartValue(0.0);
    motion->setEndValue(1.0);
    connect(motion, &QVariantAnimation::valueChanged, this,
        [this, surfaceStart, surfaceEnd, menuStart, menuEnd, opacityStart, expanded](const QVariant &value) {
            const qreal progress = value.toReal();
            composerSurface->setFixedHeight(qRound(surfaceStart + (surfaceEnd - surfaceStart) * progress));
            viviMenu->setFixedHeight(qRound(menuStart + (menuEnd - menuStart) * progress));
            viviMenuOpacity->setOpacity(opacityStart + ((expanded ? 1.0 : 0.0) - opacityStart) * progress);
            ui->composer->updateGeometry();
        });
    connect(motion, &QVariantAnimation::finished, this, [this, motion, finish] {
        if (viviMenuAnimation == motion) viviMenuAnimation = nullptr;
        finish();
        motion->deleteLater();
    });
    motion->start();
}

void MainWindow::setupViviSummary()
{
    viviSummaryPanel = new QFrame(ui->rightPanel);
    viviSummaryPanel->setObjectName(QStringLiteral("viviSummaryPanel"));
    viviSummaryPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    viviSummaryPanel->setStyleSheet(QStringLiteral(
        "QFrame#viviSummaryPanel { background:#ffffff; border:0; }"
        "QFrame#viviSummaryBanner { background:#f3edff; border-radius:13px; }"
        "QScrollArea#viviSummaryScroll { background:transparent; border:0; }"
        "QPushButton#viviSummaryAction { background:#f8f5ff; color:#694cc8;"
        " border:1px solid #e8dffa; border-radius:9px; font-weight:600; padding:7px; }"
        "QPushButton#viviSummaryAction:hover { background:#eee7fc; border-color:#d9c8f7; }"
        "QPushButton#viviSummaryAction:disabled { color:#a99dc2; background:#faf8fd; }"
        "QPushButton#viviSummaryShare { background:#7956d5; color:white; border:0;"
        " border-radius:9px; font-weight:600; padding:5px 3px; }"
        "QPushButton#viviSummaryShare:hover { background:#6846c4; }"
        "QPushButton#viviSummaryShare:disabled { background:#d6caef; color:#f6f2fc; }"));
    auto *layout = new QVBoxLayout(viviSummaryPanel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(11);
    auto *header = new QHBoxLayout;
    auto *icon = new QLabel(viviSummaryPanel);
    icon->setFixedSize(23, 23);
    icon->setPixmap(iconFromSvg(QStringLiteral(":/login/vivi-sparkle.svg"), 21).pixmap(21, 21));
    header->addWidget(icon);
    auto *title = new QLabel(QStringLiteral("Vivi · 智能讨论总结"), viviSummaryPanel);
    title->setWordWrap(true);
    title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    title->setStyleSheet(QStringLiteral("color:#252044; font-size:13px; font-weight:700;"));
    header->addWidget(title, 1);
    auto *close = new QToolButton(viviSummaryPanel);
    close->setObjectName(QStringLiteral("viviSummaryClose"));
    close->setIcon(iconFromSvg(QStringLiteral(":/login/dialog-close.svg"), 17));
    close->setIconSize(QSize(17, 17));
    close->setFixedSize(26, 26);
    close->setCursor(Qt::PointingHandCursor);
    close->setStyleSheet(QStringLiteral("QToolButton { border:0; background:transparent; }"
        "QToolButton:hover { background:#eee6fa; border-radius:7px; }"));
    header->addWidget(close);
    connect(close, &QToolButton::clicked, this, &MainWindow::closeViviSummary);
    layout->addLayout(header);

    auto *banner = new QFrame(viviSummaryPanel);
    banner->setObjectName(QStringLiteral("viviSummaryBanner"));
    auto *bannerLayout = new QVBoxLayout(banner);
    bannerLayout->setContentsMargins(12, 12, 12, 12);
    bannerLayout->setSpacing(5);
    viviSummaryRoom = new QLabel(banner);
    viviSummaryRoom->setObjectName(QStringLiteral("viviSummaryRoom"));
    viviSummaryRoom->setWordWrap(true);
    viviSummaryRoom->setStyleSheet(QStringLiteral("color:#252044; font-size:13px; font-weight:700;"));
    bannerLayout->addWidget(viviSummaryRoom);
    viviSummaryRange = new QLabel(banner);
    viviSummaryRange->setObjectName(QStringLiteral("viviSummaryRange"));
    viviSummaryRange->setWordWrap(true);
    viviSummaryRange->setStyleSheet(QStringLiteral("color:#766990; font-size:11px;"));
    bannerLayout->addWidget(viviSummaryRange);
    layout->addWidget(banner);

    viviSummaryScroll = new QScrollArea(viviSummaryPanel);
    viviSummaryScroll->setObjectName(QStringLiteral("viviSummaryScroll"));
    viviSummaryScroll->setWidgetResizable(true);
    viviSummaryScroll->setFrameShape(QFrame::NoFrame);
    viviSummaryScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    viviSummaryScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    viviSummaryScroll->viewport()->setStyleSheet(QStringLiteral("background:#ffffff;"));
    auto *sectionsContent = new QWidget;
    sectionsContent->setObjectName(QStringLiteral("viviSummarySections"));
    sectionsContent->setAttribute(Qt::WA_StyledBackground, true);
    sectionsContent->setStyleSheet(QStringLiteral(
        "QWidget#viviSummarySections { background:#ffffff; }"));
    sectionsContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *sectionsLayout = new QVBoxLayout(sectionsContent);
    sectionsLayout->setContentsMargins(1, 4, 3, 4);
    sectionsLayout->setSpacing(10);
    const QStringList names{QStringLiteral("讨论概览"), QStringLiteral("已达成共识"),
                            QStringLiteral("待解决问题"), QStringLiteral("下一步行动")};
    const QStringList marks{QStringLiteral("◇"), QStringLiteral("✓"),
                            QStringLiteral("?"), QStringLiteral("→")};
    const QStringList colors{QStringLiteral("#744bd4"), QStringLiteral("#7753cd"),
                             QStringLiteral("#8266bd"), QStringLiteral("#7351bc")};
    const QStringList tints{QStringLiteral("#f0e9ff"), QStringLiteral("#f1ebff"),
                            QStringLiteral("#f4effc"), QStringLiteral("#f1ebff")};
    for (int i = 0; i < names.size(); ++i) {
        auto *section = new SummarySection(names.at(i), marks.at(i), colors.at(i),
                                           tints.at(i), sectionsContent);
        viviSummarySections.append(section);
        sectionsLayout->addWidget(section);
        if (i + 1 < names.size()) {
            auto *divider = new QFrame(sectionsContent);
            divider->setObjectName(QStringLiteral("viviSummaryDivider"));
            divider->setFrameShape(QFrame::HLine);
            divider->setFixedHeight(1);
            divider->setStyleSheet(QStringLiteral("background:#f0eaf8; border:0;"));
            sectionsLayout->addWidget(divider);
        }
    }
    sectionsLayout->addStretch();
    viviSummaryScroll->setWidget(sectionsContent);
    layout->addWidget(viviSummaryScroll, 1);
    viviSummaryRenderTimer = new QTimer(this);
    viviSummaryRenderTimer->setSingleShot(true);
    viviSummaryRenderTimer->setInterval(90);
    connect(viviSummaryRenderTimer, &QTimer::timeout, this,
            [this] { renderViviSummary(false); });

    viviSummaryStatus = new QLabel(viviSummaryPanel);
    viviSummaryStatus->setObjectName(QStringLiteral("viviSummaryStatus"));
    viviSummaryStatus->setTextFormat(Qt::PlainText);
    viviSummaryStatus->setWordWrap(true);
    viviSummaryStatus->setStyleSheet(QStringLiteral("color:#766990; font-size:11px;"));
    layout->addWidget(viviSummaryStatus);
    auto *actions = new QHBoxLayout;
    actions->setSpacing(8);
    viviSummaryCopy = new QPushButton(QStringLiteral("复制"), viviSummaryPanel);
    viviSummaryRegenerate = new QPushButton(QStringLiteral("重新生成"), viviSummaryPanel);
    for (QPushButton *button : {viviSummaryCopy, viviSummaryRegenerate}) {
        button->setObjectName(QStringLiteral("viviSummaryAction"));
        button->setFixedHeight(38);
        button->setCursor(Qt::PointingHandCursor);
        actions->addWidget(button, 1);
    }
    viviSummaryShare = new QPushButton(QStringLiteral("分享至聊天室"), viviSummaryPanel);
    viviSummaryShare->setObjectName(QStringLiteral("viviSummaryShare"));
    viviSummaryShare->setFixedHeight(38);
    viviSummaryShare->setCursor(Qt::PointingHandCursor);
    viviSummaryShare->setEnabled(false);
    actions->addWidget(viviSummaryShare, 2);
    connect(viviSummaryCopy, &QPushButton::clicked, this, [this] {
        QGuiApplication::clipboard()->setText(viviSummaryReadOnly
            ? viviSummaryViewedText : viviSummaryText);
        viviSummaryStatus->setText(QStringLiteral("已复制讨论总结。"));
    });
    connect(viviSummaryRegenerate, &QPushButton::clicked, this, [this] {
        if (!viviSummaryPending) requestViviSummary();
    });
    connect(viviSummaryShare, &QPushButton::clicked, this, [this] {
        if (viviSummaryReadOnly || viviSummaryPending || !viviSummaryCompleted ||
            viviSummarySharePending || viviSummaryShared ||
            viviSummaryText.trimmed().isEmpty() || viviSummaryRoomId != currentRoomId ||
            !isConnected || socket->state() != QAbstractSocket::ConnectedState) return;
        if (viviSummaryShareRequestId.isEmpty())
            viviSummaryShareRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        viviSummarySharePending = true;
        viviSummaryShare->setEnabled(false);
        viviSummaryRegenerate->setEnabled(false);
        viviSummaryShare->setText(QStringLiteral("提交中…"));
        viviSummaryStatus->setText(QStringLiteral("正在保存并分享总结…"));
        const QJsonObject request{{QStringLiteral("request_id"), viviSummaryShareRequestId},
            {QStringLiteral("summary_request_id"), viviSummaryRequestId},
            {QStringLiteral("room_id"), viviSummaryRoomId}};
        socket->write("SUMMARY_SHARE|" + QJsonDocument(request).toJson(QJsonDocument::Compact).toBase64() + '\n');
        const QString pendingId = viviSummaryShareRequestId;
        QTimer::singleShot(12000, this, [this, pendingId] {
            if (!viviSummarySharePending || pendingId != viviSummaryShareRequestId) return;
            viviSummarySharePending = false;
            viviSummaryShare->setText(QStringLiteral("分享至聊天室"));
            viviSummaryShare->setEnabled(!viviSummaryReadOnly && !viviSummaryShared &&
                viviSummaryRoomId == currentRoomId && isConnected);
            viviSummaryRegenerate->setEnabled(!viviSummaryReadOnly);
            if (!viviSummaryReadOnly)
                viviSummaryStatus->setText(QStringLiteral("分享确认超时，可重试；不会重复发布。"));
        });
    });
    layout->addLayout(actions);
    ui->rightLayout->insertWidget(0, viviSummaryPanel, 1);
    viviSummaryPanel->hide();
}

void MainWindow::openViviSummary()
{
    if (!viviSummaryPanel->isHidden() && !viviSummaryReadOnly) {
        if (!viviSummaryPending && !viviSummarySharePending) requestViviSummary();
        return;
    }
    viviSummaryReadOnly = false;
    viviSummaryRegenerate->show();
    viviSummaryShare->show();
    if (!viviSummaryPanel->isHidden()) {
        if (viviSummaryRoomId == currentRoomId && viviSummaryCompleted)
            renderViviSummary(!viviSummaryPending);
        else requestViviSummary();
        return;
    }
    showViviSummaryPanel();
    if (viviSummaryRoomId == currentRoomId && viviSummaryCompleted) {
        viviSummaryRoom->setText(QStringLiteral("%1 · 讨论总结")
            .arg(roomNameLabel ? roomNameLabel->text() : currentRoomId));
        viviSummaryRange->setText(QStringLiteral("最近 %1 条消息 · 序号 %2–%3")
            .arg(viviSummaryMessageCount).arg(viviSummaryFirstSequence, viviSummaryLastSequence));
        viviSummaryStatus->setText(viviSummaryGeneratedAt.isEmpty()
            ? QStringLiteral("总结已完成。")
            : QStringLiteral("总结已完成 · %1").arg(localSummaryTime(viviSummaryGeneratedAt)));
        viviSummaryCopy->setEnabled(true);
        viviSummaryRegenerate->setEnabled(true);
        viviSummaryShare->setEnabled(!viviSummarySharePending && !viviSummaryShared);
        renderViviSummary(true);
    } else requestViviSummary();
}

void MainWindow::showViviSummaryPanel()
{
    if (!viviSummaryPanel->isHidden()) return;
    viviSummaryWasCollapsed = membersCollapsed;
    viviSummaryMemberVisibility.clear();
    auto hideMemberWidget = [this](QWidget *widget) {
        if (!widget || widget == viviSummaryPanel || viviSummaryMemberVisibility.contains(widget)) return;
        viviSummaryMemberVisibility.insert(widget, !widget->isHidden());
        widget->hide();
    };
    for (int i = 0; i < ui->rightLayout->count(); ++i)
        hideMemberWidget(ui->rightLayout->itemAt(i)->widget());
    for (int i = 0; i < ui->memberHeader->count(); ++i)
        hideMemberWidget(ui->memberHeader->itemAt(i)->widget());
    ui->rightLayout->setStretch(5, 0); // Existing member-list spacer.
    if (membersCollapsed) memberExpandButton->hide();
    ui->rightPanel->show();
    viviSummaryPanel->show();
}

void MainWindow::closeViviSummary()
{
    if (!viviSummaryPanel || viviSummaryPanel->isHidden()) return;
    viviSummaryRenderTimer->stop();
    if (viviSummaryPending && isConnected) {
        const QJsonObject request{{QStringLiteral("action"), QStringLiteral("cancel")},
            {QStringLiteral("request_id"), viviSummaryRequestId},
            {QStringLiteral("room_id"), viviSummaryRoomId}};
        socket->write(QStringLiteral("AI_SUMMARY|%1\n").arg(QString::fromLatin1(
            QJsonDocument(request).toJson(QJsonDocument::Compact).toBase64())).toUtf8());
    }
    if (viviSummaryPending) {
        viviSummaryPending = false;
        viviSummaryRequestId.clear();
        viviSummaryRoomId.clear();
    }
    viviSummaryPanel->hide();
    viviSummaryReadOnly = false;
    for (auto it = viviSummaryMemberVisibility.cbegin(); it != viviSummaryMemberVisibility.cend(); ++it)
        it.key()->setVisible(it.value());
    viviSummaryMemberVisibility.clear();
    ui->rightLayout->setStretch(5, 1);
    if (viviSummaryWasCollapsed) {
        ui->rightPanel->hide();
        memberExpandButton->show();
    }
}

void MainWindow::requestViviSummary()
{
    if (viviSummaryPending || viviSummarySharePending) return;
    viviSummaryReadOnly = false;
    viviSummarySharePending = false;
    viviSummaryShared = false;
    viviSummaryCompleted = false;
    viviSummaryShareRequestId.clear();
    viviSummaryGeneratedAt.clear();
    viviSummaryMessageCount = 0;
    viviSummaryFirstSequence.clear();
    viviSummaryLastSequence.clear();
    viviSummaryRoom->setText(QStringLiteral("%1 · 讨论总结")
        .arg(roomNameLabel ? roomNameLabel->text() : currentRoomId));
    viviSummaryRange->setText(QStringLiteral("正在读取最近 30 条讨论消息…"));
    viviSummaryRenderTimer->stop();
    for (SummarySection *section : viviSummarySections) section->reset();
    viviSummaryScroll->verticalScrollBar()->setValue(0);
    viviSummaryText.clear();
    viviSummaryCopy->setEnabled(false);
    viviSummaryRegenerate->setEnabled(false);
    viviSummaryShare->setEnabled(false);
    viviSummaryShare->setText(QStringLiteral("分享至聊天室"));
    if (!isConnected || socket->state() != QAbstractSocket::ConnectedState ||
        !viviSummarySupported) {
        viviSummaryStatus->setText(isConnected
            ? QStringLiteral("当前服务器尚不支持讨论总结，请更新 ChatServer。")
            : QStringLiteral("服务器未连接，无法生成总结。"));
        viviSummaryRegenerate->setEnabled(true);
        return;
    }
    viviSummaryRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    viviSummaryRoomId = currentRoomId;
    viviSummaryPending = true;
    viviSummaryStatus->setText(QStringLiteral("Vivi 正在整理讨论…"));
    const QJsonObject request{{QStringLiteral("action"), QStringLiteral("start")},
        {QStringLiteral("request_id"), viviSummaryRequestId},
        {QStringLiteral("room_id"), viviSummaryRoomId}};
    socket->write(QStringLiteral("AI_SUMMARY|%1\n").arg(QString::fromLatin1(
        QJsonDocument(request).toJson(QJsonDocument::Compact).toBase64())).toUtf8());
}

void MainWindow::handleViviSummary(const QJsonObject &event)
{
    if (!viviSummaryPanel || viviSummaryPanel->isHidden() || !viviSummaryPending ||
        event.value(QStringLiteral("request_id")).toString() != viviSummaryRequestId ||
        event.value(QStringLiteral("room_id")).toString() != currentRoomId ||
        viviSummaryRoomId != currentRoomId) return;
    const QString kind = event.value(QStringLiteral("event")).toString();
    if (kind == QStringLiteral("meta")) {
        viviSummaryMessageCount = event.value(QStringLiteral("message_count")).toInt();
        viviSummaryFirstSequence = event.value(QStringLiteral("first_sequence")).toString();
        viviSummaryLastSequence = event.value(QStringLiteral("last_sequence")).toString();
        if (!viviSummaryReadOnly)
            viviSummaryRange->setText(QStringLiteral("最近 %1 条消息 · 序号 %2–%3")
                .arg(viviSummaryMessageCount)
                .arg(viviSummaryFirstSequence, viviSummaryLastSequence));
    } else if (kind == QStringLiteral("chunk") || kind == QStringLiteral("complete")) {
        if (kind == QStringLiteral("chunk"))
            viviSummaryText += event.value(QStringLiteral("text")).toString();
        else {
            viviSummaryText = event.value(QStringLiteral("text")).toString();
            viviSummaryPending = false;
            viviSummaryCompleted = !viviSummaryText.trimmed().isEmpty();
            viviSummaryGeneratedAt = event.value(QStringLiteral("generated_at")).toString();
            if (!viviSummaryReadOnly) {
                viviSummaryCopy->setEnabled(!viviSummaryText.trimmed().isEmpty());
                viviSummaryRegenerate->setEnabled(true);
                viviSummaryShare->setEnabled(!viviSummaryShared && !viviSummaryText.trimmed().isEmpty() &&
                    viviSummaryMessageCount >= 2 && viviSummaryText.size() <= 20000);
                viviSummaryStatus->setText(viviSummaryGeneratedAt.isEmpty()
                    ? QStringLiteral("总结已完成。")
                    : QStringLiteral("总结已完成 · %1").arg(localSummaryTime(viviSummaryGeneratedAt)));
            }
        }
        if (kind == QStringLiteral("complete")) {
            viviSummaryRenderTimer->stop();
            if (!viviSummaryReadOnly) renderViviSummary(true);
        } else if (!viviSummaryRenderTimer->isActive()) {
            if (!viviSummaryReadOnly) viviSummaryRenderTimer->start();
        }
    } else if (kind == QStringLiteral("error")) {
        viviSummaryRenderTimer->stop();
        if (!viviSummaryReadOnly) renderViviSummary(false);
        viviSummaryPending = false;
        viviSummaryCompleted = false;
        if (!viviSummaryReadOnly) {
            viviSummaryStatus->setText(event.value(QStringLiteral("error")).toString());
            viviSummaryCopy->setEnabled(false);
            viviSummaryRegenerate->setEnabled(true);
            viviSummaryShare->setEnabled(false);
        }
    }
}

void MainWindow::renderViviSummary(bool completed)
{
    if (!viviSummaryPanel || viviSummaryPanel->isHidden()) return;
    const QVector<QString> parts = splitSummarySections(
        viviSummaryReadOnly ? viviSummaryViewedText : viviSummaryText);
    for (int i = 0; i < viviSummarySections.size(); ++i)
        viviSummarySections.at(i)->showMarkdown(parts.value(i), completed);
    // QScrollArea keeps the content width flexible; only its natural height
    // is a minimum, so long sections scroll rather than compress their rows.
    QTimer::singleShot(0, this, [this] {
        if (!viviSummaryScroll || !viviSummaryScroll->widget()) return;
        QWidget *content = viviSummaryScroll->widget();
        for (SummarySection *section : viviSummarySections)
            section->setMinimumHeight(section->layout()->sizeHint().height());
        content->setMinimumHeight(content->layout()->sizeHint().height());
    });
}

void MainWindow::openSharedSummary(const QJsonObject &message)
{
    if (message.value(QStringLiteral("room_id")).toString() != currentRoomId) return;
    const QString markdown = message.value(QStringLiteral("markdown")).toString();
    if (markdown.isEmpty()) return;
    showViviSummaryPanel();
    viviSummaryReadOnly = true;
    viviSummaryViewedText = markdown;
    viviSummaryRoom->setText(QStringLiteral("%1 · 讨论总结")
        .arg(message.value(QStringLiteral("room_name")).toString()));
    viviSummaryRange->setText(QStringLiteral("最近 %1 条消息 · 序号 %2–%3")
        .arg(message.value(QStringLiteral("message_count")).toInt())
        .arg(message.value(QStringLiteral("first_sequence")).toString(),
             message.value(QStringLiteral("last_sequence")).toString()));
    viviSummaryStatus->setText(QStringLiteral("由 %1 分享 · %2")
        .arg(message.value(QStringLiteral("shared_by_name")).toString(),
             localSummaryTime(message.value(QStringLiteral("time")).toString())));
    viviSummaryRegenerate->hide();
    viviSummaryShare->hide();
    viviSummaryCopy->setEnabled(true);
    viviSummaryScroll->verticalScrollBar()->setValue(0);
    renderViviSummary(true);
}

void MainWindow::handleSummaryShare(const QJsonObject &event)
{
    const QString kind = event.value(QStringLiteral("event")).toString();
    if (kind == QStringLiteral("message")) {
        if (event.value(QStringLiteral("room_id")).toString() == currentRoomId)
            appendSharedSummary(event);
        return;
    }
    if (kind != QStringLiteral("result") ||
        event.value(QStringLiteral("request_id")).toString() != viviSummaryShareRequestId) return;
    viviSummarySharePending = false;
    viviSummaryRegenerate->setEnabled(!viviSummaryReadOnly);
    if (event.value(QStringLiteral("ok")).toBool()) {
        viviSummaryShared = true;
        viviSummaryShare->setText(QStringLiteral("已分享"));
        viviSummaryShare->setEnabled(false);
        if (!viviSummaryReadOnly) viviSummaryStatus->setText(QStringLiteral("总结已分享至聊天室。"));
    } else {
        viviSummaryShare->setText(QStringLiteral("分享至聊天室"));
        viviSummaryShare->setEnabled(!viviSummaryReadOnly && isConnected &&
            viviSummaryRoomId == currentRoomId && !viviSummaryText.isEmpty());
        if (!viviSummaryReadOnly)
            viviSummaryStatus->setText(event.value(QStringLiteral("error")).toString());
    }
}

void MainWindow::appendSharedSummary(const QJsonObject &message)
{
    if (message.value(QStringLiteral("room_id")).toString() != currentRoomId) return;
    const QString id = message.value(QStringLiteral("message_id")).toString();
    const QString markdown = message.value(QStringLiteral("markdown")).toString();
    if (id.isEmpty() || markdown.isEmpty() || renderedSummaryMessageIds.contains(id)) return;
    renderedSummaryMessageIds.insert(id);
    lastMessageKey.clear();
    QScrollBar *scroll = ui->messageScroll->verticalScrollBar();
    const bool followBottom = scroll->value() >= scroll->maximum() - 24;
    auto *row = new QWidget(ui->messageContainer);
    row->setObjectName(QStringLiteral("discussionSummaryMessage"));
    row->setProperty("summaryMessageId", id);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 8, 0, 0);
    rowLayout->setSpacing(10);
    auto *avatar = avatarLabel(QStringLiteral("Vivi"), row);
    avatar->clear();
    avatar->setPixmap(iconFromSvg(QStringLiteral(":/login/vivi-robot.svg"), 22).pixmap(22, 22));
    avatar->setStyleSheet(QStringLiteral("background:#e8defc; border-radius:18px;"));
    rowLayout->addWidget(avatar, 0, Qt::AlignTop);
    auto *column = new QWidget(row);
    column->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto *columnLayout = new QVBoxLayout(column);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->setSpacing(7);
    const QDateTime sent = QDateTime::fromString(message.value(QStringLiteral("time")).toString(),
                                                Qt::ISODateWithMs).toLocalTime();
    auto *caption = new QLabel(QStringLiteral("Vivi  ·  由 %1 分享讨论总结  ·  %2")
        .arg(message.value(QStringLiteral("shared_by_name")).toString(),
             sent.isValid() ? sent.time().toString(QStringLiteral("hh:mm")) : QString()), column);
    caption->setTextFormat(Qt::PlainText);
    caption->setStyleSheet(QStringLiteral("color:#776891; font-size:12px;"));
    columnLayout->addWidget(caption);
    auto *card = new QFrame(column);
    card->setObjectName(QStringLiteral("discussionSummaryCard"));
    card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    card->setStyleSheet(QStringLiteral(
        "QFrame#discussionSummaryCard { background:#faf8ff; border:1px solid #e9dfff; border-radius:15px; }"
        "QLabel { border:0; background:transparent; color:#30234e; }"
        "QPushButton { color:#694cc8; background:#f8f5ff; border:1px solid #e8dffa;"
        " border-radius:8px; padding:7px 9px; font-size:12px; }"
        "QPushButton:hover { background:#eee7fc; }"));
    auto *box = new QVBoxLayout(card);
    box->setContentsMargins(17, 14, 17, 14);
    box->setSpacing(5);
    auto *titleLine = new QHBoxLayout;
    titleLine->setSpacing(7);
    auto *documentIcon = new QLabel(card);
    documentIcon->setFixedSize(20, 20);
    documentIcon->setPixmap(iconFromSvg(QStringLiteral(":/login/summary-document.svg"), 20).pixmap(20, 20));
    titleLine->addWidget(documentIcon, 0, Qt::AlignTop);
    auto *title = new QLabel(QStringLiteral("%1 · 讨论总结")
        .arg(message.value(QStringLiteral("room_name")).toString()), card);
    title->setObjectName(QStringLiteral("discussionSummaryTitle"));
    title->setTextFormat(Qt::PlainText);
    title->setWordWrap(true);
    title->setStyleSheet(QStringLiteral("color:#282043; font-size:16px; font-weight:700;"));
    titleLine->addWidget(title, 1);
    auto *kind = new QLabel(QStringLiteral("讨论总结"), card);
    kind->setObjectName(QStringLiteral("discussionSummaryKind"));
    kind->setAlignment(Qt::AlignCenter);
    kind->setStyleSheet(QStringLiteral("color:#7351c7; background:#eee7fc; border-radius:8px;"
        " padding:3px 6px; font-size:11px;"));
    titleLine->addWidget(kind, 0, Qt::AlignTop);
    box->addLayout(titleLine);
    auto *range = new QLabel(QStringLiteral("基于最近 %1 条消息生成 · 序号 %2–%3")
        .arg(message.value(QStringLiteral("message_count")).toInt())
        .arg(message.value(QStringLiteral("first_sequence")).toString(),
             message.value(QStringLiteral("last_sequence")).toString()), card);
    range->setObjectName(QStringLiteral("discussionSummaryRange"));
    range->setTextFormat(Qt::PlainText);
    range->setWordWrap(true);
    range->setStyleSheet(QStringLiteral("color:#8b7ea5; font-size:12px;"));
    box->addWidget(range);
    const QVector<QString> sections = splitSummarySections(markdown);
    const QStringList names{QStringLiteral("讨论概览"), QStringLiteral("已达成共识"),
                            QStringLiteral("待解决问题"), QStringLiteral("下一步行动")};
    for (int i = 0; i < names.size(); ++i) {
        box->addSpacing(2);
        auto *divider = new QFrame(card);
        divider->setFixedHeight(1);
        divider->setStyleSheet(QStringLiteral("background:#ebe3f7; border:0;"));
        box->addWidget(divider);
        auto *section = new QWidget(card);
        section->setObjectName(QStringLiteral("discussionSummaryPreview%1").arg(i));
        auto *sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(0, 0, 0, 0);
        sectionLayout->setSpacing(8);
        auto *heading = new QLabel(names.at(i), section);
        heading->setObjectName(QStringLiteral("discussionSummarySectionTitle%1").arg(i));
        heading->setStyleSheet(QStringLiteral("color:#30234e; font-size:15px; font-weight:600;"));
        sectionLayout->addWidget(heading);
        const QStringList items = i == 0 ? QStringList() : summaryPreviewItems(sections.value(i));
        if (items.isEmpty()) {
            auto *body = new SummaryBodyLabel(summaryPreview(sections.value(i),
                i == 0 ? 132 : 100), section);
            body->setObjectName(QStringLiteral("discussionSummarySectionBody%1").arg(i));
            sectionLayout->addWidget(body);
        } else {
            const int visibleCount = qMin(items.size(), i == 1 ? 3 : 2);
            for (int j = 0; j < visibleCount; ++j) {
                auto *entry = new QWidget(section);
                entry->setObjectName(QStringLiteral("discussionSummaryEntry%1").arg(i));
                auto *line = new QHBoxLayout(entry);
                line->setContentsMargins(0, 0, 0, 0);
                line->setSpacing(8);
                auto *mark = new QLabel(i == 1 ? QStringLiteral("✓") : QStringLiteral("•"), entry);
                mark->setAlignment(Qt::AlignCenter);
                mark->setFixedSize(18, 18);
                mark->setStyleSheet(QStringLiteral("color:#7954d4; background:#efe8ff;"
                    " border-radius:9px; font-size:12px; font-weight:700;"));
                line->addWidget(mark, 0, Qt::AlignTop);
                auto *body = new SummaryBodyLabel(summaryPreview(items.at(j), 104), entry);
                body->setObjectName(QStringLiteral("discussionSummaryEntryText%1").arg(i));
                line->addWidget(body, 1);
                sectionLayout->addWidget(entry);
            }
            if (items.size() > visibleCount) {
                auto *remaining = new QLabel(QStringLiteral("还有 %1 项，查看完整总结")
                    .arg(items.size() - visibleCount), section);
                remaining->setObjectName(QStringLiteral("discussionSummaryRemaining%1").arg(i));
                remaining->setStyleSheet(QStringLiteral("color:#806d9e; font-size:12px;"));
                sectionLayout->addWidget(remaining);
            }
        }
        box->addWidget(section);
    }
    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(7);
    auto *view = new QPushButton(QStringLiteral("查看完整总结"), card);
    view->setObjectName(QStringLiteral("discussionSummaryView"));
    auto *copy = new QPushButton(QStringLiteral("复制"), card);
    copy->setObjectName(QStringLiteral("discussionSummaryCopy"));
    buttons->addWidget(view);
    buttons->addWidget(copy);
    buttons->addStretch();
    box->addLayout(buttons);
    connect(view, &QPushButton::clicked, this, [this, message] { openSharedSummary(message); });
    connect(copy, &QPushButton::clicked, this, [markdown] {
        QGuiApplication::clipboard()->setText(markdown);
    });
    columnLayout->addWidget(card);
    rowLayout->addWidget(column);
    rowLayout->addStretch();
    summaryCards.append(card);
    ui->messageLayout->insertWidget(ui->messageLayout->count() - 1, row);
    updateBubbleWidths();
    QTimer::singleShot(0, this, [this, scroll, followBottom] {
        updateBubbleWidths();
        if (followBottom) scroll->setValue(scroll->maximum());
    });
}

void MainWindow::connectSlots()
{
    // .ui 中的 on_xxx_signal 槽函数由 setupUi 自动连接，这里只连接网络 socket。
    // 连接成功后，客户端会立刻发送 JOIN|昵称，让服务器知道这个用户正式进入聊天界面。
    connect(socket, &QTcpSocket::connected, this, [this]() {
        isConnected = true;
        viviSummarySupported = false;
        roomRestorePending = true;
        roomListRequested = roomListLoaded = switchInFlight = roomHistoryLoading = initialHistoryDone = false;
        pendingRoomId.clear();
        queuedRoomId.clear();
        currentRoomId = QStringLiteral("class-room");
        roomMembers.clear();
        onlineRoomUserIds.clear();
        onlineNames.clear();
        clearMessageView();
        roomSidebar->setRooms({});
        roomSidebar->setCurrentRoom(currentRoomId);
        roomSidebar->setNotice(QStringLiteral("正在加载聊天室…"));
        pendingRoomInvites = {};
        updateInviteBadge();
        updateRoomTitle();
        ui->serverStatusLabel->setText(QStringLiteral("● 服务器已连接"));
        ui->serverStatusLabel->setStyleSheet("color:#31b875; font-size:12px;");
        appendSystemMessage(QStringLiteral("欢迎来到 VChat 聊天室"));
        appendSystemMessage("已连接到聊天服务器");
        statusBar()->showMessage("已连接到聊天服务器");
        sendJoinMessage();
    });

    // 断开连接后，界面状态恢复为“未连接”，在线列表也只显示自己。
    connect(socket, &QTcpSocket::disconnected, this, [this]() {
        isConnected = false;
        viviSummarySupported = false;
        if (viviSummarySharePending) {
            viviSummarySharePending = false;
            viviSummaryShare->setText(QStringLiteral("分享至聊天室"));
            viviSummaryShare->setEnabled(false);
            viviSummaryRegenerate->setEnabled(!viviSummaryReadOnly);
            if (!viviSummaryReadOnly)
                viviSummaryStatus->setText(QStringLiteral("连接已断开；重连后可重试同一分享请求。"));
        }
        if (viviSummaryPending) {
            viviSummaryRenderTimer->stop();
            renderViviSummary(false);
            viviSummaryPending = false;
            viviSummaryRequestId.clear();
            viviSummaryStatus->setText(QStringLiteral("连接已断开，总结已中止。"));
            viviSummaryRegenerate->setEnabled(true);
        }
        createRoomInFlight = false;
        roomCreateTimer->stop();
        inviteSendTimer->stop();
        inviteDecisionTimer->stop();
        inviteSendInFlight = inviteDecisionInFlight = false;
        inviteSearchRequestId.clear();
        inviteSendRequestId.clear();
        inviteDecisionRequestId.clear();
        managementTimer->stop();
        managementRequestId.clear();
        managementAction.clear();
        managementRoomId.clear();
        permissionsTimer->stop();
        permissionsReadId.clear();
        permissionsSaveId.clear();
        if (roomPermissionsDialog) {
            roomPermissionsDialog->setPending(false);
            roomPermissionsDialog->showError(QStringLiteral("连接已断开，权限保存结果尚未确认。"));
        }
        if (roomManagementDialog) {
            roomManagementDialog->setPending(false);
            roomManagementDialog->showError(QStringLiteral("连接已断开，请重新连接后核对操作结果。"));
        }
        pendingRoomInvites = {};
        updateInviteBadge();
        if (roomInviteDialog) {
            roomInviteDialog->setPending(false);
            roomInviteDialog->showError(QStringLiteral("服务器连接已断开，邀请结果请重新连接后核对。"));
        }
        if (roomInviteInboxDialog) {
            roomInviteInboxDialog->setPending(false);
            roomInviteInboxDialog->setInvites({});
            roomInviteInboxDialog->showError(QStringLiteral("服务器连接已断开，请重新连接后刷新邀请。"));
        }
        if (roomCreateDialog) {
            roomCreateDialog->setPending(false);
            roomCreateDialog->showError(QStringLiteral("服务器连接已断开，请重新连接后再创建。"));
        }
        roomListLoaded = switchInFlight = roomHistoryLoading = false;
        pendingRoomId.clear();
        queuedRoomId.clear();
        updateRoomTitle();
        roomSidebar->setNotice(QStringLiteral("连接已断开"));
        onlineRoomUserIds.clear();
        mentionProtocol = false;
        onlineMentionMembers.clear();
        streamedRequestIds.clear();
        aiReplyTargets.clear();
        for (auto it = aiStreams.begin(); it != aiStreams.end(); ++it) {
            if (it->retryAwaiting) {
                it->retryAwaiting = false;
                for (QToolButton *button : it->actions) button->setEnabled(true);
                if (it->actions.size() >= 2) it->actions.at(1)->setEnabled(it->canRegenerate);
            }
            if (it->finished) continue;
            it->interrupted = true;
            it->errorText = QStringLiteral("连接已断开，Vivi 生成中断。");
            it->errorReceived = true;
            it->raw = it->displayed;
            finalizeAiStream(it.key());
        }
        if (mentionPopup) mentionPopup->hide();
        setAiStatus(QStringLiteral("unavailable"));
        for (const QString &id : aiThinkingRows.keys()) removeAiThinking(id);
        ui->serverStatusLabel->setText(QStringLiteral("● 服务器未连接"));
        ui->serverStatusLabel->setStyleSheet("color:#9b6b7d; font-size:12px;");
        updateUserList();
        appendSystemMessage("已断开与聊天服务器的连接");
        statusBar()->showMessage("已断开连接");
    });

    // readyRead 表示服务器有新数据发过来了，读取工作交给 on_socketReadyRead()。
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::on_socketReadyRead);

    // 连接失败、服务器关闭等网络错误都会进入这里。
    connect(socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        isConnected = false;
        setAiStatus(QStringLiteral("unavailable"));
        ui->serverStatusLabel->setText(QStringLiteral("● 服务器未连接"));
        ui->serverStatusLabel->setStyleSheet("color:#9b6b7d; font-size:12px;");
        appendSystemMessage("连接服务器失败: " + socket->errorString());
        statusBar()->showMessage("连接服务器失败");
    });
}

void MainWindow::on_sendButton_clicked()
{
    const QString raw = ui->messageInput->toPlainText();
    const QString message = raw.trimmed();
    if (message.isEmpty()) {
        return;
    }

    if (!isConnected || socket->state() != QAbstractSocket::ConnectedState) {
        appendSystemMessage("请先连接服务器");
        return;
    }
    if (switchInFlight) {
        roomSidebar->setNotice(QStringLiteral("聊天室正在加载，请稍候"));
        return;
    }
    if (!mentionProtocol && message.contains(QLatin1Char('\n'))) {
        appendSystemMessage(QStringLiteral("正在确认服务器的多行消息能力，请稍后重试"));
        return;
    }

    // 本项目使用最简单的文本协议：每条消息以换行符 \n 结束。
    // 服务器收到 “昵称: 内容” 后，会转发给其他在线客户端，并保存到数据库。
    QByteArray networkMessage;
    QString outgoingMessageId;
    QJsonArray mentions;
    if (mentionProtocol) {
        int leading = 0;
        while (leading < raw.size() && raw.at(leading).isSpace()) ++leading;
        for (const SelectedMention &selected : selectedMentions) {
            const int start = selected.start - leading;
            const QString expected = QStringLiteral("@") + selected.candidate.name;
            if (start < 0 || start + selected.length > message.size() ||
                message.mid(start, selected.length) != expected) continue;
            mentions.append(QJsonObject{{QStringLiteral("type"), selected.candidate.ai ? QStringLiteral("ai") : QStringLiteral("user")},
                {QStringLiteral("id"), selected.candidate.ai ? QStringLiteral("vivi") : QString::number(selected.candidate.id)},
                {QStringLiteral("name"), selected.candidate.name},
                {QStringLiteral("start"), start}, {QStringLiteral("length"), selected.length}});
        }
        outgoingMessageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QJsonObject payload{{QStringLiteral("text"), message},
            {QStringLiteral("messageId"), outgoingMessageId},
            {QStringLiteral("mentions"), mentions}};
        networkMessage = "CHAT2|" + QJsonDocument(payload).toJson(QJsonDocument::Compact).toBase64() + '\n';
    } else {
        networkMessage = (nickname + ": " + message + "\n").toUtf8();
    }
    if (socket->write(networkMessage) == -1) {
        appendSystemMessage("消息发送失败: " + socket->errorString());
        return;
    }

    appendMessage(nickname, message, true, {}, false, {}, mentions, {}, {}, outgoingMessageId);
    selectedMentions.clear();
    if (mentionPopup) mentionPopup->hide();
    ui->messageInput->clear();
    ui->messageInput->setFocus();
}

void MainWindow::on_messageInput_returnPressed()
{
    on_sendButton_clicked();
}

void MainWindow::on_socketReadyRead()
{
    // TCP 是流式传输，不能保证一次 readyRead 正好是一条完整消息。
    // 所以先追加到 receiveBuffer，再按 \n 一行一行拆出来处理。
    receiveBuffer.append(socket->readAll());

    int lineEnd = -1;
    while ((lineEnd = receiveBuffer.indexOf('\n')) != -1) {
        const QByteArray lineBytes = receiveBuffer.left(lineEnd).trimmed();
        receiveBuffer.remove(0, lineEnd + 1);

        if (!lineBytes.isEmpty()) {
            appendIncomingLine(QString::fromUtf8(lineBytes));
        }
    }
}

void MainWindow::on_actionConnect_triggered()
{
    // 菜单栏“连接服务器”会弹出输入框，手动填写服务器 IP 和端口。
    bool ok = false;
    const QString serverAddress = QInputDialog::getText(
        this,
        "连接服务器",
        "请输入服务器地址:",
        QLineEdit::Normal,
        "127.0.0.1:8080",
        &ok);

    if (ok && !serverAddress.trimmed().isEmpty()) {
        connectToServerAddress(serverAddress, true);
    }
}

void MainWindow::on_actionDisconnect_triggered()
{
    if (socket->state() == QAbstractSocket::UnconnectedState) {
        appendSystemMessage("当前没有连接服务器");
        return;
    }

    appendSystemMessage("正在断开服务器连接...");
    socket->disconnectFromHost();
}

void MainWindow::on_actionExit_triggered()
{
    close();
}

void MainWindow::on_actionChangeNickname_triggered()
{
    bool ok = false;
    const QString newNickname = QInputDialog::getText(
        this,
        "修改昵称",
        "请输入新昵称:",
        QLineEdit::Normal,
        nickname,
        &ok);

    if (!ok || newNickname.trimmed().isEmpty()) {
        return;
    }

    const QString oldNickname = nickname;
    nickname = newNickname.trimmed();
    setWindowTitle("VChat - " + nickname);
    appendSystemMessage(oldNickname + " 修改昵称为 " + nickname);

    // 如果已经连接服务器，改名后重新发送 JOIN，让服务器刷新在线列表里的名字。
    if (socket->state() == QAbstractSocket::ConnectedState) {
        sendJoinMessage();
    }
}

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(
        this,
        "关于 VChat",
        "VChat 学生群体聊天系统\n\n"
        "基于 Qt6 开发，支持登录验证、局域网聊天、服务器转发、在线用户列表和聊天记录保存。"
    );
}

bool MainWindow::connectToServerAddress(const QString &address, bool showErrorMessage)
{
    const QString serverAddress = address.trimmed();

    // 服务器地址约定为 “IP:端口”，例如 192.168.1.10:8080。
    // lastIndexOf(':') 可以兼容 IP 中没有冒号的普通 IPv4 地址。
    const int separatorIndex = serverAddress.lastIndexOf(':');
    if (separatorIndex <= 0 || separatorIndex == serverAddress.size() - 1) {
        if (showErrorMessage) {
            QMessageBox::warning(this, "连接失败", "服务器地址格式应为 IP:端口，例如 127.0.0.1:8080");
        } else {
            appendSystemMessage("服务器地址格式不正确: " + serverAddress);
        }
        return false;
    }

    const QString host = serverAddress.left(separatorIndex).trimmed();
    bool portOk = false;
    const quint16 port = serverAddress.mid(separatorIndex + 1).trimmed().toUShort(&portOk);
    if (!portOk || port == 0) {
        if (showErrorMessage) {
            QMessageBox::warning(this, "连接失败", "端口号不正确，例如 8080");
        } else {
            appendSystemMessage("服务器端口不正确: " + serverAddress);
        }
        return false;
    }

    // 如果之前连过其他服务器，先 abort 关闭旧连接，再连接新地址。
    if (socket->state() != QAbstractSocket::UnconnectedState) {
        socket->abort();
    }

    statusBar()->showMessage("正在连接服务器...");
    socket->connectToHost(host, port);
    return true;
}

void MainWindow::sendJoinMessage()
{
    if (socket->state() == QAbstractSocket::ConnectedState) {
        if (sessionToken.isEmpty()) {
            roomSidebar->setNotice(QStringLiteral("登录会话缺失，请重新登录"));
            appendSystemMessage(QStringLiteral("登录会话缺失，请重新登录后进入聊天室。"));
            return;
        }
        socket->write(QStringLiteral("JOIN|%1|%2\n").arg(nickname, sessionToken).toUtf8());
    }
}

void MainWindow::sendRoomRequest(const QString &action, const QJsonObject &arguments)
{
    if (socket->state() != QAbstractSocket::ConnectedState || sessionToken.isEmpty()) return;
    socket->write("ROOM|" + action.toUtf8() + '|' +
        QJsonDocument(arguments).toJson(QJsonDocument::Compact).toBase64() + '\n');
}

void MainWindow::requestRoomSwitch(const QString &roomId)
{
    if (!roomListLoaded || roomId.isEmpty()) return;
    bool allowed = false;
    for (const QJsonValue &value : joinedRooms)
        if (value.toObject().value(QStringLiteral("room_id")).toString() == roomId) allowed = true;
    if (!allowed) return;
    if (switchInFlight) {
        queuedRoomId = roomId;
        return;
    }
    if (roomId == currentRoomId) return;
    closeViviSummary();
    setViviMenuExpanded(false, false);
    switchInFlight = true;
    pendingRoomId = roomId;
    queuedRoomId.clear();
    updateRoomTitle();
    ui->messageInput->setEnabled(false);
    ui->sendButton->setEnabled(false);
    roomSidebar->setNotice(QStringLiteral("正在切换聊天室…"));
    sendRoomRequest(QStringLiteral("switch"), {{QStringLiteral("room_id"), roomId}});
}

void MainWindow::openCreateRoomDialog()
{
    if (roomCreateDialog) { roomCreateDialog->raise(); roomCreateDialog->activateWindow(); return; }
    if (!roomListLoaded || !isConnected || switchInFlight) {
        roomSidebar->setNotice(QStringLiteral("请等待聊天室加载完成并确认服务器已连接"));
        return;
    }
    auto *dialog = new RoomCreateDialog(this);
    roomCreateDialog = dialog;
    connect(dialog, &RoomCreateDialog::createSubmitted, this,
        [this, dialog](const QString &name, const QString &description, const QString &type) {
            if (createRoomInFlight) {
                dialog->setPending(false);
                dialog->showError(QStringLiteral("上一次创建请求结果尚未确认，不能重复提交。"));
                return;
            }
            if (socket->state() != QAbstractSocket::ConnectedState) {
                dialog->setPending(false);
                dialog->showError(QStringLiteral("服务器未连接，请稍后重试。"));
                return;
            }
            createRoomInFlight = true;
            sendRoomRequest(QStringLiteral("create"),
                {{QStringLiteral("room_name"), name},
                 {QStringLiteral("description"), description},
                 {QStringLiteral("room_type"), type}});
            roomCreateTimer->start(15000);
        });
    dialog->open();
}

void MainWindow::openInviteDialog()
{
    if (roomInviteDialog) { roomInviteDialog->raise(); roomInviteDialog->activateWindow(); return; }
    if (!canInviteToCurrentRoom() || switchInFlight || roomHistoryLoading) return;
    QJsonObject selectedRoom;
    for (const QJsonValue &value : joinedRooms) {
        if (value.toObject().value(QStringLiteral("room_id")).toString() == currentRoomId) {
            selectedRoom = value.toObject(); break;
        }
    }
    auto *dialog = new RoomInviteDialog(currentRoomId,
        selectedRoom.value(QStringLiteral("room_name")).toString(), this);
    roomInviteDialog = dialog;
    connect(dialog, &RoomInviteDialog::searchRequested, this,
        [this](const QString &roomId, const QString &query) {
            if (!isConnected) return;
            inviteSearchRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            sendRoomRequest(QStringLiteral("search_users"), {{QStringLiteral("request_id"), inviteSearchRequestId},
                {QStringLiteral("room_id"), roomId}, {QStringLiteral("query"), query}});
        });
    connect(dialog, &RoomInviteDialog::sendRequested, this,
        [this, dialog](const QString &roomId, const QJsonArray &ids) {
            if (inviteSendInFlight || !isConnected) {
                dialog->setPending(inviteSendInFlight);
                dialog->showError(QStringLiteral("上一次请求尚未确认，或服务器已断开。"));
                return;
            }
            inviteSendInFlight = true;
            inviteSendRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            sendRoomRequest(QStringLiteral("send_user_invites"),
                {{QStringLiteral("request_id"), inviteSendRequestId},
                 {QStringLiteral("room_id"), roomId}, {QStringLiteral("user_ids"), ids}});
            inviteSendTimer->start(15000);
        });
    dialog->open();
    dialog->requestSearch();
}

void MainWindow::openInviteInbox()
{
    if (roomInviteInboxDialog) { roomInviteInboxDialog->raise(); roomInviteInboxDialog->activateWindow(); return; }
    auto *dialog = new RoomInviteInboxDialog(this);
    roomInviteInboxDialog = dialog;
    dialog->setInvites(pendingRoomInvites);
    dialog->setPending(inviteDecisionInFlight);
    connect(dialog, &RoomInviteInboxDialog::decisionRequested, this,
        [this, dialog](const QString &inviteId, bool accept) {
            if (inviteDecisionInFlight || !isConnected) {
                dialog->showError(QStringLiteral("上一次处理尚未确认，或服务器已断开。"));
                return;
            }
            inviteDecisionInFlight = true;
            dialog->setPending(true);
            dialog->showError({});
            inviteDecisionRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            sendRoomRequest(QStringLiteral("respond_invite"),
                {{QStringLiteral("request_id"), inviteDecisionRequestId},
                 {QStringLiteral("invite_id"), inviteId},
                 {QStringLiteral("decision"), accept ? QStringLiteral("accept") : QStringLiteral("decline")}});
            inviteDecisionTimer->start(15000);
        });
    dialog->open();
    refreshPendingInvites();
}

void MainWindow::openRoomManagement(const QString &page)
{
    if (!isConnected || switchInFlight || roomHistoryLoading ||
        currentRoomId == QStringLiteral("class-room")) return;
    if (roomManagementDialog && roomManagementDialog->roomId() != currentRoomId)
        roomManagementDialog->close();
    if (!roomManagementDialog) {
        auto *dialog = new RoomManagementDialog(currentRoomId, ownAccountId, this);
        roomManagementDialog = dialog;
        connect(dialog, &RoomManagementDialog::saveRequested, this,
            [this](const QString &roomId, const QString &name, const QString &description) {
                submitRoomManagement(QStringLiteral("update_details"), roomId,
                    {{QStringLiteral("room_name"), name}, {QStringLiteral("description"), description}});
            });
        connect(dialog, &RoomManagementDialog::removeRequested, this,
            [this](const QString &roomId, const QString &userId) {
                submitRoomManagement(QStringLiteral("remove_member"), roomId,
                    {{QStringLiteral("user_id"), userId}});
            });
        connect(dialog, &RoomManagementDialog::leaveRequested, this,
            [this](const QString &roomId) { submitRoomManagement(QStringLiteral("leave"), roomId); });
        connect(dialog, &RoomManagementDialog::dissolveRequested, this,
            [this](const QString &roomId) { submitRoomManagement(QStringLiteral("dissolve"), roomId); });
        connect(dialog, &RoomManagementDialog::permissionsRequested, this,
            &MainWindow::openRoomPermissions);
        dialog->open();
    } else {
        roomManagementDialog->raise();
        roomManagementDialog->activateWindow();
    }
    roomManagementDialog->showPage(page);
    sendRoomRequest(QStringLiteral("details"), {{QStringLiteral("room_id"), currentRoomId},
        {QStringLiteral("request_id"), QUuid::createUuid().toString(QUuid::WithoutBraces)}});
    requestRoomMembers();
}

void MainWindow::openRoomPermissions(const QString &roomId)
{
    if (!isConnected || roomId.isEmpty() || roomId == QStringLiteral("class-room")) return;
    if (roomPermissionsDialog && roomPermissionsDialog->roomId() != roomId)
        roomPermissionsDialog->close();
    if (roomPermissionsDialog) {
        roomPermissionsDialog->raise();
        return;
    }
    bool owner = false;
    for (const QJsonValue &value : joinedRooms) {
        const QJsonObject room = value.toObject();
        if (room.value(QStringLiteral("room_id")).toString() == roomId) {
            owner = room.value(QStringLiteral("owner_id")).toString().toLongLong() == ownAccountId;
            break;
        }
    }
    QWidget *dialogParent = roomManagementDialog && roomManagementDialog->roomId() == roomId
        ? static_cast<QWidget *>(roomManagementDialog.data()) : static_cast<QWidget *>(this);
    auto *dialog = new RoomPermissionsDialog(roomId, owner, dialogParent);
    roomPermissionsDialog = dialog;
    connect(dialog, &RoomPermissionsDialog::saveRequested, this,
        [this](const QString &id, const QJsonObject &settings) {
            if (!isConnected || !permissionsSaveId.isEmpty() || !roomPermissionsDialog ||
                roomPermissionsDialog->roomId() != id) {
                if (roomPermissionsDialog) {
                    roomPermissionsDialog->setPending(false);
                    roomPermissionsDialog->showError(QStringLiteral("当前无法保存权限配置，请检查连接。"));
                }
                return;
            }
            permissionsSaveId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            sendRoomRequest(QStringLiteral("update_permissions"),
                {{QStringLiteral("room_id"), id}, {QStringLiteral("permissions"), settings},
                 {QStringLiteral("request_id"), permissionsSaveId}});
            permissionsTimer->start(15000);
        });
    dialog->open();
    permissionsReadId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString readId = permissionsReadId;
    sendRoomRequest(QStringLiteral("permissions"), {{QStringLiteral("room_id"), roomId},
        {QStringLiteral("request_id"), permissionsReadId}});
    QTimer::singleShot(15000, this, [this, readId, roomId]() {
        if (permissionsReadId != readId) return;
        permissionsReadId.clear();
        if (roomPermissionsDialog && roomPermissionsDialog->roomId() == roomId)
            roomPermissionsDialog->showError(QStringLiteral("读取权限配置超时，请关闭后重试。"));
    });
}

void MainWindow::submitRoomManagement(const QString &action, const QString &roomId,
                                      const QJsonObject &arguments)
{
    if (!managementRequestId.isEmpty() || !isConnected || roomId.isEmpty()) {
        if (roomManagementDialog) {
            roomManagementDialog->setPending(!managementRequestId.isEmpty());
            roomManagementDialog->showError(QStringLiteral("服务器未连接，或上一次操作尚未确认。"));
        }
        return;
    }
    managementRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    managementAction = action;
    managementRoomId = roomId;
    QJsonObject request = arguments;
    request.insert(QStringLiteral("room_id"), roomId);
    request.insert(QStringLiteral("request_id"), managementRequestId);
    if (roomManagementDialog) roomManagementDialog->setPending(true);
    sendRoomRequest(action, request);
    managementTimer->start(15000);
}

void MainWindow::applyRoomUpdate(const QJsonObject &room)
{
    const QString roomId = room.value(QStringLiteral("room_id")).toString();
    if (roomId.isEmpty()) return;
    for (int i = 0; i < joinedRooms.size(); ++i) {
        if (joinedRooms.at(i).toObject().value(QStringLiteral("room_id")).toString() != roomId) continue;
        QJsonObject updated = joinedRooms.at(i).toObject();
        for (auto it = room.begin(); it != room.end(); ++it) updated.insert(it.key(), it.value());
        joinedRooms.replace(i, updated);
        roomSidebar->setRooms(joinedRooms);
        roomSidebar->setCurrentRoom(currentRoomId);
        if (roomManagementDialog && roomManagementDialog->roomId() == roomId)
            roomManagementDialog->setRoom(updated);
        if (roomId == currentRoomId) updateRoomTitle();
        return;
    }
}

void MainWindow::revokeRoomAccess(const QString &roomId)
{
    if (roomId.isEmpty()) return;
    if (currentRoomId == roomId && switchInFlight &&
        pendingRoomId == QStringLiteral("class-room")) return;
    for (int i = joinedRooms.size() - 1; i >= 0; --i)
        if (joinedRooms.at(i).toObject().value(QStringLiteral("room_id")).toString() == roomId)
            joinedRooms.removeAt(i);
    roomSidebar->setRooms(joinedRooms);
    if (roomManagementDialog && roomManagementDialog->roomId() == roomId)
        roomManagementDialog->close();
    if (currentRoomId == roomId || pendingRoomId == roomId) {
        ui->messageInput->setEnabled(false);
        ui->sendButton->setEnabled(false);
        requestRoomSwitch(QStringLiteral("class-room"));
    } else roomSidebar->setCurrentRoom(currentRoomId);
    sendRoomRequest(QStringLiteral("list_mine"));
}

void MainWindow::refreshPendingInvites()
{
    if (isConnected && initialHistoryDone) sendRoomRequest(QStringLiteral("pending_invites"));
}

void MainWindow::updateInviteBadge()
{
    inviteInboxButton->setText(QStringLiteral("邀请"));
    inviteInboxButton->setIcon(inviteBellIcon(!pendingRoomInvites.isEmpty()));
    inviteInboxButton->setProperty("pendingCount", pendingRoomInvites.size());
    inviteInboxButton->setProperty("hasPendingInvite", !pendingRoomInvites.isEmpty());
}

bool MainWindow::canInviteToCurrentRoom() const
{
    if (!isConnected || !roomListLoaded || ownAccountId <= 0 ||
        currentRoomId == QStringLiteral("class-room")) return false;
    for (const QJsonValue &value : joinedRooms) {
        const QJsonObject room = value.toObject();
        if (room.value(QStringLiteral("room_id")).toString() != currentRoomId) continue;
        bool valid = false;
        const QJsonValue rawOwner = room.value(QStringLiteral("owner_id"));
        const qint64 ownerId = rawOwner.isString()
            ? rawOwner.toString().toLongLong(&valid)
            : rawOwner.toVariant().toLongLong(&valid);
        if (valid && ownerId > 0)
            return ownerId == ownAccountId || room.value(QStringLiteral("allow_member_invite")).toBool();
        break; // Older ROOM servers may omit owner_id; use their members role.
    }
    for (const RoomMember &member : roomMembers)
        if (member.id == ownAccountId && member.role == QStringLiteral("owner")) return true;
    return false;
}

void MainWindow::handleRoomNotice(const QJsonObject &notice)
{
    const QString event = notice.value(QStringLiteral("event")).toString();
    if (event == QStringLiteral("invite_received")) {
        QApplication::alert(this);
        refreshPendingInvites();
    } else if (event == QStringLiteral("invites_updated")) {
        refreshPendingInvites();
    } else if (event == QStringLiteral("members_changed") &&
               notice.value(QStringLiteral("room_id")).toString() == currentRoomId) {
        requestRoomMembers();
    } else if (event == QStringLiteral("room_updated")) {
        applyRoomUpdate(notice.value(QStringLiteral("room")).toObject());
    } else if (event == QStringLiteral("permissions_updated")) {
        const QString changedRoom = notice.value(QStringLiteral("room_id")).toString();
        applyRoomUpdate(notice.value(QStringLiteral("room")).toObject());
        if (roomPermissionsDialog && roomPermissionsDialog->roomId() == changedRoom &&
            permissionsSaveId.isEmpty())
            roomPermissionsDialog->setPermissions(notice.value(QStringLiteral("permissions")).toObject());
        sendRoomRequest(QStringLiteral("list_mine"));
    } else if (event == QStringLiteral("access_revoked") ||
               event == QStringLiteral("room_dissolved")) {
        revokeRoomAccess(notice.value(QStringLiteral("room_id")).toString());
    }
}

void MainWindow::setMembersCollapsed(bool collapsed)
{
    membersCollapsed = collapsed;
    QScrollBar *scroll = ui->messageScroll->verticalScrollBar();
    const int oldPosition = scroll->value();
    const bool wasAtBottom = oldPosition >= scroll->maximum() - 40;
    ui->rightPanel->setVisible(!collapsed);
    memberExpandButton->setVisible(collapsed);
    ui->bodyLayout->setStretch(1, 3);
    ui->bodyLayout->setStretch(2, 0);
    ui->bodyLayout->setStretch(3, collapsed ? 0 : 1);
    QSettings settings(QStringLiteral("VChat"), QStringLiteral("VChatClient"));
    settings.setValue(QStringLiteral("members/collapsed/%1").arg(ownAccountId), collapsed);
    if (!collapsed) requestRoomMembers();
    QTimer::singleShot(0, this, [this, oldPosition, wasAtBottom]() {
        updateBubbleWidths();
        if (!membersCollapsed) updateMemberListHeight();
        QScrollBar *bar = ui->messageScroll->verticalScrollBar();
        bar->setValue(wasAtBottom ? bar->maximum() : qMin(oldPosition, bar->maximum()));
    });
}

void MainWindow::clearMessageView()
{
    aiTypewriterTimer->stop();
    aiStreams.clear();
    aiThinkingRows.clear();
    aiReplyTargets.clear();
    streamedRequestIds.clear();
    renderedUserMessageIds.clear();
    renderedSummaryMessageIds.clear();
    summaryCards.clear();
    bubbleColumns.clear();
    messageBubbles.clear();
    messageBodies.clear();
    aiColumns.clear();
    aiBubbles.clear();
    aiBodies.clear();
    systemPills.clear();
    lastMessageKey.clear();
    lastSystemDate = QDate();
    // Keep the trailing stretch item; all message widgets belong to this layout.
    while (ui->messageLayout->count() > 1) {
        QLayoutItem *item = ui->messageLayout->takeAt(0);
        delete item->widget();
        delete item;
    }
}

void MainWindow::updateRoomTitle()
{
    QString name = QStringLiteral("VChat 大厅");
    QJsonObject active;
    for (const QJsonValue &value : joinedRooms) {
        const QJsonObject room = value.toObject();
        if (room.value(QStringLiteral("room_id")).toString() == currentRoomId) {
            name = room.value(QStringLiteral("room_name")).toString(name);
            active = room;
            break;
        }
    }
    if (roomNameLabel) {
        roomNameLabel->setText(name);
        roomDescriptionLabel->setText(active.value(QStringLiteral("description")).toString());
        roomDescriptionLabel->setVisible(!roomDescriptionLabel->text().isEmpty());
        const bool canInvite = canInviteToCurrentRoom();
        inviteMembersButton->setVisible(canInvite);
        inviteMembersButton->setEnabled(canInvite && !switchInFlight && !roomHistoryLoading);
        inviteMembersButton->setToolTip(QStringLiteral("邀请成员加入当前聊天室"));
        roomMoreButton->setVisible(currentRoomId != QStringLiteral("class-room") && roomListLoaded);
        roomMoreButton->setEnabled(isConnected && !switchInFlight && !roomHistoryLoading);
    }
    ui->subtitleLabel->setText(QStringLiteral("轻松连接每一次交流"));
    setWindowTitle(QStringLiteral("VChat - %1").arg(name));
}

void MainWindow::requestRoomMembers()
{
    if (roomListLoaded && !currentRoomId.isEmpty())
        sendRoomRequest(QStringLiteral("members"), {{QStringLiteral("room_id"), currentRoomId}});
}

void MainWindow::handleRoomResult(const QJsonObject &result)
{
    const QString action = result.value(QStringLiteral("action")).toString();
    const bool ok = result.value(QStringLiteral("ok")).toBool();
    const QString requestId = result.value(QStringLiteral("request_id")).toString();
    if (action == QStringLiteral("permissions")) {
        if (requestId == permissionsReadId && roomPermissionsDialog &&
            roomPermissionsDialog->roomId() == result.value(QStringLiteral("room_id")).toString()) {
            permissionsReadId.clear();
            if (ok) roomPermissionsDialog->setPermissions(result.value(QStringLiteral("permissions")).toObject());
            else roomPermissionsDialog->showError(result.value(QStringLiteral("error"))
                .toString(QStringLiteral("权限配置读取失败")));
        }
        return;
    }
    if (action == QStringLiteral("update_permissions")) {
        if (requestId != permissionsSaveId) return;
        permissionsTimer->stop();
        permissionsSaveId.clear();
        if (roomPermissionsDialog &&
            roomPermissionsDialog->roomId() == result.value(QStringLiteral("room_id")).toString()) {
            roomPermissionsDialog->setPending(false);
            if (ok) roomPermissionsDialog->accept();
            else roomPermissionsDialog->showError(result.value(QStringLiteral("error"))
                .toString(QStringLiteral("保存权限配置失败")));
        }
        if (ok) {
            sendRoomRequest(QStringLiteral("list_mine"));
            if (roomManagementDialog)
                sendRoomRequest(QStringLiteral("details"),
                    {{QStringLiteral("room_id"), result.value(QStringLiteral("room_id"))}});
        }
        return;
    }
    if (action == QStringLiteral("details")) {
        const QJsonObject room = result.value(QStringLiteral("room")).toObject();
        if (roomManagementDialog && roomManagementDialog->roomId() ==
            (ok ? room.value(QStringLiteral("room_id")).toString()
                : result.value(QStringLiteral("room_id")).toString())) {
            if (ok) roomManagementDialog->setRoom(room);
            else roomManagementDialog->showError(result.value(QStringLiteral("error"))
                .toString(QStringLiteral("聊天室详情加载失败")));
        }
        return;
    }
    if (action == QStringLiteral("update_details") || action == QStringLiteral("remove_member") ||
        action == QStringLiteral("leave") || action == QStringLiteral("dissolve")) {
        if (requestId != managementRequestId || action != managementAction) return;
        const QString operatedRoom = managementRoomId;
        managementTimer->stop();
        managementRequestId.clear();
        managementAction.clear();
        managementRoomId.clear();
        if (roomManagementDialog && roomManagementDialog->roomId() == operatedRoom)
            roomManagementDialog->setPending(false);
        if (!ok) {
            if (roomManagementDialog && roomManagementDialog->roomId() == operatedRoom)
                roomManagementDialog->showError(result.value(QStringLiteral("error"))
                    .toString(QStringLiteral("聊天室操作失败")));
            else roomSidebar->setNotice(result.value(QStringLiteral("error"))
                    .toString(QStringLiteral("聊天室操作失败")));
            return;
        }
        if (action == QStringLiteral("update_details")) {
            applyRoomUpdate(result.value(QStringLiteral("room")).toObject());
            if (roomManagementDialog && roomManagementDialog->roomId() == operatedRoom)
                roomManagementDialog->showPage(QStringLiteral("details"));
        } else if (action == QStringLiteral("remove_member")) {
            requestRoomMembers();
            sendRoomRequest(QStringLiteral("details"), {{QStringLiteral("room_id"), operatedRoom}});
        } else revokeRoomAccess(operatedRoom);
        return;
    }
    if (action == QStringLiteral("search_users")) {
        if (!roomInviteDialog || requestId != inviteSearchRequestId ||
            result.value(QStringLiteral("room_id")).toString() != roomInviteDialog->roomId()) return;
        if (ok) roomInviteDialog->setResults(result.value(QStringLiteral("users")).toArray());
        else roomInviteDialog->showError(result.value(QStringLiteral("error"))
            .toString(QStringLiteral("用户搜索失败")));
        return;
    }
    if (action == QStringLiteral("pending_invites")) {
        if (ok) {
            pendingRoomInvites = result.value(QStringLiteral("invites")).toArray();
            updateInviteBadge();
            if (roomInviteInboxDialog) roomInviteInboxDialog->setInvites(pendingRoomInvites);
        } else if (roomInviteInboxDialog) {
            roomInviteInboxDialog->showError(result.value(QStringLiteral("error"))
                .toString(QStringLiteral("邀请列表加载失败")));
        }
        return;
    }
    if (action == QStringLiteral("send_user_invites")) {
        if (!inviteSendInFlight || requestId != inviteSendRequestId) return;
        inviteSendInFlight = false;
        inviteSendTimer->stop();
        inviteSendRequestId.clear();
        if (roomInviteDialog) {
            roomInviteDialog->setPending(false);
            if (ok) roomInviteDialog->showSuccess(result.value(QStringLiteral("sent")).toArray().size());
            else roomInviteDialog->showError(result.value(QStringLiteral("error"))
                .toString(QStringLiteral("发送邀请失败")));
        } else {
            roomSidebar->setNotice(ok
                ? QStringLiteral("邀请已发送给 %1 位用户").arg(result.value(QStringLiteral("sent")).toArray().size())
                : result.value(QStringLiteral("error")).toString(QStringLiteral("发送邀请失败")));
        }
        return;
    }
    if (action == QStringLiteral("respond_invite")) {
        if (!inviteDecisionInFlight || requestId != inviteDecisionRequestId) return;
        inviteDecisionInFlight = false;
        inviteDecisionTimer->stop();
        inviteDecisionRequestId.clear();
        if (roomInviteInboxDialog) {
            roomInviteInboxDialog->setPending(false);
            if (ok) roomInviteInboxDialog->showError({});
            else roomInviteInboxDialog->showError(result.value(QStringLiteral("error"))
                .toString(QStringLiteral("处理邀请失败")));
        }
        refreshPendingInvites();
        if (ok && result.value(QStringLiteral("decision")).toString() == QStringLiteral("accept"))
            sendRoomRequest(QStringLiteral("list_mine"));
        return;
    }
    if (action == QStringLiteral("list_mine")) {
        if (!ok || !result.value(QStringLiteral("rooms")).isArray()) {
            roomSidebar->setNotice(result.value(QStringLiteral("error"))
                .toString(QStringLiteral("聊天室列表加载失败")));
            return;
        }
        joinedRooms = result.value(QStringLiteral("rooms")).toArray();
        roomListLoaded = true;
        roomSidebar->setRooms(joinedRooms);
        roomSidebar->setNotice(joinedRooms.isEmpty() ? QStringLiteral("尚未加入聊天室") : QString());
        roomSidebar->setCurrentRoom(currentRoomId);
        updateRoomTitle();
        requestRoomMembers();
        QSettings settings(QStringLiteral("VChat"), QStringLiteral("VChatClient"));
        const QString saved = settings.value(roomSettingsKey).toString();
        bool accessible = false;
        for (const QJsonValue &value : joinedRooms)
            if (value.toObject().value(QStringLiteral("room_id")).toString() == saved) accessible = true;
        if (roomRestorePending) {
            roomRestorePending = false;
            if (accessible && saved != currentRoomId) requestRoomSwitch(saved);
        }
        return;
    }
    if (action == QStringLiteral("create")) {
        if (!createRoomInFlight) return;
        createRoomInFlight = false;
        roomCreateTimer->stop();
        if (!ok || !result.value(QStringLiteral("room")).isObject()) {
            if (roomCreateDialog) {
                roomCreateDialog->setPending(false);
                roomCreateDialog->showError(result.value(QStringLiteral("error"))
                    .toString(QStringLiteral("创建聊天室失败，请稍后重试。")));
            }
            return;
        }
        const QJsonObject room = result.value(QStringLiteral("room")).toObject();
        const QString roomId = room.value(QStringLiteral("room_id")).toString();
        if (roomId.isEmpty()) {
            if (roomCreateDialog) {
                roomCreateDialog->setPending(false);
                roomCreateDialog->showError(QStringLiteral("服务器未返回聊天室 ID，请刷新聊天室列表。"));
            }
            return;
        }
        if (roomCreateDialog) roomCreateDialog->accept();
        roomRestorePending = false;
        bool found = false;
        for (const QJsonValue &value : joinedRooms)
            if (value.toObject().value(QStringLiteral("room_id")).toString() == roomId) found = true;
        if (!found) joinedRooms.append(room);
        roomSidebar->setRooms(joinedRooms);
        requestRoomSwitch(roomId);
        sendRoomRequest(QStringLiteral("list_mine"));
        return;
    }
    if (action == QStringLiteral("switch")) {
        if (!switchInFlight || (ok && result.value(QStringLiteral("room_id")).toString() != pendingRoomId))
            return;
        if (!ok) {
            switchInFlight = false;
            pendingRoomId.clear();
            ui->messageInput->setEnabled(true);
            ui->sendButton->setEnabled(!ui->messageInput->toPlainText().trimmed().isEmpty());
            roomSidebar->setCurrentRoom(currentRoomId);
            roomSidebar->setNotice(result.value(QStringLiteral("error"))
                .toString(QStringLiteral("切换聊天室失败")));
            updateRoomTitle();
            if (!queuedRoomId.isEmpty() && queuedRoomId != currentRoomId) {
                const QString next = queuedRoomId;
                queuedRoomId.clear();
                requestRoomSwitch(next);
            }
            return;
        }
        currentRoomId = pendingRoomId;
        roomHistoryLoading = true;
        clearMessageView();
        selectedMentions.clear();
        onlineMentionMembers.clear();
        if (mentionPopup) mentionPopup->hide();
        roomMembers.clear();
        onlineRoomUserIds.clear();
        onlineNames.clear();
        renderRoomMembers();
        roomSidebar->setCurrentRoom(currentRoomId);
        updateRoomTitle();
        requestRoomMembers();
        return;
    }
    if (action == QStringLiteral("members")) {
        if (!ok || result.value(QStringLiteral("room_id")).toString() != currentRoomId) return;
        if (roomManagementDialog && roomManagementDialog->roomId() == currentRoomId)
            roomManagementDialog->setMembers(result.value(QStringLiteral("members")).toArray());
        roomMembers.clear();
        for (const QJsonValue &value : result.value(QStringLiteral("members")).toArray()) {
            const QJsonObject member = value.toObject();
            bool valid = false;
            const qint64 id = member.value(QStringLiteral("user_id")).toString().toLongLong(&valid);
            const QString name = member.value(QStringLiteral("nickname")).toString();
            if (valid && !name.isEmpty())
                roomMembers.append({id, name,
                    member.contains(QStringLiteral("online")) ? member.value(QStringLiteral("online")).toBool()
                        : onlineRoomUserIds.contains(id),
                    member.value(QStringLiteral("role")).toString()});
        }
        renderRoomMembers();
        updateRoomTitle();
    }
}

bool MainWindow::currentMentionQuery(int *start, QString *query) const
{
    const QTextCursor cursor = ui->messageInput->textCursor();
    if (cursor.hasSelection()) return false;
    const QString prefix = ui->messageInput->toPlainText().left(cursor.position());
    static const QRegularExpression active(QStringLiteral(R"((?<![\p{L}\p{N}_])@([\p{L}\p{N}_]*)$)"));
    const auto match = active.match(prefix);
    if (!match.hasMatch()) return false;
    if (start) *start = match.capturedStart();
    if (query) *query = match.captured(1);
    return true;
}

void MainWindow::setupMentionPopup()
{
    mentionPopup = new QFrame(ui->centralwidget);
    mentionPopup->setObjectName(QStringLiteral("mentionPopup"));
    mentionPopup->setAttribute(Qt::WA_StyledBackground, true);
    mentionPopup->setStyleSheet(QStringLiteral(
        "QFrame#mentionPopup { background:#fff; border:1px solid #e7e0f5; border-radius:14px; } "
        "QListWidget#mentionList { background:transparent; border:0; outline:0; } "
        "QListWidget#mentionList::item { border:0; } "
        "QFrame#mentionRow { background:transparent; border-radius:10px; } "
        "QFrame#mentionRow:hover, QFrame#mentionRow[selected=\"true\"] { background:#f2edfc; }"));
    auto *shadow = new QGraphicsDropShadowEffect(mentionPopup);
    shadow->setBlurRadius(18);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(74, 50, 123, 35));
    mentionPopup->setGraphicsEffect(shadow);
    auto *layout = new QVBoxLayout(mentionPopup);
    layout->setContentsMargins(9, 9, 9, 9);
    layout->setSpacing(5);
    auto *title = new QLabel(QStringLiteral("选择要提及的成员"), mentionPopup);
    title->setStyleSheet(QStringLiteral("color:#756b8e; font-size:12px; padding:2px 5px;"));
    layout->addWidget(title);
    mentionList = new QListWidget(mentionPopup);
    mentionList->setObjectName(QStringLiteral("mentionList"));
    mentionList->setFocusPolicy(Qt::NoFocus);
    mentionList->setMouseTracking(true);
    mentionList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mentionList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mentionList->setSpacing(2);
    layout->addWidget(mentionList);
    mentionEmpty = new QLabel(QStringLiteral("没有找到相关成员"), mentionPopup);
    mentionEmpty->setAlignment(Qt::AlignCenter);
    mentionEmpty->setStyleSheet(QStringLiteral("color:#948aa7; font-size:13px; padding:12px;"));
    layout->addWidget(mentionEmpty);
    connect(mentionList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        mentionList->setCurrentItem(item);
        acceptMention();
    });
    connect(mentionList, &QListWidget::currentRowChanged, this, [this](int) {
        for (int i = 0; i < mentionList->count(); ++i) {
            QWidget *row = mentionList->itemWidget(mentionList->item(i));
            if (!row) continue;
            row->setProperty("selected", i == mentionList->currentRow());
            row->style()->unpolish(row);
            row->style()->polish(row);
        }
    });
    mentionPopup->hide();
}

void MainWindow::positionMentionPopup()
{
    if (!mentionPopup || !mentionPopup->isVisible()) return;
    const QRect chat = ui->chatPanel->geometry();
    const int width = qMin(360, chat.width() - 24);
    const int wanted = 42 + (visibleMentions.isEmpty() ? 49 : qMin(5, visibleMentions.size()) * 58);
    const int inputTop = ui->messageInput->mapTo(ui->centralwidget, QPoint(0, 0)).y();
    const int height = qMin(wanted, qMax(90, inputTop - chat.top() - 18));
    const QPoint cursor = ui->messageInput->viewport()->mapTo(ui->centralwidget,
        ui->messageInput->cursorRect().bottomLeft());
    const int x = qBound(chat.left() + 9, cursor.x() - 24, chat.right() - width - 9);
    mentionPopup->setGeometry(x, inputTop - height - 7, width, height);
    mentionPopup->raise();
}

void MainWindow::updateMentionPopup()
{
    if (!mentionPopup || ui->messageInput->composing()) {
        if (mentionPopup) mentionPopup->hide();
        return;
    }
    int start = 0;
    QString query;
    if (!currentMentionQuery(&start, &query)) { mentionPopup->hide(); return; }
    Q_UNUSED(start);
    visibleMentions.clear();
    if (QStringLiteral("Vivi").contains(query, Qt::CaseInsensitive))
        visibleMentions.append({0, QStringLiteral("Vivi"), true});
    QVector<MentionCandidate> members = onlineMentionMembers;
    if (members.isEmpty())
        for (const QString &name : onlineNames) members.append({0, name, false});
    for (const MentionCandidate &member : members) {
        if ((ownAccountId > 0 && member.id == ownAccountId) ||
            (member.id <= 0 && member.name == nickname)) continue; // No self mention.
        if (member.name.contains(query, Qt::CaseInsensitive)) visibleMentions.append(member);
    }
    mentionList->clear();
    QHash<QString, int> duplicateNames;
    for (const MentionCandidate &member : members) duplicateNames[member.name]++;
    for (const MentionCandidate &member : visibleMentions) {
        auto *item = new QListWidgetItem(mentionList);
        item->setSizeHint(QSize(0, 54));
        auto *row = new QFrame(mentionList);
        row->setObjectName(QStringLiteral("mentionRow"));
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setAttribute(Qt::WA_Hover, true);
        auto *horizontal = new QHBoxLayout(row);
        horizontal->setContentsMargins(9, 5, 9, 5);
        horizontal->setSpacing(10);
        if (member.ai) {
            auto *avatar = new QLabel(row);
            avatar->setFixedSize(38, 38);
            avatar->setAlignment(Qt::AlignCenter);
            avatar->setPixmap(iconFromSvg(QStringLiteral(":/login/vivi-robot.svg"), 22).pixmap(22, 22));
            avatar->setStyleSheet(QStringLiteral("background:#e8defc; border-radius:19px;"));
            horizontal->addWidget(avatar);
        } else horizontal->addWidget(avatarLabel(member.name, row, 38));
        auto *details = new QVBoxLayout;
        details->setContentsMargins(0, 0, 0, 0);
        details->setSpacing(2);
        QString name = member.name;
        if (!member.ai && duplicateNames.value(member.name) > 1 && member.id != 0)
            name += QStringLiteral("  #") + QString::number(member.id);
        auto *label = new QLabel(name, row);
        label->setStyleSheet(QStringLiteral("color:#30234e; font-size:13px; font-weight:600;"));
        details->addWidget(label);
        auto *status = new QLabel(member.ai
            ? QStringLiteral("AI 助手 · ") + (viviStatusLabel ? viviStatusLabel->text() : QStringLiteral("暂时不可用"))
            : QStringLiteral("在线成员 · 在线"), row);
        status->setStyleSheet(QStringLiteral("color:#827799; font-size:11px;"));
        details->addWidget(status);
        horizontal->addLayout(details, 1);
        mentionList->setItemWidget(item, row);
    }
    mentionEmpty->setVisible(visibleMentions.isEmpty());
    mentionList->setVisible(!visibleMentions.isEmpty());
    if (!visibleMentions.isEmpty()) mentionList->setCurrentRow(0);
    mentionPopup->show();
    positionMentionPopup();
}

void MainWindow::acceptMention()
{
    const int index = mentionList ? mentionList->currentRow() : -1;
    if (index < 0 || index >= visibleMentions.size()) return;
    int start = 0;
    QString query;
    if (!currentMentionQuery(&start, &query)) { mentionPopup->hide(); return; }
    const MentionCandidate candidate = visibleMentions.at(index);
    QTextCursor cursor = ui->messageInput->textCursor();
    const int end = cursor.position();
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    const QString mention = QStringLiteral("@") + candidate.name;
    cursor.insertText(mention + QLatin1Char(' '));
    ui->messageInput->setTextCursor(cursor);
    selectedMentions.append({start, int(mention.size()), candidate});
    mentionPopup->hide();
    ui->messageInput->setFocus();
}

void MainWindow::updateSelectedMentionRanges(int position, int removed, int added)
{
    for (int i = selectedMentions.size() - 1; i >= 0; --i) {
        SelectedMention &selected = selectedMentions[i];
        if (position + removed <= selected.start) selected.start += added - removed;
        else if (position >= selected.start + selected.length) continue;
        else selectedMentions.removeAt(i);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    auto *eventWidget = qobject_cast<QWidget *>(watched);
    if (viviMenuExpanded && eventWidget && eventWidget->window() == this &&
        event->type() == QEvent::KeyPress &&
        static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
        setViviMenuExpanded(false);
        return true;
    }
    if (viviMenuExpanded && event->type() == QEvent::MouseButtonPress) {
        if (eventWidget && eventWidget->window() == this &&
            eventWidget != ui->composer && !ui->composer->isAncestorOf(eventWidget))
            setViviMenuExpanded(false);
    }
    if (watched == ui->centralwidget && event->type() == QEvent::Resize)
        QTimer::singleShot(0, this, &MainWindow::positionMentionPopup);
    if (watched == ui->messageInput && event->type() == QEvent::KeyPress &&
        mentionPopup && mentionPopup->isVisible() && !ui->messageInput->composing()) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape) { mentionPopup->hide(); return true; }
        if (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down) {
            if (!visibleMentions.isEmpty()) {
                const int direction = key->key() == Qt::Key_Down ? 1 : -1;
                mentionList->setCurrentRow((mentionList->currentRow() + direction + visibleMentions.size()) % visibleMentions.size());
            }
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter || key->key() == Qt::Key_Tab) {
            if (!visibleMentions.isEmpty()) acceptMention();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::appendMessage(const QString &user, const QString &message, bool own, const QString &displayTime,
                               bool ai, const QString &aiRequestId, const QJsonArray &mentions,
                               const QJsonObject &replyTarget, const QString &streamMessageId,
                               const QString &userMessageId)
{
    if (!ai && !userMessageId.isEmpty()) {
        if (renderedUserMessageIds.contains(userMessageId)) return;
        renderedUserMessageIds.insert(userMessageId);
    }
    QScrollBar *scroll = ui->messageScroll->verticalScrollBar();
    const bool followBottom = scroll->value() >= scroll->maximum() - 24;
    const QString messageKey = (ai ? QStringLiteral("ai:") : own ? QStringLiteral("self:") : QStringLiteral("other:")) + user;
    auto *row = new QWidget(ui->messageContainer);
    row->setProperty("mentions", mentions.toVariantList());
    if (!streamMessageId.isEmpty()) row->setProperty("aiMessageId", streamMessageId);
    if (!userMessageId.isEmpty()) row->setProperty("userMessageId", userMessageId);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, lastMessageKey == messageKey ? 0 : 8, 0, 0);
    rowLayout->setSpacing(10);

    auto *avatar = avatarLabel(own ? QStringLiteral("我") : user, row);
    if (ai) {
        avatar->clear();
        avatar->setPixmap(iconFromSvg(QStringLiteral(":/login/vivi-robot.svg"), 22).pixmap(22, 22));
        avatar->setStyleSheet(QStringLiteral("background:#e8defc; border-radius:18px;"));
    }
    auto *column = new QWidget(row);
    auto *columnLayout = new QVBoxLayout(column);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->setSpacing(ai ? 8 : 5);
    auto *captionRow = new QHBoxLayout;
    captionRow->setContentsMargins(0, 0, 0, 0);
    captionRow->setSpacing(8);
    if (own) captionRow->addStretch();
    auto *nameLabel = new QLabel(own ? QStringLiteral("我") : user, column);
    nameLabel->setTextFormat(Qt::PlainText);
    QFont nameFont(QStringLiteral("Microsoft YaHei UI"));
    nameFont.setPixelSize(13);
    nameFont.setWeight(QFont::Medium);
    nameLabel->setFont(nameFont);
    nameLabel->setStyleSheet(ai ? QStringLiteral("color:#7956d5;") : QStringLiteral("color:#685d83;"));
    captionRow->addWidget(nameLabel);
    const QString replyName = replyTarget.value(QStringLiteral("nickname")).toString();
    if (ai && !replyName.isEmpty()) {
        auto *replyLabel = new QLabel(QStringLiteral("回复 @") + replyName, column);
        replyLabel->setObjectName(QStringLiteral("aiReplyLabel"));
        replyLabel->setTextFormat(Qt::PlainText);
        replyLabel->setStyleSheet(QStringLiteral("color:#9185aa; font-size:11px;"));
        captionRow->addWidget(replyLabel);
        row->setProperty("replyAccountId", replyTarget.value(QStringLiteral("accountId")).toString());
    }
    const QString rawTime = displayTime.isEmpty()
        ? QTime::currentTime().toString(QStringLiteral("hh:mm:ss"))
        : displayTime.contains(QLatin1Char('T'))
            ? QDateTime::fromString(displayTime, Qt::ISODateWithMs).time().toString(QStringLiteral("hh:mm:ss"))
            : displayTime;
    auto *timeLabel = new QLabel(rawTime.left(5), column);
    QFont timeFont(QStringLiteral("Microsoft YaHei UI"));
    timeFont.setPixelSize(11);
    timeFont.setWeight(QFont::Normal);
    timeLabel->setFont(timeFont);
    timeLabel->setStyleSheet(QStringLiteral("color:#9a91ad;"));
    captionRow->addWidget(timeLabel);
    if (!own) captionRow->addStretch();
    columnLayout->addLayout(captionRow);
    auto *bubble = new QFrame(column);
    if (ai) bubble->setObjectName(QStringLiteral("viviReadingCard"));
    bubble->setStyleSheet(ai
        ? QStringLiteral("QFrame#viviReadingCard { background-color:#faf8ff;"
                         " border:1px solid #e9dfff; border-radius:16px;"
                         " border-top-left-radius:8px; }")
        : own
        ? QStringLiteral("QFrame { background-color:#8064d9; border-radius:14px; border-top-right-radius:7px; } QLabel { color:white; font-size:14px; font-weight:400; }")
        : QStringLiteral("QFrame { background-color:#f3f0f9; border-radius:14px; border-top-left-radius:7px; } QLabel { color:#292342; font-size:14px; font-weight:400; }"));
    auto *bubbleLayout = new QVBoxLayout(bubble);
    bubbleLayout->setContentsMargins(ai ? 17 : 14, ai ? 13 : 10,
                                    ai ? 17 : 14, ai ? 13 : 10);
    bubbleLayout->setSpacing(0);
    if (ai) {
        auto *body = new AiMarkdownWidget(message, bubble);
        if (!streamMessageId.isEmpty()) body->setStreamingText({});
        bubbleLayout->addWidget(body);
        aiColumns.append(column);
        aiBubbles.append(bubble);
        aiBodies.append(body);
    } else {
        auto *body = new QLabel(message, bubble);
        body->setTextFormat(Qt::PlainText);
        body->setWordWrap(true);
        body->setTextInteractionFlags(Qt::TextSelectableByMouse);
        body->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        bubbleLayout->addWidget(body);
        bubbleColumns.append(column);
        messageBubbles.append(bubble);
        messageBodies.append(body);
    }
    bubble->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    columnLayout->addWidget(bubble, 0, own ? Qt::AlignRight : Qt::AlignLeft);
    if (ai) {
        auto *actionBar = new QWidget(column);
        actionBar->setObjectName(QStringLiteral("viviActionBar"));
        actionBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        auto *actions = new QGridLayout(actionBar);
        actions->setContentsMargins(17, 0, 0, 0);
        actions->setHorizontalSpacing(4);
        actions->setVerticalSpacing(2);
        int actionColumn = 0;
        auto makeAction = [actionBar, actions, &actionColumn](const QString &label, const QString &iconPath, const QString &tip) {
            auto *button = new QToolButton(actionBar);
            button->setText(label);
            button->setIcon(iconFromSvg(iconPath, 16));
            button->setIconSize(QSize(16, 16));
            button->setToolButtonStyle(label.isEmpty() ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
            button->setToolTip(tip);
            button->setCursor(Qt::PointingHandCursor);
            button->setStyleSheet(QStringLiteral(
                "QToolButton { color:#827799; border:0; border-radius:6px; padding:3px 5px; font-size:12px; background:transparent; } "
                "QToolButton:hover { color:#7956d5; background:#f2edfb; } "
                "QToolButton:checked { color:#6746c4; background:#e9e0fa; }"));
            actions->addWidget(button, 0, actionColumn++);
            return button;
        };
        auto *copy = makeAction(QStringLiteral("复制全文"), QStringLiteral(":/login/ai-copy.svg"), QStringLiteral("复制 Vivi 回复"));
        auto *retry = makeAction(QStringLiteral("重新生成"), QStringLiteral(":/login/refresh.svg"), QStringLiteral("按原问题和上下文重新生成"));
        auto *up = makeAction({}, QStringLiteral(":/login/ai-up.svg"), QStringLiteral("有帮助"));
        auto *down = makeAction({}, QStringLiteral(":/login/ai-down.svg"), QStringLiteral("没有帮助"));
        up->setObjectName(QStringLiteral("viviActionLike"));
        down->setObjectName(QStringLiteral("viviActionDislike"));
        up->setCheckable(true);
        down->setCheckable(true);
        connect(copy, &QToolButton::clicked, this, [this, body = aiBodies.last()]() {
            QGuiApplication::clipboard()->setText(body->plainText());
        });
        retry->setEnabled(!aiRequestId.isEmpty() &&
            (streamMessageId.isEmpty() || replyTarget.value(QStringLiteral("accountId")).toString()
                == QString::number(ownAccountId)));
        connect(retry, &QToolButton::clicked, this, [this, aiRequestId, streamMessageId]() {
            if (!isConnected || socket->state() != QAbstractSocket::ConnectedState) {
                appendSystemMessage(QStringLiteral("无法重新生成：服务器未连接。"));
                return;
            }
            QString originalId = aiRequestId;
            if (!streamMessageId.isEmpty()) {
                auto it = aiStreams.find(streamMessageId);
                if (it == aiStreams.end() || !it->finished || it->raw.isEmpty() ||
                    !it->canRegenerate || it->retryAwaiting) return;
                originalId = it->originalRequestId;
                // Prevent repeated clicks before the server broadcasts the new generation.
                it->retryAwaiting = true;
                for (QToolButton *button : it->actions) button->setEnabled(false);
            }
            socket->write(QStringLiteral("AI_REGENERATE|%1\n").arg(originalId).toUtf8());
        });
        connect(up, &QToolButton::clicked, this, [down](bool checked) { if (checked) down->setChecked(false); });
        connect(down, &QToolButton::clicked, this, [up](bool checked) { if (checked) up->setChecked(false); });
        if (!streamMessageId.isEmpty()) {
            AiStreamState state;
            state.body = aiBodies.last();
            state.actions = {copy, retry, up, down};
            state.requestId = aiRequestId;
            state.originalRequestId = aiRequestId;
            state.canRegenerate = !aiRequestId.startsWith(QStringLiteral("history:")) &&
                replyTarget.value(QStringLiteral("accountId")).toString()
                == QString::number(ownAccountId);
            for (QToolButton *button : state.actions) button->setEnabled(false);
            auto *cancel = makeAction(QStringLiteral("取消生成"), QStringLiteral(":/login/refresh.svg"),
                                      QStringLiteral("取消当前 Vivi 请求"));
            cancel->setObjectName(QStringLiteral("viviActionCancel"));
            cancel->setVisible(replyTarget.value(QStringLiteral("accountId")).toString()
                               == QString::number(ownAccountId));
            connect(cancel, &QToolButton::clicked, this, [this, streamMessageId]() {
                const auto it = aiStreams.constFind(streamMessageId);
                if (it != aiStreams.cend() && isConnected &&
                    socket->state() == QAbstractSocket::ConnectedState)
                    socket->write(QStringLiteral("AI_CANCEL|%1\n").arg(it->requestId).toUtf8());
            });
            state.cancel = cancel;
            aiStreams.insert(streamMessageId, state);
        }
        columnLayout->addWidget(actionBar, 0, Qt::AlignLeft);
    }
    if (own) {
        rowLayout->addStretch();
        rowLayout->addWidget(column);
        rowLayout->addWidget(avatar, 0, Qt::AlignTop);
    } else {
        rowLayout->addWidget(avatar, 0, Qt::AlignTop);
        rowLayout->addWidget(column);
        rowLayout->addStretch();
    }
    ui->messageLayout->insertWidget(ui->messageLayout->count() - 1, row);
    lastMessageKey = messageKey;
    updateBubbleWidths();
    QTimer::singleShot(0, this, [this, scroll, followBottom]() {
        updateBubbleWidths();
        if (followBottom) {
            ui->messageLayout->activate();
            scroll->setValue(scroll->maximum());
            // QTextDocument reports its final wrapped height after the layout pass.
            const int previousMax = scroll->maximum();
            QTimer::singleShot(40, this, [scroll, previousMax]() {
                if (scroll->value() >= previousMax - 24)
                    scroll->setValue(scroll->maximum());
            });
        }
    });
}

void MainWindow::flushAiStream(const QString &messageId)
{
    auto it = aiStreams.find(messageId);
    if (it == aiStreams.end() || it->finished || !it->body) return;
    QScrollBar *scroll = ui->messageScroll->verticalScrollBar();
    const int oldMax = scroll->maximum();
    const bool followBottom = scroll->value() >= oldMax - 24;
    it->body->setStreamingText(it->displayed);
    ++it->paints;
    const auto follow = [scroll, oldMax, followBottom]() {
        if (followBottom && scroll->value() >= oldMax - 24)
            scroll->setValue(scroll->maximum());
    };
    QTimer::singleShot(0, this, follow);
    QTimer::singleShot(40, this, follow);
}

void MainWindow::advanceAiTypewriter()
{
    bool pending = false;
    QList<QString> readyToFinish;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = aiStreams.begin(); it != aiStreams.end(); ++it) {
        AiStreamState &state = it.value();
        if (state.finished) continue;
        const int backlog = state.raw.size() - state.displayed.size();
        if (backlog <= 0) {
            if (state.completeReceived || state.errorReceived) readyToFinish.append(it.key());
            continue;
        }
        // Keep one trailing grapheme briefly so a combining mark in the next
        // network chunk cannot be shown as a separate character.
        QTextBoundaryFinder boundaries(QTextBoundaryFinder::Grapheme, state.raw);
        boundaries.setPosition(state.displayed.size());
        const int steps = backlog > 1500 ? 6 : backlog > 600 ? 4 : backlog > 200 ? 2 : 1;
        int position = state.displayed.size();
        for (int i = 0; i < steps; ++i) {
            const int next = boundaries.toNextBoundary();
            if (next < 0 || next <= position) break;
            if (next == state.raw.size() && !state.completeReceived && !state.errorReceived
                && now - state.lastChunkMs < 60) break;
            position = next;
        }
        if (position > state.displayed.size()) {
            state.displayed = state.raw.left(position);
            flushAiStream(it.key());
        }
        if (state.displayed.size() == state.raw.size() && (state.completeReceived || state.errorReceived))
            readyToFinish.append(it.key());
        else if (state.displayed.size() < state.raw.size()) pending = true;
    }
    for (const QString &id : readyToFinish) finalizeAiStream(id);
    if (!pending) aiTypewriterTimer->stop();
}

void MainWindow::finalizeAiStream(const QString &messageId)
{
    auto it = aiStreams.find(messageId);
    if (it == aiStreams.end() || it->finished || !it->body) return;
    it->finished = true;
    QScrollBar *scroll = ui->messageScroll->verticalScrollBar();
    const int oldMax = scroll->maximum();
    const bool followBottom = scroll->value() >= oldMax - 24;
    if (it->completeReceived) {
        it->body->finishStreaming(it->raw);
        it->previousContent.clear();
        for (QToolButton *button : it->actions) button->setEnabled(true);
        if (it->actions.size() >= 2) it->actions.at(1)->setEnabled(it->canRegenerate);
    } else {
        if (!it->previousContent.isEmpty()) {
            it->raw = it->previousContent;
            it->displayed = it->raw;
            it->body->finishStreaming(it->raw);
            for (QToolButton *button : it->actions) button->setEnabled(true);
            if (it->actions.size() >= 2) it->actions.at(1)->setEnabled(it->canRegenerate);
        } else {
            it->body->setStreamingText(it->raw.isEmpty() ? it->errorText
                : it->raw + QLatin1Char('\n') + it->errorText);
            if (!it->actions.isEmpty()) it->actions.first()->setEnabled(!it->raw.isEmpty());
        }
    }
    if (it->cancel) it->cancel->hide();
    updateBubbleWidths();
    qInfo() << "Vivi UI stream complete: chunks=" << it->chunksReceived
            << "chunk_span_ms=" << (it->lastChunkMs - it->firstChunkMs)
            << "max_chunk_gap_ms=" << it->maxChunkGapMs
            << "bubble_updates=" << it->paints;
    const auto follow = [scroll, oldMax, followBottom]() {
        if (followBottom && scroll->value() >= oldMax - 24)
            scroll->setValue(scroll->maximum());
    };
    QTimer::singleShot(0, this, follow);
    QTimer::singleShot(40, this, follow);
}

void MainWindow::appendAiStreamEvent(const QJsonObject &event)
{
    const QString kind = event.value(QStringLiteral("event")).toString();
    const QString requestId = event.value(QStringLiteral("request_id")).toString();
    const QString messageId = event.value(QStringLiteral("message_id")).toString();
    const QString generationId = event.value(QStringLiteral("generation_id")).toString();
    if (requestId.isEmpty() || messageId.isEmpty()) return;
    streamedRequestIds.insert(requestId);
    removeAiThinking(requestId);
    const bool existed = aiStreams.contains(messageId);
    if (!existed) {
        const QJsonObject target{{QStringLiteral("accountId"), event.value(QStringLiteral("user_id")).toString()},
                                 {QStringLiteral("nickname"), event.value(QStringLiteral("nickname")).toString()}};
        appendMessage(QStringLiteral("Vivi"), {}, false,
            event.value(QStringLiteral("created_at")).toString(), true, requestId, {}, target, messageId);
    }
    auto it = aiStreams.find(messageId);
    if (it == aiStreams.end() || !it->body) return;
    if (it->generationId.isEmpty()) it->generationId = generationId;
    // History is replayed before AiRoom's active snapshots. Attach the live
    // request to the row already placed by HISTORY, rather than adding a
    // second bubble or rejecting its chunks because the placeholder ID differs.
    if (it->requestId.startsWith(QStringLiteral("history:")) && !it->finished &&
        (kind == QStringLiteral("ai_start") || kind == QStringLiteral("ai_complete") ||
         kind == QStringLiteral("ai_error")) &&
        !event.value(QStringLiteral("regeneration")).toBool()) {
        it->requestId = requestId;
        it->originalRequestId = event.value(QStringLiteral("original_request_id")).toString();
        it->generationId = generationId;
        it->canRegenerate = event.value(QStringLiteral("user_id")).toString()
            == QString::number(ownAccountId);
        if (it->cancel) it->cancel->setVisible(it->canRegenerate);
    }
    if ((kind == QStringLiteral("ai_start") || kind == QStringLiteral("ai_complete")) && existed &&
        event.value(QStringLiteral("regeneration")).toBool() && it->finished &&
        ((generationId != it->generationId &&
          requestId.toULongLong() > it->requestId.toULongLong()) ||
         (it->interrupted && requestId == it->requestId && generationId == it->generationId))) {
        it->previousContent = it->raw.isEmpty()
            ? event.value(QStringLiteral("previous_text")).toString() : it->raw;
        it->body->beginStreaming();
        it->requestId = requestId;
        it->generationId = generationId;
        it->retryAwaiting = false;
        it->interrupted = false;
        it->raw.clear();
        it->displayed.clear();
        it->errorText.clear();
        it->sequence = 0;
        it->finished = false;
        it->completeReceived = false;
        it->errorReceived = false;
        it->chunksReceived = 0;
        it->paints = 0;
        it->firstChunkMs = 0;
        it->lastChunkMs = 0;
        it->maxChunkGapMs = 0;
        for (QToolButton *button : it->actions) button->setEnabled(false);
        if (it->cancel) it->cancel->setVisible(
            event.value(QStringLiteral("user_id")).toString() == QString::number(ownAccountId));
        qInfo() << "Vivi regeneration started: request=" << requestId
                << "message=" << messageId << "generation=" << generationId;
    }
    if (it->finished && it->requestId.startsWith(QStringLiteral("history:")) &&
        kind == QStringLiteral("ai_complete") &&
        it->raw == event.value(QStringLiteral("full_text")).toString()) {
        it->requestId = requestId;
        it->originalRequestId = event.value(QStringLiteral("original_request_id")).toString();
        it->generationId = generationId;
        it->canRegenerate = event.value(QStringLiteral("user_id")).toString()
            == QString::number(ownAccountId);
        if (it->actions.size() >= 2) it->actions.at(1)->setEnabled(it->canRegenerate);
    }
    if (it->finished) return;
    if (it->requestId != requestId || it->generationId != generationId) {
        qWarning() << "Vivi stream event rejected: request/message association mismatch";
        return;
    }
    const QString originalId = event.value(QStringLiteral("original_request_id")).toString();
    if (!originalId.isEmpty()) it->originalRequestId = originalId;
    if (event.value(QStringLiteral("regeneration")).toBool() && it->previousContent.isEmpty())
        it->previousContent = event.value(QStringLiteral("previous_text")).toString();
    const int sequence = event.value(QStringLiteral("sequence")).toInt();
    if (kind == QStringLiteral("ai_start")) {
        if (sequence >= it->sequence) {
            it->sequence = sequence;
            it->raw = event.value(QStringLiteral("full_text")).toString();
            if (!it->raw.isEmpty()) aiTypewriterTimer->start();
        }
    } else if (kind == QStringLiteral("ai_chunk")) {
        if (sequence <= it->sequence) return;
        it->sequence = sequence;
        it->raw += event.value(QStringLiteral("text")).toString();
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (it->chunksReceived == 0) it->firstChunkMs = now;
        else it->maxChunkGapMs = qMax(it->maxChunkGapMs, now - it->lastChunkMs);
        it->lastChunkMs = now;
        ++it->chunksReceived;
        if (!aiTypewriterTimer->isActive()) aiTypewriterTimer->start();
    } else if (kind == QStringLiteral("ai_complete") || kind == QStringLiteral("ai_error")) {
        it->sequence = sequence;
        if (kind == QStringLiteral("ai_complete")) {
            const QString finalText = event.value(QStringLiteral("full_text")).toString();
            if (!finalText.isEmpty() || it->raw.isEmpty()) it->raw = finalText;
            else qWarning() << "Vivi completion contained no text; retaining streamed content"
                            << "characters=" << it->raw.size();
            it->completeReceived = true;
        } else {
            it->errorText = event.value(QStringLiteral("error")).toString();
            it->errorReceived = true;
        }
        // A reconnecting client may receive only the final snapshot.
        if (!existed && it->chunksReceived == 0) it->displayed = it->raw;
        if (!it->raw.startsWith(it->displayed)) it->displayed.clear();
        if (it->displayed.size() == it->raw.size()) finalizeAiStream(messageId);
        else if (!aiTypewriterTimer->isActive()) aiTypewriterTimer->start();
    }
}

void MainWindow::appendAiThinking(const QString &id)
{
    removeAiThinking(id);
    QScrollBar *scroll = ui->messageScroll->verticalScrollBar();
    const bool followBottom = scroll->value() >= scroll->maximum() - 24;
    auto *row = new QWidget(ui->messageContainer);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->addStretch();
    auto *pill = new SystemPill(row, QColor("#eee8fc"));
    auto *inside = new QHBoxLayout(pill);
    inside->setContentsMargins(14, 8, 14, 8);
    inside->setSpacing(8);
    auto *icon = new QLabel(pill);
    icon->setPixmap(iconFromSvg(QStringLiteral(":/login/vivi-robot.svg"), 18).pixmap(18, 18));
    inside->addWidget(icon);
    auto *text = new QLabel(QStringLiteral("Vivi 正在思考…"), pill);
    text->setStyleSheet(QStringLiteral("color:#7956d5; font-size:13px;"));
    inside->addWidget(text);
    auto *pulse = new QTimer(row);
    pulse->setInterval(420);
    connect(pulse, &QTimer::timeout, row, [text, phase = 0]() mutable {
        phase = (phase + 1) % 4;
        text->setText(QStringLiteral("Vivi 正在思考") + QString(phase, QChar(0x2022)));
    });
    pulse->start();
    layout->addWidget(pill);
    layout->addStretch();
    ui->messageLayout->insertWidget(ui->messageLayout->count() - 1, row);
    aiThinkingRows.insert(id, row);
    QTimer::singleShot(0, this, [scroll, followBottom]() {
        if (followBottom) scroll->setValue(scroll->maximum());
    });
}

void MainWindow::removeAiThinking(const QString &id)
{
    QWidget *row = aiThinkingRows.take(id);
    if (!row) return;
    ui->messageLayout->removeWidget(row);
    row->deleteLater();
}

void MainWindow::setAiStatus(const QString &status)
{
    if (!viviStatusLabel || !viviStatusDot) return;
    const bool online = status == QStringLiteral("online");
    const bool thinking = status == QStringLiteral("thinking");
    viviStatusLabel->setText(online ? QStringLiteral("在线")
        : thinking ? QStringLiteral("思考中") : QStringLiteral("暂时不可用"));
    viviStatusDot->setStyleSheet(QStringLiteral("background:%1; border-radius:4px;")
        .arg(online ? QStringLiteral("#31b875") : thinking ? QStringLiteral("#9068f3") : QStringLiteral("#a6a0b3")));
    viviStatusLabel->setStyleSheet(QStringLiteral("color:%1; font-size:12px;")
        .arg(online ? QStringLiteral("#328d62") : thinking ? QStringLiteral("#7956d5") : QStringLiteral("#827a95")));
    viviStatusLabel->parentWidget()->setToolTip(online ? QStringLiteral("Vivi AI 服务已配置")
        : thinking ? QStringLiteral("Vivi 正在处理请求") : QStringLiteral("Vivi AI 服务暂时不可用"));
    if (mentionPopup && mentionPopup->isVisible()) updateMentionPopup();
}

void MainWindow::appendSystemMessage(const QString &message)
{
    lastMessageKey.clear();
    QScrollBar *scroll = ui->messageScroll->verticalScrollBar();
    const bool followBottom = scroll->value() >= scroll->maximum() - 24;
    const QDate today = QDate::currentDate();
    if (lastSystemDate != today) {
        auto *dateRow = new QWidget(ui->messageContainer);
        auto *dateLayout = new QHBoxLayout(dateRow);
        dateLayout->setContentsMargins(0, 0, 0, 2);
        dateLayout->addStretch();
        auto *dateLabel = new RoundedLabel(QStringLiteral("今天 ") + QTime::currentTime().toString(QStringLiteral("hh:mm")), dateRow);
        dateLabel->setAlignment(Qt::AlignCenter);
        dateLabel->setMinimumHeight(32);
        dateLabel->setFillColor(QColor("#f1ecfc"));
        dateLabel->setStyleSheet(QStringLiteral("color:#756991; padding:5px 13px; font-size:12px;"));
        dateLayout->addWidget(dateLabel);
        dateLayout->addStretch();
        ui->messageLayout->insertWidget(ui->messageLayout->count() - 1, dateRow);
        lastSystemDate = today;
    }
    auto *row = new QWidget(ui->messageContainer);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addStretch();
    auto *pill = new SystemPill(row);
    auto *pillLayout = new QHBoxLayout(pill);
    pillLayout->setContentsMargins(19, 8, 19, 8);
    pillLayout->setSpacing(9);
    const bool connectedNotice = message == QStringLiteral("已连接到聊天服务器");
    const bool welcomeNotice = message == QStringLiteral("欢迎来到 VChat 聊天室");
    pill->setProperty("hasIcon", connectedNotice || welcomeNotice);
    if (connectedNotice || welcomeNotice) {
        auto *icon = new QLabel(pill);
        icon->setFixedSize(20, 20);
        const QString path = connectedNotice ? QStringLiteral(":/login/connected.svg")
                                             : QStringLiteral(":/login/members.svg");
        icon->setPixmap(iconFromSvg(path, 20).pixmap(20, 20));
        pillLayout->addWidget(icon, 0, Qt::AlignVCenter);
    }
    auto *label = new QLabel(message, pill);
    label->setObjectName(QStringLiteral("systemMessageText"));
    label->setTextFormat(Qt::PlainText);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    QFont pillFont(QStringLiteral("Microsoft YaHei UI"));
    pillFont.setPixelSize(13);
    pillFont.setWeight(QFont::Medium);
    label->setFont(pillFont);
    label->setStyleSheet(QStringLiteral("color:#59468e; background:transparent;"));
    pillLayout->addWidget(label);
    systemPills.append(pill);
    const int available = qMax(180, int(ui->messageScroll->viewport()->width() * 0.78));
    const int natural = label->fontMetrics().horizontalAdvance(message) + 48 + (connectedNotice || welcomeNotice ? 28 : 0);
    pill->setFixedWidth(qMin(available, qMax(110, natural)));
    pill->setMinimumHeight(40);
    layout->addWidget(pill);
    layout->addStretch();
    ui->messageLayout->insertWidget(ui->messageLayout->count() - 1, row);
    QTimer::singleShot(0, this, [this, scroll, followBottom]() {
        updateBubbleWidths();
        if (followBottom)
            scroll->setValue(scroll->maximum());
    });
}

void MainWindow::appendPresenceNotice(const QString &name, bool joined)
{
    lastMessageKey.clear();
    QScrollBar *scroll = ui->messageScroll->verticalScrollBar();
    const bool followBottom = scroll->value() >= scroll->maximum() - 24;
    auto *row = new QWidget(ui->messageContainer);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addStretch();
    auto *pill = new SystemPill(row, QColor(joined ? "#eee8fc" : "#f1f0f5"));
    pill->setProperty("hasIcon", true);
    auto *pillLayout = new QHBoxLayout(pill);
    pillLayout->setContentsMargins(19, 8, 19, 8);
    pillLayout->setSpacing(8);
    auto *icon = new QLabel(pill);
    icon->setFixedSize(20, 20);
    icon->setPixmap(iconFromSvg(joined ? QStringLiteral(":/login/user-plus.svg")
                                       : QStringLiteral(":/login/log-out.svg"), 20).pixmap(20, 20));
    pillLayout->addWidget(icon, 0, Qt::AlignVCenter);
    auto *label = new QLabel(name + (joined ? QStringLiteral(" 加入了聊天室")
                                           : QStringLiteral(" 离开了聊天室")), pill);
    label->setObjectName(QStringLiteral("systemMessageText"));
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPixelSize(14);
    font.setWeight(QFont::Medium);
    label->setFont(font);
    label->setStyleSheet(joined ? QStringLiteral("color:#7658c4; background:transparent;")
                                : QStringLiteral("color:#8a849a; background:transparent;"));
    pillLayout->addWidget(label);
    const int natural = label->fontMetrics().horizontalAdvance(label->text()) + 48 + 28;
    const int available = qMax(180, int(ui->messageScroll->viewport()->width() * 0.78));
    pill->setFixedWidth(qMin(available, qMax(110, natural)));
    pill->setMinimumHeight(40);
    systemPills.append(pill);
    layout->addWidget(pill);
    layout->addStretch();
    ui->messageLayout->insertWidget(ui->messageLayout->count() - 1, row);
    QTimer::singleShot(0, this, [this, scroll, followBottom]() {
        updateBubbleWidths();
        if (followBottom)
            scroll->setValue(scroll->maximum());
    });
}

void MainWindow::appendIncomingLine(const QString &line)
{
    if (line.startsWith(QStringLiteral("SUMMARY_SHARE|"))) {
        const QJsonDocument document = QJsonDocument::fromJson(
            QByteArray::fromBase64(line.mid(14).toLatin1()));
        if (document.isObject()) handleSummaryShare(document.object());
        return;
    }
    if (line.startsWith(QStringLiteral("AI_SUMMARY|"))) {
        const QJsonDocument document = QJsonDocument::fromJson(
            QByteArray::fromBase64(line.mid(11).toLatin1()));
        if (document.isObject()) {
            if (document.object().value(QStringLiteral("event")).toString() == QStringLiteral("capability"))
                viviSummarySupported = true;
            else handleViviSummary(document.object());
        }
        return;
    }
    if (line.startsWith(QStringLiteral("ROOM|notice|"))) {
        const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromBase64(line.mid(12).toLatin1()));
        if (document.isObject()) handleRoomNotice(document.object());
        return;
    }
    if (line.startsWith(QStringLiteral("ROOM|result|"))) {
        const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromBase64(line.mid(12).toLatin1()));
        if (document.isObject()) handleRoomResult(document.object());
        return;
    }
    if (line.startsWith(QStringLiteral("HISTORY|"))) {
        const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromBase64(line.mid(8).toLatin1()));
        if (!document.isObject()) return;
        const QJsonObject item = document.object();
        if (item.value(QStringLiteral("room_id")).toString() != currentRoomId) return;
        const QString id = item.value(QStringLiteral("message_id")).toString();
        if (id.isEmpty()) return;
        const QString body = item.value(QStringLiteral("text")).toString();
        if (item.value(QStringLiteral("type")) == QStringLiteral("ai")) {
            auto existing = aiStreams.find(id);
            if (existing != aiStreams.end()) {
                if (existing->finished && existing->raw != body && existing->body) {
                    existing->body->beginStreaming();
                    existing->body->finishStreaming(body);
                    existing->raw = body;
                    existing->displayed = body;
                    updateBubbleWidths();
                }
                return;
            }
            const QString persistedState = item.value(QStringLiteral("ai_state")).toString();
            const bool pending = persistedState == QStringLiteral("pending");
            const bool failed = persistedState == QStringLiteral("failed") ||
                persistedState == QStringLiteral("cancelled") ||
                persistedState == QStringLiteral("timed_out");
            const QJsonObject event{{QStringLiteral("event"), pending ? QStringLiteral("ai_start")
                : failed ? QStringLiteral("ai_error") : QStringLiteral("ai_complete")},
                {QStringLiteral("request_id"), QStringLiteral("history:") + id},
                {QStringLiteral("message_id"), id},
                {QStringLiteral("generation_id"), QStringLiteral("persisted")},
                {QStringLiteral("original_request_id"), QStringLiteral("0")},
                {QStringLiteral("user_id"), item.value(QStringLiteral("sender_id")).toString()},
                {QStringLiteral("nickname"), item.value(QStringLiteral("reply_to_name")).toString()},
                {QStringLiteral("created_at"), item.value(QStringLiteral("time")).toString()},
                {QStringLiteral("sequence"), 0}, {QStringLiteral("full_text"), pending ? QString() : body},
                {QStringLiteral("error"), failed ? body : QString()}};
            appendAiStreamEvent(event);
            if (pending) {
                auto stream = aiStreams.find(id);
                if (stream != aiStreams.end() && stream->body) {
                    stream->body->setStreamingText(QStringLiteral("Vivi 正在思考…"));
                    if (stream->cancel) stream->cancel->hide();
                }
            }
        } else if (item.value(QStringLiteral("type")) == QStringLiteral("discussion_summary")) {
            QJsonObject shared = QJsonDocument::fromJson(body.toUtf8()).object();
            shared.insert(QStringLiteral("room_id"), currentRoomId);
            shared.insert(QStringLiteral("message_id"), id);
            shared.insert(QStringLiteral("shared_by_id"), item.value(QStringLiteral("sender_id")));
            shared.insert(QStringLiteral("shared_by_name"), item.value(QStringLiteral("sender")));
            shared.insert(QStringLiteral("time"), item.value(QStringLiteral("time")));
            appendSharedSummary(shared);
        } else if (item.value(QStringLiteral("type")) == QStringLiteral("user")) {
            const QString senderId = item.value(QStringLiteral("sender_id")).toString();
            appendMessage(item.value(QStringLiteral("sender")).toString(), body,
                !senderId.isEmpty() && senderId == QString::number(ownAccountId),
                item.value(QStringLiteral("time")).toString(), false, {}, {}, {}, {}, id);
        }
        return;
    }
    if (line == QStringLiteral("HISTORY_END")) {
        if (roomHistoryLoading) {
            roomHistoryLoading = false;
            switchInFlight = false;
            pendingRoomId.clear();
            ui->messageInput->setEnabled(true);
            ui->sendButton->setEnabled(!ui->messageInput->toPlainText().trimmed().isEmpty());
            roomSidebar->setNotice({});
            QSettings settings(QStringLiteral("VChat"), QStringLiteral("VChatClient"));
            settings.setValue(roomSettingsKey, currentRoomId);
            if (!queuedRoomId.isEmpty() && queuedRoomId != currentRoomId) {
                const QString next = queuedRoomId;
                queuedRoomId.clear();
                requestRoomSwitch(next);
            } else updateRoomTitle();
        } else if (!initialHistoryDone) {
            initialHistoryDone = true;
            if (!roomListRequested) {
                roomListRequested = true;
                sendRoomRequest(QStringLiteral("list_mine"));
            }
            refreshPendingInvites();
        }
        return;
    }
    if (line.startsWith(QStringLiteral("USERLIST2|"))) {
        const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromBase64(line.mid(10).toLatin1()));
        if (!document.isArray()) return;
        QVector<MentionCandidate> members;
        for (const QJsonValue &value : document.array()) {
            const QJsonObject object = value.toObject();
            bool valid = false;
            const qint64 id = object.value(QStringLiteral("id")).toString().toLongLong(&valid);
            const QString name = object.value(QStringLiteral("name")).toString();
            if (valid && !name.isEmpty()) members.append({id, name, false});
        }
        onlineMentionMembers = members;
        onlineRoomUserIds.clear();
        for (const MentionCandidate &member : members) onlineRoomUserIds.insert(member.id);
        if (roomListLoaded) {
            requestRoomMembers(); // Membership may have changed since the last snapshot.
        }
        mentionProtocol = true;
        if (mentionPopup && mentionPopup->isVisible()) updateMentionPopup();
        return;
    }
    if (line.startsWith(QStringLiteral("CHAT2|"))) {
        const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromBase64(line.mid(6).toLatin1()));
        if (!document.isObject()) return;
        const QJsonObject payload = document.object();
        const QString roomId = payload.value(QStringLiteral("room_id")).toString();
        if ((!roomId.isEmpty() && roomId != currentRoomId) ||
            (roomId.isEmpty() && currentRoomId != QStringLiteral("class-room"))) return;
        appendMessage(payload.value(QStringLiteral("sender")).toString(),
            payload.value(QStringLiteral("text")).toString(), false,
            payload.value(QStringLiteral("time")).toString(), false, {},
            payload.value(QStringLiteral("mentions")).toArray(), {}, {},
            payload.value(QStringLiteral("messageId")).toString());
        return;
    }
    if (line.startsWith(QStringLiteral("AI|"))) {
        const QStringList parts = line.split(QLatin1Char('|'));
        if (parts.size() != 4) return;
        const QString state = parts.at(1);
        const QString id = parts.at(2);
        const QString content = QString::fromUtf8(QByteArray::fromBase64(parts.at(3).toLatin1()));
        if (state == QStringLiteral("stream")) {
            const QJsonDocument document = QJsonDocument::fromJson(content.toUtf8());
            if (document.isObject() &&
                document.object().value(QStringLiteral("room_id")).toString() == currentRoomId &&
                document.object().value(QStringLiteral("request_id")).toString() == id)
                appendAiStreamEvent(document.object());
            return;
        }
        if (state == QStringLiteral("status")) setAiStatus(content);
        else if (state == QStringLiteral("meta")) {
            const QJsonDocument metadata = QJsonDocument::fromJson(content.toUtf8());
            if (metadata.isObject()) aiReplyTargets.insert(id, metadata.object());
        }
        else if (state == QStringLiteral("thinking")) {
            if (!streamedRequestIds.contains(id)) appendAiThinking(id);
        }
        else if (state == QStringLiteral("reply")) {
            if (streamedRequestIds.contains(id)) { aiReplyTargets.remove(id); return; }
            removeAiThinking(id);
            appendMessage(QStringLiteral("Vivi"), content, false, QString(), true, id, {}, aiReplyTargets.take(id));
        } else if (state == QStringLiteral("error")) {
            if (id == QStringLiteral("0")) {
                for (auto it = aiStreams.begin(); it != aiStreams.end(); ++it) {
                    if (!it->retryAwaiting) continue;
                    it->retryAwaiting = false;
                    for (QToolButton *button : it->actions) button->setEnabled(true);
                    if (it->actions.size() >= 2) it->actions.at(1)->setEnabled(it->canRegenerate);
                }
            }
            if (streamedRequestIds.contains(id)) return;
            removeAiThinking(id);
            aiReplyTargets.remove(id);
            appendSystemMessage(QStringLiteral("Vivi：") + content);
        }
        return;
    }
    if (line.startsWith(QStringLiteral("USERLIST|"))) {
        QStringList users = line.mid(QStringLiteral("USERLIST|").size()).split(',', Qt::SkipEmptyParts);
        for (QString &user : users)
            user = user.trimmed();
        updateUserList(users);
        return;
    }
    if (line.startsWith(QStringLiteral("[System]"))) {
        const QString notice = line.mid(QStringLiteral("[System]").size()).trimmed();
        if (notice == QStringLiteral("Connected to chat server") ||
            notice == QStringLiteral("Welcome %1").arg(nickname)) {
            return; // Already represented by local connection and welcome notices.
        }
        const QString joinedSuffix = QStringLiteral(" joined the chat");
        const QString leftSuffix = QStringLiteral(" left the chat");
        if (notice.endsWith(joinedSuffix) && notice.size() > joinedSuffix.size()) {
            appendPresenceNotice(notice.left(notice.size() - joinedSuffix.size()), true);
            return;
        }
        if (notice.endsWith(leftSuffix) && notice.size() > leftSuffix.size()) {
            appendPresenceNotice(notice.left(notice.size() - leftSuffix.size()), false);
            return;
        }
        appendSystemMessage(notice);
        return;
    }

    // The server forwards [hh:mm:ss] nickname: text. Preserve its time and sender.
    static const QRegularExpression timePrefix(QStringLiteral(R"(^\[(\d{2}:\d{2}:\d{2})\] (.*)$)"));
    const QRegularExpressionMatch match = timePrefix.match(line);
    const QString time = match.hasMatch() ? match.captured(1) : QString();
    const QString payload = match.hasMatch() ? match.captured(2) : line;
    QString sender;
    QString body;
    // Match the longest online nickname first, including names that contain a colon.
    for (const QString &candidate : onlineNames) {
        const QString prefix = candidate + QStringLiteral(": ");
        if (payload.startsWith(prefix) && candidate.size() > sender.size()) {
            sender = candidate;
            body = payload.mid(prefix.size());
        }
    }
    if (sender.isEmpty()) {
        const int separator = payload.indexOf(QStringLiteral(": "));
        if (separator > 0) {
            sender = payload.left(separator);
            body = payload.mid(separator + 2);
        }
    }
    if (sender.isEmpty()) {
        appendSystemMessage(payload);
        return;
    }
    appendMessage(sender, body, false, time);
}

void MainWindow::updateUserList(const QStringList &users)
{
    if (roomListLoaded) {
        onlineNames = users;
        renderRoomMembers();
        return;
    }
    QStringList incomingNames;
    if (isConnected) {
        for (const QString &user : users) {
            if (!user.trimmed().isEmpty())
                incomingNames.append(user.trimmed());
        }
    }
    // Identical broadcasts need no widget rebuild, which also avoids visible flicker.
    if (incomingNames == onlineNames && renderedUserListOwner == nickname)
        return;
    onlineNames = incomingNames;
    renderedUserListOwner = nickname;

    // USERLIST contains names only. The nickname sent in JOIN is the strongest
    // identity available without changing the protocol. Do not invent a missing self.
    QStringList displayNames = onlineNames;
    const int selfPosition = displayNames.indexOf(nickname);
    if (selfPosition > 0)
        displayNames.move(selfPosition, 0);
    ui->userList->clear();
    for (int index = 0; index < displayNames.size(); ++index) {
        const QString &user = displayNames.at(index);
        const bool isSelf = selfPosition >= 0 && index == 0;
        auto *item = new QListWidgetItem(ui->userList);
        item->setSizeHint(QSize(0, 72));
        auto *row = new QFrame(ui->userList);
        row->setObjectName(QStringLiteral("memberCard"));
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setAttribute(Qt::WA_Hover, true);
        row->setStyleSheet(isSelf
            ? QStringLiteral("QFrame#memberCard { background:#f5f0fd; border-radius:13px; } QFrame#memberCard:hover { background:#efe7fa; }")
            : QStringLiteral("QFrame#memberCard { background:transparent; border-radius:13px; } QFrame#memberCard:hover { background:#f7f4fc; }"));
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(12, 8, 12, 8);
        layout->setSpacing(12);
        layout->addWidget(avatarLabel(user, row, 42));
        auto *details = new QVBoxLayout();
        details->setSpacing(4);
        auto *name = new QLabel(isSelf ? user + QStringLiteral("（我）") : user, row);
        name->setTextFormat(Qt::PlainText);
        name->setStyleSheet(QStringLiteral("color:#252044; font-size:14px; font-weight:600;"));
        details->addWidget(name);
        auto *status = new QHBoxLayout();
        status->setContentsMargins(0, 0, 0, 0);
        status->setSpacing(6);
        auto *dot = new QLabel(row);
        dot->setFixedSize(8, 8);
        dot->setStyleSheet(QStringLiteral("background-color:#31b875; border-radius:4px;"));
        status->addWidget(dot);
        auto *online = new QLabel(QStringLiteral("在线"), row);
        online->setStyleSheet(QStringLiteral("color:#79718f; font-size:12px;"));
        status->addWidget(online);
        status->addStretch();
        details->addLayout(status);
        layout->addLayout(details, 1);
        ui->userList->setItemWidget(item, row);
    }
    const QString count = QString::number(onlineNames.size());
    ui->onlineBadge->setText(count + QStringLiteral(" 人在线"));
    ui->onlineCountLabel->setText(QStringLiteral("在线人数：") + count);
    updateMemberListHeight();
    if (mentionPopup && mentionPopup->isVisible()) updateMentionPopup();
}

void MainWindow::renderRoomMembers()
{
    if (!roomListLoaded) return;
    QVector<RoomMember> display = roomMembers;
    std::stable_sort(display.begin(), display.end(), [this](const RoomMember &a, const RoomMember &b) {
        const auto rank = [this](const RoomMember &m) {
            if (m.id == ownAccountId) return 0;
            if (m.role == QStringLiteral("owner")) return 1;
            return m.online ? 2 : 3;
        };
        return rank(a) < rank(b);
    });
    ui->userList->clear();
    int onlineCount = 0;
    for (const RoomMember &member : display) {
        if (member.online) ++onlineCount;
        const bool isSelf = member.id == ownAccountId;
        auto *item = new QListWidgetItem(ui->userList);
        item->setSizeHint(QSize(0, 72));
        auto *row = new QFrame(ui->userList);
        row->setObjectName(QStringLiteral("memberCard"));
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setAttribute(Qt::WA_Hover, true);
        row->setStyleSheet(isSelf
            ? QStringLiteral("QFrame#memberCard { background:#f5f0fd; border-radius:13px; } QFrame#memberCard:hover { background:#efe7fa; }")
            : QStringLiteral("QFrame#memberCard { background:transparent; border-radius:13px; } QFrame#memberCard:hover { background:#f7f4fc; }"));
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(12, 8, 12, 8);
        layout->setSpacing(12);
        layout->addWidget(avatarLabel(member.name, row, 42));
        auto *details = new QVBoxLayout;
        details->setSpacing(4);
        auto *name = new QLabel(isSelf ? member.name + QStringLiteral("（我）") : member.name, row);
        name->setTextFormat(Qt::PlainText);
        name->setStyleSheet(QStringLiteral("color:#252044; font-size:14px; font-weight:600;"));
        if (member.role == QStringLiteral("owner") && currentRoomId != QStringLiteral("class-room")) {
            auto *nameRow = new QHBoxLayout;
            nameRow->setContentsMargins(0, 0, 0, 0);
            nameRow->setSpacing(4);
            nameRow->addWidget(name);
            auto *owner = new QLabel(QStringLiteral("房主"), row);
            owner->setStyleSheet(QStringLiteral("color:#8058c9; background:#eee6fc;"
                " border-radius:7px; padding:1px 3px; font-size:10px;"));
            nameRow->addWidget(owner);
            nameRow->addStretch();
            details->addLayout(nameRow);
        } else {
            details->addWidget(name);
        }
        auto *status = new QHBoxLayout;
        status->setContentsMargins(0, 0, 0, 0);
        status->setSpacing(6);
        auto *dot = new QLabel(row);
        dot->setFixedSize(8, 8);
        dot->setStyleSheet(member.online
            ? QStringLiteral("background:#31b875; border-radius:4px;")
            : QStringLiteral("background:#b7afc6; border-radius:4px;"));
        status->addWidget(dot);
        auto *state = new QLabel(member.online ? QStringLiteral("在线") : QStringLiteral("离线"), row);
        state->setStyleSheet(QStringLiteral("color:#79718f; font-size:12px;"));
        status->addWidget(state);
        status->addStretch();
        details->addLayout(status);
        layout->addLayout(details, 1);
        ui->userList->setItemWidget(item, row);
    }
    ui->onlineTitle->setText(QStringLiteral("聊天室成员"));
    ui->onlineBadge->setText(QStringLiteral("%1 人在线").arg(onlineCount));
    ui->onlineCountLabel->setText(QStringLiteral("聊天室成员：%1 · 在线：%2")
        .arg(display.size()).arg(onlineCount));
    updateMemberListHeight();
}

void MainWindow::updateMemberListHeight()
{
    // The real user list scrolls when crowded; Vivi stays in its own card below it.
    const int needed = (roomListLoaded ? roomMembers.size() : onlineNames.size()) * 76;
    const int available = qMax(100, ui->rightPanel->height() - 215);
    ui->userList->setFixedHeight(qMin(needed, available));
}

void MainWindow::updateBubbleWidths()
{
    const int limit = qMax(180, int(ui->messageScroll->viewport()->width() * 0.68));
    for (QWidget *column : bubbleColumns)
        column->setMaximumWidth(limit);
    for (int i = 0; i < messageBubbles.size(); ++i) {
        QFrame *bubble = messageBubbles.at(i);
        QLabel *body = messageBodies.at(i);
        int natural = 0;
        for (const QString &line : body->text().split('\n'))
            natural = qMax(natural, body->fontMetrics().horizontalAdvance(line));
        bubble->setMaximumWidth(limit);
        bubble->setMinimumWidth(qMin(limit, qMax(56, natural + 28)));
    }
    const int aiLimit = qMax(210, qMin(700, int(ui->messageScroll->viewport()->width() * 0.76)));
    for (int i = 0; i < aiBubbles.size(); ++i) {
        aiColumns.at(i)->setMaximumWidth(aiLimit);
        QFrame *bubble = aiBubbles.at(i);
        AiMarkdownWidget *body = aiBodies.at(i);
        int natural = 0;
        const QFontMetrics metrics(body->font());
        for (const QString &line : body->plainText().split(QLatin1Char('\n')))
            natural = qMax(natural, metrics.horizontalAdvance(line));
        const int desired = body->isStreaming() ? aiLimit
            : qMin(aiLimit, qMax(140, natural + 38));
        body->setContentWidth(qMax(100, desired - 34));
        bubble->setFixedWidth(desired);
        QWidget *actionBar = aiColumns.at(i)->findChild<QWidget *>(QStringLiteral("viviActionBar"));
        if (!actionBar) continue;
        const bool compactActions = aiLimit < 290;
        if (actionBar->property("compactActions").toBool() == compactActions) continue;
        auto *grid = qobject_cast<QGridLayout *>(actionBar->layout());
        QToolButton *like = actionBar->findChild<QToolButton *>(QStringLiteral("viviActionLike"));
        QToolButton *dislike = actionBar->findChild<QToolButton *>(QStringLiteral("viviActionDislike"));
        QToolButton *cancel = actionBar->findChild<QToolButton *>(QStringLiteral("viviActionCancel"));
        if (!grid || !like || !dislike) continue;
        grid->removeWidget(like);
        grid->removeWidget(dislike);
        if (cancel) grid->removeWidget(cancel);
        const int row = compactActions ? 1 : 0;
        const int column = compactActions ? 0 : 2;
        grid->addWidget(like, row, column);
        grid->addWidget(dislike, row, column + 1);
        if (cancel) grid->addWidget(cancel, row, column + 2);
        actionBar->setProperty("compactActions", compactActions);
        actionBar->updateGeometry();
    }
    const int summaryLimit = qMin(540, qMax(120,
        ui->messageScroll->viewport()->width() - 60));
    for (QFrame *card : summaryCards) {
        card->setMinimumWidth(qMin(summaryLimit, qMax(220, int(summaryLimit * 0.92))));
        card->setMaximumWidth(summaryLimit);
        card->updateGeometry();
    }
    const int systemLimit = qMax(180, int(ui->messageScroll->viewport()->width() * 0.78));
    for (QFrame *pill : systemPills) {
        QLabel *label = pill->findChild<QLabel *>(QStringLiteral("systemMessageText"));
        const bool hasIcon = pill->property("hasIcon").toBool();
        const int natural = label->fontMetrics().horizontalAdvance(label->text()) + 48 + (hasIcon ? 28 : 0);
        pill->setFixedWidth(qMin(systemLimit, qMax(110, natural)));
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (ui && ui->messageScroll) {
        updateBubbleWidths();
        QTimer::singleShot(0, this, &MainWindow::updateMemberListHeight);
        QTimer::singleShot(0, this, [this] { updateViviMenuLayout(); });
    }
}
