#include "appconfig.h"

#include <QSettings>

QString AppConfig::organizationName()
{
    return QStringLiteral("QuizForge");
}
    
QString AppConfig::applicationName()
{
    return QStringLiteral("QuizApp");
}

QString AppConfig::applicationDisplayName()
{
    return QStringLiteral("QuizForge Desktop");
}

QUrl AppConfig::defaultApiBaseUrl()
{
    const QString envOverride = qEnvironmentVariable("QUIZFORGE_API_BASE_URL");

    if (!envOverride.trimmed().isEmpty()) {
        return QUrl(envOverride.trimmed());
    }

    return QUrl(QStringLiteral("http://localhost:3000"));
}

QUrl AppConfig::loadApiBaseUrl()
{
    QSettings settings;
    const auto storedValue = settings.value(QStringLiteral("network/apiBaseUrl"));

    if (storedValue.isValid()) {
        const QUrl storedUrl(storedValue.toString());

        if (storedUrl.isValid() && !storedUrl.isEmpty()) {
            return storedUrl;
        }
    }

    return defaultApiBaseUrl();
}

void AppConfig::saveApiBaseUrl(const QUrl &url)
{
    QSettings settings;
    settings.setValue(QStringLiteral("network/apiBaseUrl"), url.toString());
}

