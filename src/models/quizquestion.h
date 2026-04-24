#pragma once

#include <QString>
#include <QStringList>

struct QuizQuestion
{
    QString id;
    QString category;
    QString difficulty;
    QString question;
    QStringList answers;
    int correctIndex = -1;
    QString explanation;

    [[nodiscard]] bool isValid() const
    {
        return !id.trimmed().isEmpty() &&
               !category.trimmed().isEmpty() &&
               !difficulty.trimmed().isEmpty() &&
               !question.trimmed().isEmpty() &&
               answers.size() >= 2 &&
               correctIndex >= 0 &&
               correctIndex < answers.size();
    }
};

