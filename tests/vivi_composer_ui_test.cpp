#include "mainwindow.h"
#include "mentioninput.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFrame>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCursor>
#include <QTimer>
#include <QToolButton>
#include <cstdio>

static void settle(int milliseconds = 320)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    MainWindow window(QStringLiteral("测试用户"));
    window.resize(1200, 780);
    window.show();
    settle(100);
    auto *input = window.findChild<MentionInput *>(QStringLiteral("messageInput"));
    auto *menu = window.findChild<QFrame *>(QStringLiteral("viviFeatureMenu"));
    auto *surface = window.findChild<QFrame *>(QStringLiteral("composerSurface"));
    auto *button = window.findChild<QToolButton *>(QStringLiteral("viviComposerButton"));
    auto *emoji = window.findChild<QToolButton *>(QStringLiteral("emojiComposerButton"));
    auto *send = window.findChild<QPushButton *>(QStringLiteral("sendButton"));
    auto *chat = window.findChild<QFrame *>(QStringLiteral("chatPanel"));
    if (!input || !menu || !surface || !button || !emoji || !send || !chat ||
        window.findChildren<MentionInput *>().size() != 1 || menu->isVisible() ||
        input->mapTo(&window, QPoint(0, 0)).y() >= button->mapTo(&window, QPoint(0, 0)).y()) return 1;
    const QString capture = qEnvironmentVariable("VCHAT_UI_CAPTURE_DIR");
    if (!capture.isEmpty()) window.grab().save(QDir(capture).filePath(QStringLiteral("vivi-empty.png")));
    input->setPlainText(QStringLiteral("第一行\n第二行\n第三行"));
    QTextCursor selected = input->textCursor();
    selected.setPosition(0);
    selected.setPosition(3, QTextCursor::KeepAnchor);
    input->setTextCursor(selected);
    const QString original = input->toPlainText();
    const int cursorPosition = input->textCursor().position();
    const int anchorPosition = input->textCursor().anchor();
    button->click();
    settle(100);
    if (menu->height() <= 0 || menu->height() >= 116) return 11;
    if (!capture.isEmpty()) window.grab().save(QDir(capture).filePath(QStringLiteral("vivi-transition.png")));
    settle(230);
    if (!capture.isEmpty()) window.grab().save(QDir(capture).filePath(QStringLiteral("vivi-expanded.png")));
    if (!menu->isVisible() || !input->compactMode() || !button->isChecked() ||
        input->toPlainText() != original || input->textCursor().position() != cursorPosition ||
        input->textCursor().anchor() != anchorPosition ||
        qAbs(input->mapTo(&window, QPoint(0, input->height() / 2)).y() -
             button->mapTo(&window, QPoint(0, button->height() / 2)).y()) > 2 ||
        send->mapTo(&window, QPoint(0, 0)).x() <= input->mapTo(&window, QPoint(0, 0)).x()) {
        std::fprintf(stderr, "expanded: menu=%d compact=%d checked=%d text=%d cursor=%d/%d y=%d/%d x=%d/%d\n",
            menu->isVisible(), input->compactMode(), button->isChecked(),
            input->toPlainText() == original, input->textCursor().position(), input->textCursor().anchor(),
            input->mapTo(&window, QPoint(0, 0)).y(), button->mapTo(&window, QPoint(0, 0)).y(),
            input->mapTo(&window, QPoint(0, 0)).x(), send->mapTo(&window, QPoint(0, 0)).x());
        return 2;
    }
    button->click();
    settle();
    if (menu->isVisible() || input->compactMode() || input->toPlainText() != original ||
        input->textCursor().anchor() != anchorPosition ||
        input->mapTo(&window, QPoint(0, 0)).y() >= button->mapTo(&window, QPoint(0, 0)).y()) return 3;
    if (!capture.isEmpty()) window.grab().save(QDir(capture).filePath(QStringLiteral("vivi-default.png")));
    button->click();
    settle();
    auto *messageArea = window.findChild<QWidget *>(QStringLiteral("messageContainer"));
    QMouseEvent outside(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10),
                        QPointF(10, 10), Qt::LeftButton,
                        Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(messageArea, &outside);
    settle();
    if (menu->isVisible()) return 4;
    button->click();
    settle();
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(input, &escape);
    settle();
    if (menu->isVisible()) return 5;
    button->click();
    button->click();
    button->click();
    settle();
    if (!menu->isVisible() || !input->compactMode()) return 6;
    const auto cards = menu->findChildren<QPushButton *>(QStringLiteral("viviActionCard"));
    if (cards.size() != 3) return 12;
    const int chatWidthBeforeSummary = chat->width();
    cards.first()->click();
    settle();
    if (menu->isVisible()) return 13;
    auto *summary = window.findChild<QFrame *>(QStringLiteral("viviSummaryPanel"));
    auto *summaryClose = window.findChild<QToolButton *>(QStringLiteral("viviSummaryClose"));
    auto *members = window.findChild<QListWidget *>(QStringLiteral("userList"));
    if (!summary || !summaryClose || !members || !summary->isVisible() ||
        members->isVisible() || chat->width() != chatWidthBeforeSummary) return 19;
    if (!capture.isEmpty()) window.grab().save(QDir(capture).filePath(QStringLiteral("vivi-summary-panel.png")));
    summaryClose->click();
    settle();
    if (summary->isVisible() || !members->isVisible() ||
        chat->width() != chatWidthBeforeSummary) return 20;
    button->click();
    settle();
    input->setPlainText(QStringLiteral("一\n二\n三\n四\n五\n六\n七\n八"));
    settle(50);
    if (input->verticalScrollBar()->maximum() == 0 || input->height() != 44) return 10;
    window.resize(830, 560);
    settle(100);
    const QPoint menuTopLeft = menu->mapTo(&window, QPoint(0, 0));
    const QPoint chatTopLeft = chat->mapTo(&window, QPoint(0, 0));
    if (input->width() < 90 || menuTopLeft.x() < chatTopLeft.x() ||
        menuTopLeft.x() + menu->width() > chatTopLeft.x() + chat->width() ||
        menuTopLeft.y() + menu->height() > chatTopLeft.y() + chat->height() ||
        send->mapTo(&window, QPoint(send->width(), 0)).x() > chatTopLeft.x() + chat->width()) {
        std::fprintf(stderr, "narrow overflow: chat=%d,%d %dx%d menu=%d,%d %dx%d surface=%d,%d %dx%d\n",
            chatTopLeft.x(), chatTopLeft.y(), chat->width(), chat->height(), menuTopLeft.x(), menuTopLeft.y(),
            menu->width(), menu->height(), surface->mapTo(&window, QPoint(0, 0)).x(),
            surface->mapTo(&window, QPoint(0, 0)).y(), surface->width(), surface->height());
        return 7;
    }
    int enterCount = 0;
    QObject::connect(input, &MentionInput::returnPressed, &app, [&] { ++enterCount; });
    QKeyEvent shiftEnter(QEvent::KeyPress, Qt::Key_Return, Qt::ShiftModifier);
    QApplication::sendEvent(input, &shiftEnter);
    if (!input->toPlainText().contains(QLatin1Char('\n'))) return 8;
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(input, &enter);
    if (enterCount != 1) return 9;
    input->clear();
    input->insertPlainText(QStringLiteral("保留撤销"));
    button->click();
    settle();
    input->undo();
    if (!input->toPlainText().isEmpty()) return 15;
    input->setFocus();
    QInputMethodEvent preedit(QStringLiteral("ni"), {});
    QApplication::sendEvent(input, &preedit);
    if (!input->composing()) return 16;
    button->click();
    settle();
    if (!input->composing()) return 17;
    QInputMethodEvent commit(QString(), {});
    commit.setCommitString(QStringLiteral("你"));
    QApplication::sendEvent(input, &commit);
    if (input->toPlainText() != QStringLiteral("你")) return 18;
    settle(40);
    input->clear();
    if (!capture.isEmpty()) window.grab().save(QDir(capture).filePath(QStringLiteral("vivi-expanded-empty.png")));
    const QPoint finalMenu = menu->mapTo(&window, QPoint(0, 0));
    const QPoint finalChat = chat->mapTo(&window, QPoint(0, 0));
    std::fprintf(stderr, "final geometry: chat=%d,%d %dx%d menu=%d,%d %dx%d\n",
        finalChat.x(), finalChat.y(), chat->width(), chat->height(), finalMenu.x(), finalMenu.y(),
        menu->width(), menu->height());
    if (finalMenu.x() < finalChat.x() || finalMenu.x() + menu->width() > finalChat.x() + chat->width() ||
        finalMenu.y() + menu->height() > finalChat.y() + chat->height()) {
        std::fprintf(stderr, "final overflow: chat=%d,%d %dx%d menu=%d,%d %dx%d\n",
            finalChat.x(), finalChat.y(), chat->width(), chat->height(), finalMenu.x(), finalMenu.y(),
            menu->width(), menu->height());
        return 14;
    }
    std::fprintf(stdout, "VIVI_COMPOSER_UI one_input=true menu_toggle=true selection=true narrow=true keys=true\n");
    return 0;
}
