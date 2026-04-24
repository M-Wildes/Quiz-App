#include "questionbank.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace {

QStringList candidatePaths()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString currentDir = QDir::currentPath();

    return {
        QDir(appDir).filePath(QStringLiteral("data/sample_questions.json")),
        QDir(appDir).filePath(QStringLiteral("../data/sample_questions.json")),
        QDir(appDir).filePath(QStringLiteral("../../data/sample_questions.json")),
        QDir(appDir).filePath(QStringLiteral("../../../data/sample_questions.json")),
        QDir(appDir).filePath(QStringLiteral("../../../../data/sample_questions.json")),
        QDir(appDir).filePath(QStringLiteral("../../../../../data/sample_questions.json")),
        QDir(currentDir).filePath(QStringLiteral("data/sample_questions.json")),
        QDir(currentDir).filePath(QStringLiteral("sample_questions.json")),
    };
}

QStringList readAnswers(const QJsonArray &array)
{
    QStringList answers;
    answers.reserve(array.size());

    for (const auto &value : array) {
        answers.push_back(value.toString());
    }

    return answers;
}

} // namespace

QVector<QuizQuestion> QuestionBank::loadFromDefaultLocation(QString *errorMessage)
{
    return loadFromFile(resolveDefaultQuestionPath(), errorMessage);
}

QVector<QuizQuestion> QuestionBank::loadFromFile(
    const QString &filePath,
    QString *errorMessage
)
{
    QVector<QuizQuestion> questions;

    if (filePath.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not find sample_questions.json.");
        }
        return questions;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not open question file: %1")
                                .arg(file.errorString());
        }
        return questions;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Question file did not contain a JSON array.");
        }
        return questions;
    }

    for (const auto &value : document.array()) {
        const QJsonObject object = value.toObject();
        QuizQuestion question;
        question.id = object.value(QStringLiteral("id")).toString();
        question.category = object.value(QStringLiteral("category")).toString();
        question.difficulty = object.value(QStringLiteral("difficulty")).toString();
        question.question = object.value(QStringLiteral("question")).toString();
        question.answers = readAnswers(object.value(QStringLiteral("answers")).toArray());
        question.correctIndex = object.value(QStringLiteral("correctIndex")).toInt(-1);
        question.explanation = object.value(QStringLiteral("explanation")).toString();

        if (question.isValid()) {
            questions.push_back(question);
        }
    }

    if (questions.isEmpty() && errorMessage != nullptr) {
        *errorMessage = QStringLiteral("No valid questions were found in %1.")
                            .arg(QDir::toNativeSeparators(filePath));
    }

    return questions;
}

QString QuestionBank::resolveDefaultQuestionPath()
{
    for (const auto &path : candidatePaths()) {
        if (QFile::exists(path)) {
            return path;
        }
    }

    return {};
}

QStringList QuestionBank::availableCategories(const QVector<QuizQuestion> &questions)
{
    QSet<QString> uniqueCategories;
    for (const auto &question : questions) {
        uniqueCategories.insert(question.category.trimmed());
    }

    QStringList categories = uniqueCategories.values();
    categories.sort(Qt::CaseInsensitive);
    return categories;
}

QVector<QuizQuestion> QuestionBank::filteredQuestions(
    const QVector<QuizQuestion> &questions,
    const QString &category,
    const QString &difficulty
)
{
    QVector<QuizQuestion> filtered;

    for (const auto &question : questions) {
        const bool categoryMatches =
            category == QStringLiteral("Mixed") ||
            category.trimmed().isEmpty() ||
            question.category.compare(category, Qt::CaseInsensitive) == 0;

        const bool difficultyMatches =
            difficulty.trimmed().isEmpty() ||
            question.difficulty.compare(difficulty, Qt::CaseInsensitive) == 0;

        if (categoryMatches && difficultyMatches) {
            filtered.push_back(question);
        }
    }

    return filtered;
}
