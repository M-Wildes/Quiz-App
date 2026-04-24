#pragma once

#include <QJsonObject>
#include <QString>

struct QuizResultPayload
{
    QString quizType;
    QString category;
    QString difficulty;
    int correctAnswers = 0;
    int totalQuestions = 0;
    int durationMs = 0;
    bool streakBonus = false;

    [[nodiscard]] QJsonObject toJson() const
    {
        return QJsonObject{
            {QStringLiteral("quizType"), quizType},
            {QStringLiteral("category"), category},
            {QStringLiteral("difficulty"), difficulty},
            {QStringLiteral("correctAnswers"), correctAnswers},
            {QStringLiteral("totalQuestions"), totalQuestions},
            {QStringLiteral("durationMs"), durationMs},
            {QStringLiteral("streakBonus"), streakBonus},
        };
    }
};

