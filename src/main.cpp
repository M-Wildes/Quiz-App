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
    // Keep the dark Aero palette centralized so dialogs and native widgets match the app shell.
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(21, 24, 28));
    palette.setColor(QPalette::WindowText, QColor(242, 244, 246));
    palette.setColor(QPalette::Base, QColor(32, 36, 42));
    palette.setColor(QPalette::AlternateBase, QColor(43, 48, 54));
    palette.setColor(QPalette::ToolTipBase, QColor(39, 43, 49));
    palette.setColor(QPalette::ToolTipText, QColor(242, 244, 246));
    palette.setColor(QPalette::Text, QColor(242, 244, 246));
    palette.setColor(QPalette::Button, QColor(68, 74, 82));
    palette.setColor(QPalette::ButtonText, QColor(242, 244, 246));
    palette.setColor(QPalette::Highlight, QColor(185, 195, 204));
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
