#pragma once

#include <QUrl>

class AppConfig
{
public:
    static QString organizationName();
    static QString applicationName();
    static QString applicationDisplayName();

    static QUrl defaultApiBaseUrl();
    static QUrl loadApiBaseUrl();
    static void saveApiBaseUrl(const QUrl &url);
};

