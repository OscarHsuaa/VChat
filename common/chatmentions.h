#pragma once

#include <QRegularExpression>

namespace ChatMentions {
inline constexpr int minimumAllMembers = 3;
inline const QString allName = QStringLiteral("全体成员");
inline const QString allToken = QStringLiteral("@全体成员");

inline bool isAllTokenAt(const QString &text, int start, int length)
{
    static const QRegularExpression token(
        QStringLiteral(R"((?<![\p{L}\p{N}\p{M}_@])@全体成员(?![\p{L}\p{N}\p{M}_@]))"));
    return start >= 0 && start <= text.size() && length == allToken.size() &&
        token.match(text, start, QRegularExpression::NormalMatch,
                    QRegularExpression::AnchorAtOffsetMatchOption).hasMatch();
}
}
