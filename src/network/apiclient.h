#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

#include "models/quizresultpayload.h"

class ApiClient
{
public:
    explicit ApiClient(QUrl baseUrl = {});

    [[nodiscard]] QUrl baseUrl() const;
    void setBaseUrl(const QUrl &baseUrl);

    QNetworkReply *login(const QString &email, const QString &password);
    QNetworkReply *signup(
        const QString &email,
        const QString &password,
        const QString &displayName,
        const QString &username
    );
    QNetworkReply *fetchPlayerStats(const QString &accessToken);
    QNetworkReply *fetchCommunityQuizzes();
    QNetworkReply *fetchCommunityQuiz(const QString &slug);
    QNetworkReply *fetchLeaderboard();
    QNetworkReply *uploadQuizResult(
        const QuizResultPayload &payload,
        const QString &accessToken
    );

private:
    [[nodiscard]] QUrl endpoint(const QString &path) const;
    [[nodiscard]] QNetworkRequest makeJsonRequest(
        const QString &path,
        const QString &bearerToken = {}
    ) const;

    QNetworkAccessManager m_network;
    QUrl m_baseUrl;
};
