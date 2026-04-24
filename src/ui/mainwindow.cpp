#include "mainwindow.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStatusBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include "network/apiclient.h"
#include "utils/appconfig.h"

namespace {

QLabel *makeLabel(const QString &text, const QString &name, bool wrap = true)
{
    auto *label = new QLabel(text);
    label->setObjectName(name);
    label->setWordWrap(wrap);
    return label;
}

QWidget *makeSectionHeader(
    const QString &eyebrow,
    const QString &title,
    const QString &body
)
{
    auto *section = new QWidget;
    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(makeLabel(eyebrow, QStringLiteral("eyebrow"), false));
    layout->addWidget(makeLabel(title, QStringLiteral("sectionTitle")));
    layout->addWidget(makeLabel(body, QStringLiteral("bodyCopy")));
    return section;
}

} // namespace

MainWindow::MainWindow(ApiClient *apiClient, QWidget *parent)
    : QMainWindow(parent)
    , m_apiClient(apiClient)
{
    setWindowTitle(QStringLiteral("QuizForge Desktop"));
    resize(1360, 860);
    setMinimumSize(1100, 720);

    setStyleSheet(QStringLiteral(R"(
        QMainWindow {
            background: #f6f0e7;
            color: #1d1715;
        }

        QWidget#navPanel,
        QWidget#contentPanel {
            background: transparent;
        }

        QLabel#appTitle {
            color: #1d1715;
            font-size: 28px;
            font-weight: 700;
        }

        QLabel#pageTitle {
            color: #1d1715;
            font-size: 34px;
            font-weight: 700;
        }

        QLabel#eyebrow {
            color: #69594d;
            font-size: 11px;
            font-weight: 700;
            letter-spacing: 2px;
            text-transform: uppercase;
        }

        QLabel#sectionTitle {
            color: #1d1715;
            font-size: 26px;
            font-weight: 700;
        }

        QLabel#bodyCopy {
            color: #69594d;
            font-size: 14px;
            line-height: 1.5;
        }

        QLabel#chip {
            background: rgba(255, 138, 0, 0.14);
            border: 1px solid rgba(255, 138, 0, 0.18);
            border-radius: 16px;
            color: #8d4200;
            font-size: 12px;
            font-weight: 700;
            padding: 8px 12px;
        }

        QFrame#card {
            background: rgba(255, 251, 245, 0.90);
            border: 1px solid rgba(62, 43, 22, 0.10);
            border-radius: 22px;
        }

        QPushButton#navButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 18px;
            color: #5f5146;
            font-size: 15px;
            font-weight: 600;
            padding: 14px 18px;
            text-align: left;
        }

        QPushButton#navButton:hover {
            background: rgba(255, 255, 255, 0.64);
            border-color: rgba(62, 43, 22, 0.08);
            color: #1d1715;
        }

        QPushButton#navButton:checked {
            background: qlineargradient(
                x1: 0, y1: 0, x2: 1, y2: 1,
                stop: 0 rgba(255, 151, 0, 0.95),
                stop: 1 rgba(255, 111, 0, 0.95)
            );
            border-color: rgba(141, 66, 0, 0.18);
            color: #fff7ef;
        }

        QPushButton#primaryButton,
        QPushButton#secondaryButton {
            border-radius: 18px;
            font-size: 14px;
            font-weight: 700;
            padding: 12px 16px;
        }

        QPushButton#primaryButton {
            background: #ff8a00;
            border: 1px solid #ff8a00;
            color: #fff8f1;
        }

        QPushButton#secondaryButton {
            background: rgba(255, 255, 255, 0.74);
            border: 1px solid rgba(62, 43, 22, 0.10);
            color: #1d1715;
        }

        QLineEdit#settingsInput {
            background: rgba(255, 255, 255, 0.88);
            border: 1px solid rgba(62, 43, 22, 0.14);
            border-radius: 16px;
            color: #1d1715;
            font-size: 14px;
            padding: 12px 14px;
        }
    )"));

    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(24, 24, 24, 24);
    rootLayout->setSpacing(24);

    auto *navPanel = new QWidget;
    navPanel->setObjectName(QStringLiteral("navPanel"));
    navPanel->setFixedWidth(280);
    auto *navLayout = new QVBoxLayout(navPanel);
    navLayout->setContentsMargins(0, 0, 0, 0);
    navLayout->setSpacing(12);

    auto *brandCard = new QFrame;
    brandCard->setObjectName(QStringLiteral("card"));
    auto *brandLayout = new QVBoxLayout(brandCard);
    brandLayout->setContentsMargins(22, 22, 22, 22);
    brandLayout->setSpacing(8);
    auto *brandTitle = new QLabel(QStringLiteral("QuizForge"));
    brandTitle->setObjectName(QStringLiteral("appTitle"));
    brandLayout->addWidget(brandTitle);
    brandLayout->addWidget(makeLabel(
        QStringLiteral("Cross-platform desktop shell for the real quiz app. "
                       "Built for Windows and macOS from one C++ codebase."),
        QStringLiteral("bodyCopy")
    ));
    navLayout->addWidget(brandCard);

    navLayout->addWidget(createNavButton(QStringLiteral("Dashboard"), DashboardPage));
    navLayout->addWidget(createNavButton(QStringLiteral("Quiz"), QuizPage));
    navLayout->addWidget(createNavButton(QStringLiteral("Profile"), ProfilePage));
    navLayout->addWidget(createNavButton(QStringLiteral("Leaderboards"), LeaderboardsPage));
    navLayout->addWidget(createNavButton(QStringLiteral("Battle Pass"), BattlePassPage));
    navLayout->addWidget(createNavButton(QStringLiteral("Settings"), SettingsPage));
    navLayout->addStretch(1);

    auto *navFooter = new QFrame;
    navFooter->setObjectName(QStringLiteral("card"));
    auto *navFooterLayout = new QVBoxLayout(navFooter);
    navFooterLayout->setContentsMargins(20, 20, 20, 20);
    navFooterLayout->setSpacing(6);
    navFooterLayout->addWidget(makeLabel(
        QStringLiteral("Starter status"),
        QStringLiteral("eyebrow"),
        false
    ));
    navFooterLayout->addWidget(makeLabel(
        QStringLiteral("Qt 6 + CMake scaffold ready"),
        QStringLiteral("sectionTitle")
    ));
    navFooterLayout->addWidget(makeLabel(
        QStringLiteral("Next step is replacing the placeholder pages with the "
                       "real quiz loop, auth flow, and synced progression."),
        QStringLiteral("bodyCopy")
    ));
    navLayout->addWidget(navFooter);

    auto *contentPanel = new QWidget;
    contentPanel->setObjectName(QStringLiteral("contentPanel"));
    auto *contentLayout = new QVBoxLayout(contentPanel);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(18);

    auto *headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(12);

    m_pageTitleLabel = new QLabel(QStringLiteral("Dashboard"));
    m_pageTitleLabel->setObjectName(QStringLiteral("pageTitle"));
    headerLayout->addWidget(m_pageTitleLabel, 1);

    m_statusLabel = new QLabel;
    m_statusLabel->setObjectName(QStringLiteral("chip"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(m_statusLabel, 0, Qt::AlignTop);

    contentLayout->addLayout(headerLayout);

    m_pageStack = new QStackedWidget;
    m_pageStack->addWidget(createDashboardPage());
    m_pageStack->addWidget(createQuizPage());
    m_pageStack->addWidget(createProfilePage());
    m_pageStack->addWidget(createLeaderboardsPage());
    m_pageStack->addWidget(createBattlePassPage());
    m_pageStack->addWidget(createSettingsPage());
    contentLayout->addWidget(m_pageStack, 1);

    rootLayout->addWidget(navPanel);
    rootLayout->addWidget(contentPanel, 1);

    statusBar()->showMessage(QStringLiteral("Desktop scaffold ready."));

    setCurrentPage(DashboardPage);
    updateApiStatus();
}

QWidget *MainWindow::createDashboardPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    layout->addWidget(makeSectionHeader(
        QStringLiteral("Product loop"),
        QStringLiteral("This shell already maps to your website and progression system."),
        QStringLiteral("Use the dashboard as the command center for player identity, "
                       "progress, and launch milestones while the quiz loop is still being built.")
    ));

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(16);
    grid->addWidget(createCard(
        QStringLiteral("Current target"),
        QStringLiteral("Playable desktop MVP"),
        QStringLiteral("Main menu, category select, answer flow, score screen, and local polish.")
    ), 0, 0);
    grid->addWidget(createCard(
        QStringLiteral("Connected web"),
        QStringLiteral("Accounts already have a backend home"),
        QStringLiteral("The website stack is ready for login, score upload, leaderboards, and battle pass sync.")
    ), 0, 1);
    grid->addWidget(createCard(
        QStringLiteral("Next milestone"),
        QStringLiteral("Replace placeholders with the quiz loop"),
        QStringLiteral("Focus on one excellent mode first, then attach the online profile and progression systems.")
    ), 1, 0);
    grid->addWidget(createCard(
        QStringLiteral("Starter data"),
        QStringLiteral("Sample questions file included"),
        QStringLiteral("A small JSON question bank is in data/sample_questions.json so the game has a clean seed format.")
    ), 1, 1);
    layout->addLayout(grid);

    return wrapInScrollArea(page);
}

