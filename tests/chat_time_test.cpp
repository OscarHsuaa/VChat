#include "chattimeutils.h"

#include <QCoreApplication>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QDate today(2026, 11, 17);
    const auto local = [](const QDate &date, const QTime &time) {
        return QDateTime(date, time, QTimeZone::systemTimeZone());
    };
    const QDateTime first = local(today, QTime(9, 15));
    const QDateTime within = local(today, QTime(9, 18));
    const QDateTime exactlyFive = local(today, QTime(9, 20));
    const QDateTime beyondFive = local(today, QTime(9, 21));
    const QDateTime yesterday = local(today.addDays(-1), QTime(23, 59));
    const QDateTime midnight = local(today, QTime(0, 1));

    if (!ChatTime::needsLabel({}, first) || ChatTime::needsLabel(first, within) ||
        ChatTime::needsLabel(first, exactlyFive) || !ChatTime::needsLabel(first, beyondFive)) return 1;
    if (!ChatTime::needsLabel(beyondFive, first)) return 8;
    if (!ChatTime::needsLabel(yesterday, midnight)) return 2;
    if (ChatTime::label(first, today) != QStringLiteral("09:15") ||
        ChatTime::label(yesterday, today) != QStringLiteral("11月16日 23:59") ||
        ChatTime::label(local(today.addDays(-2), QTime(15, 20)), today)
            != QStringLiteral("11月15日 15:20")) return 3;
    if (ChatTime::label(local(QDate(2026, 11, 17), QTime(14, 30)),
                        QDate(2026, 11, 20)) != QStringLiteral("11月17日 14:30") ||
        ChatTime::label(local(QDate(2026, 11, 10), QTime(23, 30)), today)
            != QStringLiteral("11月10日 23:30") ||
        ChatTime::label(local(QDate(2025, 12, 25), QTime(10, 30)), today)
            != QStringLiteral("12月25日 10:30")) return 4;
    if (ChatTime::tooltip(local(today, QTime(10, 28, 36)))
            != QStringLiteral("2026-11-17 10:28:36")) return 5;
    if (ChatTime::parse(QStringLiteral("23:59:00"), midnight).date() != today.addDays(-1)) return 6;
    if (ChatTime::parse(QStringLiteral("09:15:00"), local(today, QTime(13, 0))).date()
        != today) return 9;
    if (ChatTime::parse(QStringLiteral("invalid"), first).isValid()) return 10;
    if (ChatTime::parse(QStringLiteral("2026-11-17T09:15:00.000"), first) != first) return 7;
    const QDateTime original = local(today, QTime(6, 38));
    const QString wire = original.toUTC().toString(Qt::ISODateWithMs);
    if (ChatTime::parse(wire, local(today.addDays(1), QTime(6, 40))) != original ||
        ChatTime::parse(wire, local(today.addDays(7), QTime(6, 40))) != original) return 11;
    return 0;
}
