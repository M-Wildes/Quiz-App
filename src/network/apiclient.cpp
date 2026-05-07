#include "apiclient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

// Core client state
ApiClient::ApiClient(QUrl baseUrl)
    : m_baseUrl(baseUrl)
{
}

QUrl ApiClient::baseUrl() const
{
    return m_baseUrl;
}

void ApiClient::setBaseUrl(const QUrl &baseUrl)
{
    m_baseUrl = baseUrl;
}

// Auth and read/write API calls
QNetworkReply *ApiClient::login(const QString &email, const QString &password)
{
    const QJsonObject body{
        {QStringLiteral("email"), email},
        {QStringLiteral("password"), password},
    };

    return m_network.post(
        makeJsonRequest(QStringLiteral("/api/auth/login")),
        QJsonDocument(body).toJson(QJsonDocument::Compact)
    );
}

QNetworkReply *ApiClient::fetchPlayerStats(const QString &accessToken)
{
    return m_network.get(makeJsonRequest(QStringLiteral("/api/me/stats"), accessToken));
}

QNetworkReply *ApiClient::fetchCommunityQuizzes()
{
    return m_network.get(makeJsonRequest(QStringLiteral("/api/community-quizzes")));
}

QNetworkReply *ApiClient::fetchCommunityQuiz(const QString &slug)
{
    const QString encodedSlug = QString::fromUtf8(QUrl::toPercentEncoding(slug.trimmed()));
    return m_network.get(
        makeJsonRequest(QStringLiteral("/api/community-quizzes/%1").arg(encodedSlug))
    );
}

QNetworkReply *ApiClient::fetchLeaderboard()
{
    return m_network.get(makeJsonRequest(QStringLiteral("/api/leaderboard")));
}

QNetworkReply *ApiClient::uploadQuizResult(
    const QuizResultPayload &payload,
    const QString &accessToken
)
{
    return m_network.post(
        makeJsonRequest(QStringLiteral("/api/game/upload-result"), accessToken),
        QJsonDocument(payload.toJson()).toJson(QJsonDocument::Compact)
    );
}

// Shared request construction
QUrl ApiClient::endpoint(const QString &path) const
{
    // Every request is built from the currently selected backend root so local/live switching stays centralized.
    QUrl url = m_baseUrl;
    url.setPath(path);
    return url;
}

QNetworkRequest ApiClient::makeJsonRequest(
    const QString &path,
    const QString &bearerToken
) const
{
    QNetworkRequest request(endpoint(path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    if (!bearerToken.trimmed().isEmpty()) {
        request.setRawHeader(
            "Authorization",
            QByteArray("Bearer ") + bearerToken.trimmed().toUtf8()
        );
    }

    return request;
}