QWidget *MainWindow::createQuizPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    layout->addWidget(makeSectionHeader(
        QStringLiteral("Core gameplay"),
        QStringLiteral("Build one polished mode before you chase feature count."),
        QStringLiteral("The best first implementation here is Classic mode: category, difficulty, timer, answer feedback, and results.")
    ));

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(16);
    grid->addWidget(createCard(
        QStringLiteral("Mode 01"),
        QStringLiteral("Classic"),
        QStringLiteral("The baseline quiz loop. This should ship first and feel excellent before anything else.")
    ), 0, 0);
    grid->addWidget(createCard(
        QStringLiteral("Mode 02"),
        QStringLiteral("Time Attack"),
        QStringLiteral("A high-tempo variant that rewards quick recall once the standard flow is stable.")
    ), 0, 1);
    grid->addWidget(createCard(
        QStringLiteral("Mode 03"),
        QStringLiteral("Daily Challenge"),
        QStringLiteral("Perfect for retention once account sync and daily rewards are connected.")
    ), 1, 0);
    grid->addWidget(createCard(
        QStringLiteral("Question format"),
        QStringLiteral("Tagged JSON bank"),
        QStringLiteral("Category, difficulty, correct index, and explanation are already represented in the starter data format.")
    ), 1, 1);
    layout->addLayout(grid);

    return wrapInScrollArea(page);
}

