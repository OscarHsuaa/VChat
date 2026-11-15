#include "mentioninput.h"

#include <QApplication>
#include <QClipboard>
#include <QEventLoop>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QDebug>

static void settle()
{
    QEventLoop loop;
    QTimer::singleShot(30, &loop, &QEventLoop::quit);
    loop.exec();
}

static void press(MentionInput &input, int key, Qt::KeyboardModifiers modifiers = {})
{
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(&input, &event);
    settle();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
    app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
    QWidget panel;
    auto *outer = new QVBoxLayout(&panel);
    auto *messages = new QWidget(&panel);
    messages->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    outer->addWidget(messages, 1);
    auto *composer = new QFrame(&panel);
    composer->setMinimumHeight(76);
    composer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *row = new QHBoxLayout(composer);
    row->setContentsMargins(16, 12, 16, 12);
    auto *input = new MentionInput(composer);
    input->setObjectName(QStringLiteral("messageInput"));
    input->setStyleSheet(QStringLiteral(
        "MentionInput#messageInput { font-size:14px; padding:13px 16px;"
        "border:1px solid #e4ddf3; border-radius:13px; }"));
    input->setMinimumHeight(50);
    input->setPlaceholderText(QStringLiteral("请输入消息..."));
    input->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    row->addWidget(input);
    auto *send = new QPushButton(QStringLiteral("发送"), composer);
    send->setFixedHeight(50);
    row->addWidget(send);
    row->setAlignment(send, Qt::AlignBottom);
    outer->addWidget(composer);
    panel.resize(680, 420);
    panel.show();
    settle();
    input->setFocus();
    settle();
    if (argc > 1) input->grab().save(QString::fromLocal8Bit(argv[1]));

    auto check = [](bool ok, const char *name) {
        if (!ok) qCritical() << "FAIL" << name;
        return ok;
    };
    bool ok = true;
    ok &= check(input->height() == 50 && composer->height() == 76,
                "initial height");
    input->setPlainText(QStringLiteral("你好"));
    settle();
    if (argc > 2) input->grab().save(QString::fromLocal8Bit(argv[2]));
    ok &= check(input->height() == 50, "one line");
    const int messageHeight = messages->height();
    int previousHeight = input->height();
    for (int lines = 2; lines <= 4; ++lines) {
        input->setPlainText(QStringList(lines, QStringLiteral("一行文字")).join(QLatin1Char('\n')));
        settle();
        ok &= check(input->height() > previousHeight, "two to four lines grow");
        previousHeight = input->height();
    }
    input->setPlainText(QStringLiteral("第一行\n第二行\n第三行\n第四行\n第五行"));
    settle();
    ok &= check(input->height() > 50 && composer->height() > 76 &&
                messages->height() < messageHeight, "five lines grow composer");
    ok &= check(send->geometry().bottom() == input->geometry().bottom(),
                "send button bottom alignment");
    input->setPlainText(QStringLiteral("一行\n二行\n三行\n四行\n五行\n六行\n七行"));
    settle();
    ok &= check(input->verticalScrollBar()->maximum() > 0,
                "overflow scrolls inside input");
    input->clear();
    settle();
    if (argc > 3) input->grab().save(QString::fromLocal8Bit(argv[3]));
    ok &= check(input->height() == 50 && composer->height() == 76,
                "deletion shrinks composer");

    input->setPlainText(QString(90, QLatin1Char('x')));
    settle();
    const int wideHeight = input->height();
    panel.resize(420, 420);
    settle();
    ok &= check(input->height() > wideHeight, "window width rewraps");
    input->clear();
    QApplication::clipboard()->setText(QStringLiteral("甲\n乙\n丙"));
    input->setFocus();
    press(*input, Qt::Key_V, Qt::ControlModifier);
    ok &= check(input->toPlainText() == QStringLiteral("甲\n乙\n丙") &&
                input->height() > 50, "multiline paste");

    int sends = 0;
    QObject::connect(input, &MentionInput::returnPressed, &panel, [&]() {
        if (!input->toPlainText().trimmed().isEmpty()) {
            ++sends;
            input->clear();
        }
    });
    input->clear();
    press(*input, Qt::Key_Return);
    ok &= check(sends == 0, "blank input does not send");
    input->setPlainText(QStringLiteral("  \n  "));
    press(*input, Qt::Key_Return);
    ok &= check(sends == 0, "whitespace-only input does not send");
    input->setPlainText(QStringLiteral("@Vivi 你好"));
    press(*input, Qt::Key_Return, Qt::ShiftModifier);
    ok &= check(sends == 0 && input->toPlainText().contains(QLatin1Char('\n')),
                "shift enter inserts newline");
    press(*input, Qt::Key_Return);
    ok &= check(sends == 1 && input->height() == 50,
                "enter sends and restores height");

    QInputMethodEvent preedit(QStringLiteral("ni"), {});
    QApplication::sendEvent(input, &preedit);
    QKeyEvent candidateEnter(QEvent::KeyPress, Qt::Key_Return, {});
    QApplication::sendEvent(input, &candidateEnter);
    ok &= check(sends == 1, "IME preedit enter does not send");
    QInputMethodEvent commit(QString(), {});
    commit.setCommitString(QStringLiteral("你"));
    QApplication::sendEvent(input, &commit);
    QKeyEvent commitEnter(QEvent::KeyPress, Qt::Key_Return, {});
    QApplication::sendEvent(input, &commitEnter);
    ok &= check(sends == 1, "IME commit enter does not send");
    settle();
    press(*input, Qt::Key_Return);
    ok &= check(sends == 2, "next deliberate enter sends");

    if (ok) qInfo() << "VChatComposerTest passed";
    return ok ? 0 : 1;
}
