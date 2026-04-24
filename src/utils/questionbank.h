#pragma once

#include <QVector>

#include "models/quizquestion.h"

class QuestionBank
{
public:
    static QVector<QuizQuestion> loadFromDefaultLocation(QString *errorMessage = nullptr);
    static QVector<QuizQuestion> loadFromFile(
        const QString &filePath,
        QString *errorMessage = nullptr
    );
    static QString resolveDefaultQuestionPath();
    static QStringList availableCategories(const QVector<QuizQuestion> &questions);
    static QVector<QuizQuestion> filteredQuestions(
        const QVector<QuizQuestion> &questions,
        const QString &category,
        const QString &difficulty
    );
};