QWidget *MainWindow::createProfilePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    layout->addWidget(makeSectionHeader(
        QStringLiteral("Identity and sync"),
        QStringLiteral("One player account should work in both the desktop app and the website."),
        QStringLiteral("This page is where login state, player level, total XP, and recent synced stats should eventually live.")
    ));

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(16);
    grid->addWidget(createCard(
        QStringLiteral("Auth"),
        QStringLiteral("Email and password"),
        QStringLiteral("The network layer already includes login and signup calls for the website API endpoints.")
    ), 0, 0);
    grid->addWidget(createCard(
        QStringLiteral("Sync"),
        QStringLiteral("Pull stats on app launch"),
        QStringLiteral("Fetch profile data after sign-in so level, XP, and recent activity stay consistent across devices.")
    ), 0, 1);
    grid->addWidget(createCard(
        QStringLiteral("Storage"),
        QStringLiteral("Local session persistence"),
        QStringLiteral("Add secure token storage so the player does not need to sign in every launch.")
    ), 1, 0);
    grid->addWidget(createCard(
        QStringLiteral("Later"),
        QStringLiteral("Profile customization"),
        QStringLiteral("Display name, avatar, title, and unlocked cosmetics can all live here once the core loop is ready.")
    ), 1, 1);
    layout->addLayout(grid);

    return wrapInScrollArea(page);
}

