#pragma once

#include <QDateTime>
#include <QString>

struct PlayerProfile
{
    QString displayName;
    QString username;
    QString email;
    int level = 1;
    int totalXp = 0;
    int totalScore = 0;
    int quizzesCompleted = 0;
    int battlePassTier = 1;
    int battlePassXp = 0;
    int nextBattlePassXp = 200;
    int currentStreak = 0;
    int accuracy = 0;
    QString equippedProfilePicture = QStringLiteral("starter-badge");
    QString equippedAvatarFrame = QStringLiteral("clean-frame");
    QString equippedTitle = QStringLiteral("quiz-player");
    QString equippedTitleName = QStringLiteral("Quiz Player");
    QString equippedHoverAnimation = QStringLiteral("soft-lift");
    int cosmeticCount = 0;
    bool signedIn = false;
    QDateTime lastSyncedAt;
};
