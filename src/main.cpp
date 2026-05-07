#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QPalette>

#include "network/apiclient.h"
#include "ui/mainwindow.h"
#include "utils/appconfig.h"

namespace {

QPalette buildPalette()
{
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(10, 14, 20));
    palette.setColor(QPalette::WindowText, QColor(235, 241, 248));
    palette.setColor(QPalette::Base, QColor(20, 28, 39));
    palette.setColor(QPalette::AlternateBase, QColor(26, 35, 48));
    palette.setColor(QPalette::ToolTipBase, QColor(18, 24, 34));
    palette.setColor(QPalette::ToolTipText, QColor(235, 241, 248));
    palette.setColor(QPalette::Text, QColor(235, 241, 248));
    palette.setColor(QPalette::Button, QColor(22, 31, 43));
    palette.setColor(QPalette::ButtonText, QColor(235, 241, 248));
    palette.setColor(QPalette::Highlight, QColor(97, 188, 255));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    return palette;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(AppConfig::applicationName());
    app.setApplicationDisplayName(AppConfig::applicationDisplayName());
    app.setOrganizationName(AppConfig::organizationName());
    app.setStyle(QStringLiteral("Fusion"));
    app.setPalette(buildPalette());
    app.setFont(QFont(QStringLiteral("Segoe UI"), 10));
    app.setWindowIcon(QIcon(QStringLiteral(":/app.ico")));

    ApiClient apiClient(AppConfig::loadApiBaseUrl());
    MainWindow window(&apiClient);
    window.setWindowIcon(app.windowIcon());
    window.show();

    return app.exec();
}