QWidget *MainWindow::createLeaderboardsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    layout->addWidget(makeSectionHeader(
        QStringLiteral("Competition"),
        QStringLiteral("Leaderboards should make every quiz run feel visible."),
        QStringLiteral("Start with a global ladder, then add weekly resets, category filters, and friend challenges.")
    ));

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(16);
    grid->addWidget(createCard(
        QStringLiteral("Global"),
        QStringLiteral("Top performers"),
        QStringLiteral("Ideal first leaderboard screen: player, score, category, and trend.")
    ), 0, 0);
    grid->addWidget(createCard(
        QStringLiteral("Weekly"),
        QStringLiteral("Fresh competition"),
        QStringLiteral("Weekly resets give players a reason to jump back in even if they cannot catch the all-time leaders.")
    ), 0, 1);
    grid->addWidget(createCard(
        QStringLiteral("Category"),
        QStringLiteral("Specialist ladders"),
        QStringLiteral("A strong player identity emerges when Science, History, Movies, and other categories have their own rankings.")
    ), 1, 0);
    grid->addWidget(createCard(
        QStringLiteral("API"),
        QStringLiteral("Website endpoint already planned"),
        QStringLiteral("The desktop app can fetch leaderboard data from GET /api/leaderboard once the real UI is built.")
    ), 1, 1);
    layout->addLayout(grid);

    return wrapInScrollArea(page);
}

QWidget *MainWindow::createBattlePassPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    layout->addWidget(makeSectionHeader(
        QStringLiteral("Retention loop"),
        QStringLiteral("Battle pass progression should reward repeat play, not just one big score."),
        QStringLiteral("Tie XP to completed quizzes, accuracy, difficulty, and streak bonuses so every session moves the player forward.")
    ));

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(16);
    grid->addWidget(createCard(
        QStringLiteral("Tier pacing"),
        QStringLiteral("Simple first season"),
        QStringLiteral("Keep the first reward track cosmetic so balance changes stay easy while the game loop matures.")
    ), 0, 0);
    grid->addWidget(createCard(
        QStringLiteral("Rewards"),
        QStringLiteral("Badges, titles, themes"),
        QStringLiteral("These are lower-risk launch rewards than hard gameplay unlocks and still make progression feel meaningful.")
    ), 0, 1);
    grid->addWidget(createCard(
        QStringLiteral("XP input"),
        QStringLiteral("Website and app can share one formula"),
        QStringLiteral("Use the same scoring and XP rules everywhere so the battle pass stays trustworthy.")
    ), 1, 0);
    grid->addWidget(createCard(
        QStringLiteral("Later"),
        QStringLiteral("Season recap and claim flow"),
        QStringLiteral("Once syncing works, add reward claiming, season countdowns, and a clearer unlock timeline.")
    ), 1, 1);
    layout->addLayout(grid);

    return wrapInScrollArea(page);
}

QWidget *MainWindow::createSettingsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    layout->addWidget(makeSectionHeader(
        QStringLiteral("Local setup"),
        QStringLiteral("Point the desktop app at the website while you build."),
        QStringLiteral("By default this starter targets the local Next.js site on port 3000, but you can change it here without recompiling.")
    ));

    auto *settingsCard = qobject_cast<QFrame *>(createCard(
        QStringLiteral("API base URL"),
        QStringLiteral("Choose the website backend target"),
        QStringLiteral("This value is stored locally with QSettings and used by the starter ApiClient.")
    ));

    auto *settingsLayout = qobject_cast<QVBoxLayout *>(settingsCard->layout());

    m_apiBaseUrlEdit = new QLineEdit(AppConfig::loadApiBaseUrl().toString());
    m_apiBaseUrlEdit->setObjectName(QStringLiteral("settingsInput"));
    settingsLayout->addWidget(m_apiBaseUrlEdit);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setContentsMargins(0, 8, 0, 0);
    buttonRow->setSpacing(12);

    auto *saveButton = new QPushButton(QStringLiteral("Save API URL"));
    saveButton->setObjectName(QStringLiteral("primaryButton"));
    connect(saveButton, &QPushButton::clicked, this, [this] {
        saveApiBaseUrl();
    });

    auto *resetButton = new QPushButton(QStringLiteral("Reset to localhost"));
    resetButton->setObjectName(QStringLiteral("secondaryButton"));
    connect(resetButton, &QPushButton::clicked, this, [this] {
        m_apiBaseUrlEdit->setText(AppConfig::defaultApiBaseUrl().toString());
        saveApiBaseUrl();
    });

    buttonRow->addWidget(saveButton);
    buttonRow->addWidget(resetButton);
    buttonRow->addStretch(1);
    settingsLayout->addLayout(buttonRow);

    layout->addWidget(settingsCard);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(16);
    grid->addWidget(createCard(
        QStringLiteral("Auth endpoints"),
        QStringLiteral("Login and signup ready"),
        QStringLiteral("The starter network client already targets /api/auth/login and /api/auth/signup.")
    ), 0, 0);
    grid->addWidget(createCard(
        QStringLiteral("Game sync"),
        QStringLiteral("Upload result endpoint mapped"),
        QStringLiteral("Once the quiz loop exists, the desktop app can post to /api/game/upload-result.")
    ), 0, 1);
    grid->addWidget(createCard(
        QStringLiteral("Stats"),
        QStringLiteral("Profile sync endpoint mapped"),
        QStringLiteral("Use GET /api/me/stats after sign-in to refresh the player dashboard.")
    ), 1, 0);
    grid->addWidget(createCard(
        QStringLiteral("Leaderboards"),
        QStringLiteral("Competition endpoint mapped"),
        QStringLiteral("Use GET /api/leaderboard to power the global and weekly ladders.")
    ), 1, 1);
    layout->addLayout(grid);

    return wrapInScrollArea(page);
}

