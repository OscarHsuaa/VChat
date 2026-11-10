#pragma once

#include <QDialog>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QScreen>
#include <QToolButton>
#include <QVBoxLayout>

namespace RoomDialogChrome {
class DragTitleBar final : public QWidget {
public:
    using QWidget::QWidget;
protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            offset_ = event->globalPosition().toPoint() - window()->pos();
            dragging_ = true;
            event->accept();
        }
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (dragging_ && event->buttons().testFlag(Qt::LeftButton)) {
            window()->move(event->globalPosition().toPoint() - offset_);
            event->accept();
        }
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        dragging_ = false;
        QWidget::mouseReleaseEvent(event);
    }
private:
    QPoint offset_;
    bool dragging_ = false;
};

struct Shell {
    QVBoxLayout *layout = nullptr;
    QLabel *title = nullptr;
    QLabel *icon = nullptr;
};

inline Shell makeShell(QDialog *dialog, const QString &title, const QString &iconName,
                       const QString &style)
{
    dialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog->setAttribute(Qt::WA_TranslucentBackground);
    dialog->setStyleSheet(style);
    auto *outer = new QVBoxLayout(dialog);
    outer->setContentsMargins(16, 16, 16, 20);
    auto *panel = new QFrame(dialog);
    panel->setObjectName(QStringLiteral("roomPanel"));
    auto *shadow = new QGraphicsDropShadowEffect(panel);
    shadow->setBlurRadius(26);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(75, 48, 136, 42));
    panel->setGraphicsEffect(shadow);
    outer->addWidget(panel);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(26, 19, 26, 24);
    layout->setSpacing(13);
    auto *bar = new DragTitleBar(panel);
    bar->setFixedHeight(34);
    auto *barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(0, 0, 0, 0);
    barLayout->setSpacing(9);
    auto *icon = new QLabel(bar);
    icon->setFixedSize(21, 21);
    icon->setPixmap(QIcon(QStringLiteral(":/login/") + iconName).pixmap(21, 21));
    icon->setAttribute(Qt::WA_TransparentForMouseEvents);
    barLayout->addWidget(icon);
    auto *heading = new QLabel(title, bar);
    heading->setStyleSheet(QStringLiteral("font-size:16px; font-weight:700; color:#252044;"));
    heading->setAttribute(Qt::WA_TransparentForMouseEvents);
    barLayout->addWidget(heading);
    barLayout->addStretch();
    auto *close = new QToolButton(bar);
    close->setObjectName(QStringLiteral("dialogClose"));
    close->setIcon(QIcon(QStringLiteral(":/login/dialog-close.svg")));
    close->setIconSize(QSize(17, 17));
    close->setFixedSize(30, 30);
    close->setCursor(Qt::PointingHandCursor);
    close->setToolTip(QStringLiteral("关闭"));
    QObject::connect(close, &QToolButton::clicked, dialog, &QDialog::reject);
    barLayout->addWidget(close);
    layout->addWidget(bar);
    return {layout, heading, icon};
}

inline void centerOnParent(QDialog *dialog)
{
    QWidget *parent = dialog->parentWidget();
    if (!parent) return;
    const QPoint center = parent->mapToGlobal(parent->rect().center());
    QPoint topLeft = center - QPoint(dialog->width() / 2, dialog->height() / 2);
    if (QScreen *screen = QGuiApplication::screenAt(center)) {
        const QRect available = screen->availableGeometry();
        topLeft.setX(qBound(available.left(), topLeft.x(),
                             qMax(available.left(), available.right() - dialog->width() + 1)));
        topLeft.setY(qBound(available.top(), topLeft.y(),
                             qMax(available.top(), available.bottom() - dialog->height() + 1)));
    }
    dialog->move(topLeft);
}
}
