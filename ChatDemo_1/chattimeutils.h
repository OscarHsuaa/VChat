#ifndef CHATTIMEUTILS_H
#define CHATTIMEUTILS_H

#include <QDateTime>
#include <QString>
#include <QTimeZone>

namespace ChatTime {

inline QDateTime parse(const QString &raw, const QDateTime &now = QDateTime::currentDateTime())
{
    if (raw.isEmpty()) return now;
    QDateTime sent = QDateTime::fromString(raw, Qt::ISODateWithMs);
    if (!sent.isValid()) sent = QDateTime::fromString(raw, Qt::ISODate);
    if (sent.isValid()) return sent.toLocalTime();

    QTime time = QTime::fromString(raw, QStringLiteral("HH:mm:ss"));
    if (!time.isValid()) time = QTime::fromString(raw, QStringLiteral("HH:mm"));
    if (!time.isValid()) return {};
    QDate date = now.date();
    // Legacy live packets contain only a clock time. A packet from just
    // before midnight can arrive after midnight without a date component.
    if (time.msecsSinceStartOfDay() - now.time().msecsSinceStartOfDay()
        > 12 * 60 * 60 * 1000) date = date.addDays(-1);
    return QDateTime(date, time, QTimeZone::systemTimeZone());
}

inline bool needsLabel(const QDateTime &previous, const QDateTime &current)
{
    return current.isValid() && (!previous.isValid() || previous.date() != current.date() ||
        qAbs(previous.msecsTo(current)) > 5 * 60 * 1000);
}

inline QString label(const QDateTime &sent, const QDate &today = QDate::currentDate())
{
    if (!sent.isValid()) return {};
    const QDate localDate = sent.date();
    const QString clock = sent.time().toString(QStringLiteral("HH:mm"));
    if (localDate == today) return clock;
    return sent.toString(QStringLiteral("MM月dd日 HH:mm"));
}

inline QString tooltip(const QDateTime &sent)
{
    return sent.isValid() ? sent.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString();
}

} // namespace ChatTime

#endif // CHATTIMEUTILS_H