QWidget *MainWindow::wrapInScrollArea(QWidget *content) const
{
    auto *scrollArea = new QScrollArea;
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setWidget(content);
    return scrollArea;
}

QWidget *MainWindow::createCard(
    const QString &eyebrow,
    const QString &title,
    const QString &body
) const
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("card"));

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(8);
    layout->addWidget(makeLabel(eyebrow, QStringLiteral("eyebrow"), false));
    layout->addWidget(makeLabel(title, QStringLiteral("sectionTitle")));
    layout->addWidget(makeLabel(body, QStringLiteral("bodyCopy")));
    layout->addStretch(1);

    return card;
}

QPushButton *MainWindow::createNavButton(const QString &text, int index)
{
    auto *button = new QPushButton(text);
    button->setCheckable(true);
    button->setObjectName(QStringLiteral("navButton"));
    connect(button, &QPushButton::clicked, this, [this, index, button] {
        Q_UNUSED(button);
        setCurrentPage(index);
    });
    return button;
}

void MainWindow::setCurrentPage(int index)
{
    m_pageStack->setCurrentIndex(index);

    const auto buttons = findChildren<QPushButton *>(QStringLiteral("navButton"));
    for (auto *button : buttons) {
        const bool isCurrent = (
            (index == DashboardPage && button->text() == QStringLiteral("Dashboard")) ||
            (index == QuizPage && button->text() == QStringLiteral("Quiz")) ||
            (index == ProfilePage && button->text() == QStringLiteral("Profile")) ||
            (index == LeaderboardsPage && button->text() == QStringLiteral("Leaderboards")) ||
            (index == BattlePassPage && button->text() == QStringLiteral("Battle Pass")) ||
            (index == SettingsPage && button->text() == QStringLiteral("Settings"))
        );
        button->setChecked(isCurrent);
    }

    switch (index) {
    case DashboardPage:
        m_pageTitleLabel->setText(QStringLiteral("Dashboard"));
        break;
    case QuizPage:
        m_pageTitleLabel->setText(QStringLiteral("Quiz"));
        break;
    case ProfilePage:
        m_pageTitleLabel->setText(QStringLiteral("Profile"));
        break;
    case LeaderboardsPage:
        m_pageTitleLabel->setText(QStringLiteral("Leaderboards"));
        break;
    case BattlePassPage:
        m_pageTitleLabel->setText(QStringLiteral("Battle Pass"));
        break;
    case SettingsPage:
        m_pageTitleLabel->setText(QStringLiteral("Settings"));
        break;
    }
}

void MainWindow::saveApiBaseUrl()
{
    const QUrl url = QUrl::fromUserInput(m_apiBaseUrlEdit->text().trimmed());

    if (!url.isValid() || url.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("Invalid URL"),
            QStringLiteral("Enter a valid base URL such as http://localhost:3000.")
        );
        return;
    }

    AppConfig::saveApiBaseUrl(url);

    if (m_apiClient != nullptr) {
        m_apiClient->setBaseUrl(url);
    }

    updateApiStatus();
    statusBar()->showMessage(QStringLiteral("API base URL saved."), 3000);
}

void MainWindow::updateApiStatus()
{
    const QUrl url = (m_apiClient != nullptr) ? m_apiClient->baseUrl() : AppConfig::loadApiBaseUrl();
    m_statusLabel->setText(QStringLiteral("API: %1").arg(url.toString()));
}

