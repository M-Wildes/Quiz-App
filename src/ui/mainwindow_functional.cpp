#include "mainwindow.h"

#include <algorithm>
#include <cmath>
#include <random>

#include <QAbstractItemView>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QSizePolicy>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QNetworkReply>
#include <QPointer>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include "network/apiclient.h"
#include "utils/appconfig.h"
#include "utils/questionbank.h"

namespace {

struct BattlePassReward
{
    int tier = 1;
    int xpRequired = 200;
    QString rewardName;
    QString rarity;
};

QString formatNumber(int value)
{
    return QLocale::system().toString(value);
}

QString formatDuration(int durationMs)
{
    const int totalSeconds = std::max(durationMs / 1000, 0);
    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;

    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

QString prettifyToken(const QString &value)
{
    const QString trimmed = value.trimmed().toLower();
    if (trimmed.isEmpty()) {
        return QStringLiteral("Unknown");
    }

    QString pretty = trimmed;
    pretty[0] = pretty[0].toUpper();
    return pretty;
}

QString formatIsoDateTime(const QString &value)
{
    if (value.trimmed().isEmpty()) {
        return QStringLiteral("Recently");
    }

    QDateTime timestamp = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!timestamp.isValid()) {
        timestamp = QDateTime::fromString(value, Qt::ISODate);
    }

    if (!timestamp.isValid()) {
        return value;
    }

    return timestamp.toLocalTime().toString(QStringLiteral("dd MMM HH:mm"));
}

bool isLocalUrl(const QUrl &url)
{
    const QString host = url.host().trimmed().toLower();
    return host == QStringLiteral("localhost") ||
           host == QStringLiteral("127.0.0.1");
}

int valueToInt(const QJsonValue &value, int fallback = 0)
{
    if (value.isDouble()) {
        return value.toInt(fallback);
    }

    if (value.isString()) {
        bool ok = false;
        const int parsed = value.toString().toInt(&ok);
        return ok ? parsed : fallback;
    }

    return fallback;
}

bool valueToBool(const QJsonValue &value, bool fallback = false)
{
    if (value.isBool()) {
        return value.toBool();
    }

    if (value.isString()) {
        return value.toString().compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
    }

    return fallback;
}

QString extractReplyError(QNetworkReply *reply, const QByteArray &bytes)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);

    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
        const QJsonObject object = document.object();
        const QString error = object.value(QStringLiteral("error")).toString();
        if (!error.trimmed().isEmpty()) {
            return error;
        }

        const QString message = object.value(QStringLiteral("message")).toString();
        if (!message.trimmed().isEmpty()) {
            return message;
        }
    }

    const QString replyError = reply != nullptr ? reply->errorString() : QString();
    if (!replyError.trimmed().isEmpty()) {
        return replyError;
    }

    return QStringLiteral("The request could not be completed.");
}

int calculateLevelFromXp(int totalXp)
{
    return static_cast<int>(std::floor(std::sqrt(std::max(totalXp, 0) / 180.0))) + 1;
}

double scoreMultiplierForDifficulty(const QString &difficulty)
{
    if (difficulty.compare(QStringLiteral("hard"), Qt::CaseInsensitive) == 0) {
        return 1.5;
    }

    if (difficulty.compare(QStringLiteral("medium"), Qt::CaseInsensitive) == 0) {
        return 1.25;
    }

    return 1.0;
}

int xpBonusForDifficulty(const QString &difficulty)
{
    if (difficulty.compare(QStringLiteral("hard"), Qt::CaseInsensitive) == 0) {
        return 45;
    }

    if (difficulty.compare(QStringLiteral("medium"), Qt::CaseInsensitive) == 0) {
        return 20;
    }

    return 0;
}

int calculateAccuracyPercent(int correctAnswers, int totalQuestions)
{
    if (totalQuestions <= 0) {
        return 0;
    }

    return static_cast<int>(std::round((static_cast<double>(correctAnswers) / totalQuestions) * 100.0));
}

int calculateScore(const QString &difficulty, int correctAnswers, int totalQuestions)
{
    const int questionCount = std::clamp(totalQuestions, 1, 100);
    const int clampedCorrect = std::clamp(correctAnswers, 0, questionCount);
    const double accuracyRatio = static_cast<double>(clampedCorrect) / questionCount;

    // Score rewards both raw correct answers and overall accuracy so short and long runs scale consistently.
    return static_cast<int>(std::round(
        clampedCorrect * 100.0 * scoreMultiplierForDifficulty(difficulty) +
        accuracyRatio * 250.0
    ));
}

int calculateXp(
    const QString &difficulty,
    int correctAnswers,
    int totalQuestions,
    int durationMs,
    bool streakBonus
)
{
    const int questionCount = std::clamp(totalQuestions, 1, 100);
    const int clampedCorrect = std::clamp(correctAnswers, 0, questionCount);
    const double accuracyRatio = static_cast<double>(clampedCorrect) / questionCount;

    // XP is intentionally softer than score: everyone progresses, but stronger runs still move further.
    int xpEarned = 50 +
                   static_cast<int>(std::round(accuracyRatio * 60.0)) +
                   xpBonusForDifficulty(difficulty);

    if (accuracyRatio >= 0.9) {
        xpEarned += 20;
    } else if (accuracyRatio >= 0.8) {
        xpEarned += 10;
    }

    if (durationMs > 0 && durationMs < questionCount * 4500) {
        xpEarned += 15;
    }

    if (streakBonus) {
        xpEarned += 10;
    }

    return xpEarned;
}

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

QWidget *makeMetricCard(
    const QString &label,
    QLabel **valueLabel,
    QLabel **hintLabel = nullptr
)
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("card"));

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(8);
    layout->addWidget(makeLabel(label, QStringLiteral("eyebrow"), false));

    auto *value = new QLabel(QStringLiteral("--"));
    value->setObjectName(QStringLiteral("metricValue"));
    value->setWordWrap(false);
    layout->addWidget(value);

    auto *hint = makeLabel(QString(), QStringLiteral("bodyCopy"));
    layout->addWidget(hint);
    layout->addStretch(1);

    if (valueLabel != nullptr) {
        *valueLabel = value;
    }

    if (hintLabel != nullptr) {
        *hintLabel = hint;
    } else {
        hint->hide();
    }

    return card;
}

QVector<BattlePassReward> rewardTrack()
{
    return {
        {1, 200, QStringLiteral("Starter profile badge"), QStringLiteral("free")},
        {2, 400, QStringLiteral("Retro avatar frame"), QStringLiteral("premium")},
        {3, 600, QStringLiteral("Quiz title unlock"), QStringLiteral("free")},
        {4, 800, QStringLiteral("Arcade neon avatar frame"), QStringLiteral("premium")},
        {5, 1000, QStringLiteral("Season winner banner"), QStringLiteral("legendary")},
        {6, 1200, QStringLiteral("Bonus question pack"), QStringLiteral("premium")},
        {7, 1400, QStringLiteral("Profile flair set"), QStringLiteral("free")},
        {8, 1600, QStringLiteral("Night shift theme pack"), QStringLiteral("legendary")},
    };
}

} // namespace

// Main window startup and shared styling
MainWindow::MainWindow(ApiClient *apiClient, QWidget *parent)
    : QMainWindow(parent)
    , m_apiClient(apiClient)
{
    m_playerProfile.displayName = QStringLiteral("Local Challenger");
    m_playerProfile.level = 1;
    m_playerProfile.nextBattlePassXp = 200;

    setWindowTitle(QStringLiteral("QuizForge Desktop"));
    resize(1360, 860);
    setMinimumSize(1120, 760);

    m_quizUiTimer = new QTimer(this);
    m_quizUiTimer->setInterval(250);
    connect(m_quizUiTimer, &QTimer::timeout, this, [this] {
        updateQuizTimer();
    });

    setStyleSheet(QStringLiteral(R"(
        QMainWindow {
            background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 1,
                                        stop: 0 #080a12,
                                        stop: 0.55 #101322,
                                        stop: 1 #080a12);
            color: #f6f1e9;
        }
        QWidget#navPanel {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                        stop: 0 rgba(16, 19, 31, 0.94),
                                        stop: 1 rgba(18, 22, 36, 0.92));
            border-radius: 28px;
            border: 1px solid rgba(255, 255, 255, 0.10);
        }
        QWidget#contentPanel { background: transparent; }
        QLabel#appTitle { color: #f6f1e9; font-size: 30px; font-weight: 700; }
        QLabel#pageTitle { color: #f6f1e9; font-size: 34px; font-weight: 700; }
        QLabel#eyebrow {
            color: #b7ad9f;
            font-size: 11px;
            font-weight: 700;
            letter-spacing: 2px;
            text-transform: uppercase;
        }
        QLabel#sectionTitle { color: #f6f1e9; font-size: 26px; font-weight: 700; }
        QLabel#bodyCopy { color: #b7ad9f; font-size: 14px; }
        QLabel#metricValue { color: #ffffff; font-size: 30px; font-weight: 700; }
        QLabel#chip {
            background: rgba(255, 159, 28, 0.16);
            border: 1px solid rgba(255, 159, 28, 0.28);
            border-radius: 16px;
            color: #ffd08a;
            font-size: 12px;
            font-weight: 700;
            padding: 8px 12px;
        }
        QLabel#questionText { color: #fff7eb; font-size: 22px; font-weight: 700; }
        QLabel#feedbackPositive { color: #58d6c8; font-size: 14px; font-weight: 600; }
        QLabel#feedbackNegative { color: #ff8f8f; font-size: 14px; font-weight: 600; }
        QFrame#card {
            background: rgba(16, 19, 31, 0.88);
            border: 1px solid rgba(255, 255, 255, 0.10);
            border-radius: 24px;
        }
        QLineEdit#settingsInput,
        QComboBox,
        QSpinBox {
            background: rgba(255, 255, 255, 0.06);
            border: 1px solid rgba(255, 255, 255, 0.13);
            border-radius: 16px;
            color: #f6f1e9;
            font-size: 14px;
            min-height: 20px;
            padding: 10px 12px;
        }
        QLineEdit#settingsInput:focus,
        QComboBox:focus,
        QSpinBox:focus {
            border: 1px solid #ff9f1c;
        }
        QListWidget,
        QTableWidget {
            background: rgba(16, 19, 31, 0.92);
            border: 1px solid rgba(255, 255, 255, 0.10);
            border-radius: 18px;
            color: #f6f1e9;
            font-size: 13px;
            padding: 6px;
            gridline-color: rgba(255, 255, 255, 0.10);
        }
        QListWidget::item {
            border-radius: 12px;
            margin: 2px 0;
            padding: 8px;
        }
        QListWidget::item:selected {
            background: rgba(255, 159, 28, 0.16);
            color: #ffffff;
        }
        QTableWidget::item { padding: 8px; }
        QHeaderView::section {
            background: rgba(255, 255, 255, 0.055);
            border: none;
            border-bottom: 1px solid rgba(255, 255, 255, 0.10);
            color: #b7ad9f;
            font-size: 12px;
            font-weight: 700;
            padding: 10px 8px;
        }
        QProgressBar {
            background: rgba(255, 255, 255, 0.09);
            border: 1px solid rgba(255, 255, 255, 0.10);
            border-radius: 12px;
            color: #f6f1e9;
            min-height: 18px;
            text-align: center;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0,
                                        stop: 0 #ff9f1c,
                                        stop: 0.55 #58d6c8,
                                        stop: 1 #2340cf);
            border-radius: 10px;
        }
        QPushButton#navButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 18px;
            color: #b7ad9f;
            font-size: 15px;
            font-weight: 600;
            padding: 14px 18px;
            text-align: left;
        }
        QPushButton#navButton:hover {
            background: rgba(255, 255, 255, 0.07);
            border-color: rgba(255, 255, 255, 0.10);
            color: #ffffff;
        }
        QPushButton#navButton:checked {
            background: rgba(255, 159, 28, 0.16);
            border-color: rgba(255, 159, 28, 0.28);
            color: #ffd08a;
        }
        QPushButton#primaryButton,
        QPushButton#secondaryButton,
        QPushButton#answerButton {
            border-radius: 18px;
            font-size: 14px;
            font-weight: 700;
            padding: 12px 16px;
        }
        QPushButton#primaryButton {
            background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 1,
                                        stop: 0 #ff9700,
                                        stop: 1 #ff6f00);
            border: 1px solid rgba(255, 159, 28, 0.35);
            color: #fff7eb;
        }
        QPushButton#primaryButton:hover {
            background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 1,
                                        stop: 0 #ffad38,
                                        stop: 1 #ff820f);
            border-color: rgba(255, 208, 138, 0.45);
        }
        QPushButton#secondaryButton {
            background: rgba(255, 255, 255, 0.07);
            border: 1px solid rgba(255, 255, 255, 0.10);
            color: #f6f1e9;
        }
        QPushButton#secondaryButton:hover {
            background: rgba(255, 255, 255, 0.10);
            border-color: rgba(255, 255, 255, 0.16);
        }
        QPushButton#answerButton {
            background: rgba(255, 255, 255, 0.055);
            border: 1px solid rgba(255, 255, 255, 0.10);
            color: #f6f1e9;
            text-align: left;
        }
        QPushButton#answerButton:hover {
            background: rgba(255, 255, 255, 0.08);
            border-color: rgba(255, 159, 28, 0.24);
        }
        QPushButton#ghostButton {
            background: transparent;
            border: 1px solid rgba(121, 166, 214, 0.16);
            border-radius: 18px;
            color: #9fb1c7;
            font-size: 14px;
            font-weight: 700;
            padding: 12px 16px;
        }
        QPushButton#ghostButton:hover {
            background: rgba(97, 188, 255, 0.08);
            border-color: rgba(97, 188, 255, 0.24);
            color: #edf3fb;
        }
        QScrollBar:vertical {
            background: rgba(255, 255, 255, 0.04);
            width: 12px;
            margin: 4px 0 4px 0;
            border-radius: 6px;
        }
        QScrollBar::handle:vertical {
            background: rgba(255, 159, 28, 0.28);
            min-height: 28px;
            border-radius: 6px;
        }
        QScrollBar::handle:vertical:hover {
            background: rgba(255, 159, 28, 0.42);
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical,
        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            background: transparent;
            height: 0px;
        }
    )"));

    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(24, 24, 24, 24);
    rootLayout->setSpacing(0);

    // The root stack keeps the first-launch welcome/auth flow separate from the main in-app shell.
    m_rootStack = new QStackedWidget;
    rootLayout->addWidget(m_rootStack, 1);

    m_rootStack->addWidget(createLandingPage());

    auto *shellPage = new QWidget;
    m_mainShellPage = shellPage;
    m_rootStack->addWidget(shellPage);

    auto *shellLayout = new QHBoxLayout(shellPage);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(24);

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
        QStringLiteral("Play solo, chase the board, and keep your progress moving everywhere you sign in."),
        QStringLiteral("bodyCopy")
    ));
    navLayout->addWidget(brandCard);

    navLayout->addWidget(createNavButton(QStringLiteral("Dashboard"), DashboardPage));
    navLayout->addWidget(createNavButton(QStringLiteral("Quiz"), QuizPage));
    navLayout->addWidget(createNavButton(QStringLiteral("Community"), CommunityPage));
    navLayout->addWidget(createNavButton(QStringLiteral("Profile"), ProfilePage));
    navLayout->addWidget(createNavButton(QStringLiteral("Leaderboards"), LeaderboardsPage));
    navLayout->addWidget(createNavButton(QStringLiteral("Battle Pass"), BattlePassPage));
    navLayout->addStretch(1);

    auto *navFooter = new QFrame;
    navFooter->setObjectName(QStringLiteral("card"));
    auto *navFooterLayout = new QVBoxLayout(navFooter);
    navFooterLayout->setContentsMargins(20, 20, 20, 20);
    navFooterLayout->setSpacing(6);
    navFooterLayout->addWidget(makeLabel(QStringLiteral("Season focus"), QStringLiteral("eyebrow"), false));
    navFooterLayout->addWidget(makeLabel(QStringLiteral("Shared quizzes are ready to discover"), QStringLiteral("sectionTitle")));
    navFooterLayout->addWidget(makeLabel(
        QStringLiteral("Jump into classic mode, then visit Community to pull in quizzes that were made and shared on the website."),
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
    m_pageStack->addWidget(createCommunityPage());
    m_pageStack->addWidget(createProfilePage());
    m_pageStack->addWidget(createLeaderboardsPage());
    m_pageStack->addWidget(createBattlePassPage());
    m_pageStack->addWidget(createSettingsPage());
    contentLayout->addWidget(m_pageStack, 1);

    shellLayout->addWidget(navPanel);
    shellLayout->addWidget(contentPanel, 1);

    loadQuestions();
    populateQuestionFilters();
    updateQuestionPoolStatus();
    updateCommunityQuizUi();
    updateDashboardUi();
    updateProfileUi();
    updateBattlePassUi();
    updateLeaderboardStatus(QStringLiteral("Refresh to load leaderboard data."));

    setCurrentPage(DashboardPage);
    showLandingPage();
    updateApiStatus();
    refreshCommunityQuizzes();
    statusBar()->showMessage(QStringLiteral("Choose how you want to start."), 3000);
}

// Page construction
QWidget *MainWindow::createLandingPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(48, 32, 48, 32);
    layout->setSpacing(0);

    layout->addStretch(1);

    auto *heroCard = new QFrame;
    heroCard->setObjectName(QStringLiteral("card"));
    heroCard->setMaximumWidth(640);
    auto *heroLayout = new QVBoxLayout(heroCard);
    heroLayout->setContentsMargins(30, 28, 30, 28);
    heroLayout->setSpacing(14);

    heroLayout->addWidget(makeLabel(QStringLiteral("Desktop launch"), QStringLiteral("eyebrow"), false));

    auto *title = new QLabel(QStringLiteral("Welcome to QuizForge"));
    title->setObjectName(QStringLiteral("sectionTitle"));
    title->setWordWrap(true);
    heroLayout->addWidget(title);

    heroLayout->addWidget(makeLabel(
        QStringLiteral("Log in to sync your profile, create a new account on the QuizForge website, or use guest access to play without an account."),
        QStringLiteral("bodyCopy")
    ));

    m_emailEdit = new QLineEdit;
    m_emailEdit->setPlaceholderText(QStringLiteral("Email"));
    m_emailEdit->setFrame(true);
    m_emailEdit->setFixedHeight(48);
    m_emailEdit->setMinimumWidth(420);
    m_emailEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_emailEdit->setTextMargins(14, 0, 14, 0);
    m_emailEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #182230; border: 1px solid #38506c; border-radius: 8px; color: #ffffff; font-size: 14px; padding: 0px; }"
        "QLineEdit:focus { border: 2px solid #61bcff; }"
    ));

    m_passwordEdit = new QLineEdit;
    m_passwordEdit->setPlaceholderText(QStringLiteral("Password"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setFrame(true);
    m_passwordEdit->setFixedHeight(48);
    m_passwordEdit->setMinimumWidth(420);
    m_passwordEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_passwordEdit->setTextMargins(14, 0, 14, 0);
    m_passwordEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #182230; border: 1px solid #38506c; border-radius: 8px; color: #ffffff; font-size: 14px; padding: 0px; }"
        "QLineEdit:focus { border: 2px solid #61bcff; }"
    ));

    auto *formLayout = new QVBoxLayout;
    formLayout->setContentsMargins(0, 8, 0, 0);
    formLayout->setSpacing(0);

    auto *emailLabel = makeLabel(QStringLiteral("Email"), QStringLiteral("bodyCopy"), false);
    emailLabel->setFixedHeight(22);
    auto *passwordLabel = makeLabel(QStringLiteral("Password"), QStringLiteral("bodyCopy"), false);
    passwordLabel->setFixedHeight(22);

    formLayout->addWidget(emailLabel);
    formLayout->addSpacing(6);
    formLayout->addWidget(m_emailEdit);
    formLayout->addSpacing(18);
    formLayout->addWidget(passwordLabel);
    formLayout->addSpacing(6);
    formLayout->addWidget(m_passwordEdit);
    heroLayout->addLayout(formLayout);
    heroLayout->addSpacing(16);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setContentsMargins(0, 8, 0, 0);
    buttonRow->setSpacing(10);

    auto *loginButton = new QPushButton(QStringLiteral("Login"));
    loginButton->setObjectName(QStringLiteral("primaryButton"));
    loginButton->setMinimumWidth(90);
    loginButton->setFixedHeight(42);
    connect(loginButton, &QPushButton::clicked, this, [this] { handleLogin(); });

    auto *signupButton = new QPushButton(QStringLiteral("Create Account"));
    signupButton->setObjectName(QStringLiteral("secondaryButton"));
    signupButton->setMinimumWidth(150);
    signupButton->setFixedHeight(42);
    connect(signupButton, &QPushButton::clicked, this, [this] { handleSignup(); });

    auto *guestButton = new QPushButton(QStringLiteral("Guest Access"));
    guestButton->setObjectName(QStringLiteral("ghostButton"));
    guestButton->setMinimumWidth(140);
    guestButton->setFixedHeight(42);
    connect(guestButton, &QPushButton::clicked, this, [this] { continueAsGuest(); });

    buttonRow->addWidget(loginButton);
    buttonRow->addWidget(signupButton);
    buttonRow->addWidget(guestButton);
    buttonRow->addStretch(1);
    heroLayout->addLayout(buttonRow);

    auto *unlockHint = makeLabel(
        QStringLiteral("Menus unlock after login or guest access. Account creation opens the QuizForge website signup page."),
        QStringLiteral("bodyCopy")
    );
    heroLayout->addWidget(unlockHint);

    layout->addWidget(heroCard, 0, Qt::AlignHCenter);
    layout->addStretch(1);

    return page;
}

QWidget *MainWindow::createDashboardPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    layout->addWidget(makeSectionHeader(
        QStringLiteral("Player home"),
        QStringLiteral("See where your progress stands and what to do next."),
        QStringLiteral("Keep the focus on your level, season push, and recent results so every return to the app feels immediately useful.")
    ));

    auto *heroCard = new QFrame;
    heroCard->setObjectName(QStringLiteral("card"));
    auto *heroLayout = new QVBoxLayout(heroCard);
    heroLayout->setContentsMargins(22, 22, 22, 22);
    heroLayout->setSpacing(10);

    m_dashboardWelcomeLabel = new QLabel;
    m_dashboardWelcomeLabel->setObjectName(QStringLiteral("sectionTitle"));
    heroLayout->addWidget(m_dashboardWelcomeLabel);

    m_dashboardSeasonValueLabel = makeLabel(QString(), QStringLiteral("bodyCopy"));
    heroLayout->addWidget(m_dashboardSeasonValueLabel);

    auto *heroButtonRow = new QHBoxLayout;
    heroButtonRow->setContentsMargins(0, 8, 0, 0);
    heroButtonRow->setSpacing(12);

    auto *startQuizButton = new QPushButton(QStringLiteral("Start Quiz"));
    startQuizButton->setObjectName(QStringLiteral("primaryButton"));
    connect(startQuizButton, &QPushButton::clicked, this, [this] { setCurrentPage(QuizPage); });

    auto *refreshStatsButton = new QPushButton(QStringLiteral("Refresh Stats"));
    refreshStatsButton->setObjectName(QStringLiteral("secondaryButton"));
    connect(refreshStatsButton, &QPushButton::clicked, this, [this] { refreshRemoteStats(); });

    auto *refreshLeaderboardButton = new QPushButton(QStringLiteral("Refresh Leaderboard"));
    refreshLeaderboardButton->setObjectName(QStringLiteral("secondaryButton"));
    connect(refreshLeaderboardButton, &QPushButton::clicked, this, [this] { refreshLeaderboard(); });

    heroButtonRow->addWidget(startQuizButton);
    heroButtonRow->addWidget(refreshStatsButton);
    heroButtonRow->addWidget(refreshLeaderboardButton);
    heroButtonRow->addStretch(1);
    heroLayout->addLayout(heroButtonRow);

    layout->addWidget(heroCard);

    auto *metricsLayout = new QGridLayout;
    metricsLayout->setHorizontalSpacing(16);
    metricsLayout->setVerticalSpacing(16);
    metricsLayout->addWidget(makeMetricCard(QStringLiteral("Current level"), &m_dashboardLevelValueLabel), 0, 0);
    metricsLayout->addWidget(makeMetricCard(QStringLiteral("Total XP"), &m_dashboardXpValueLabel), 0, 1);
    metricsLayout->addWidget(makeMetricCard(QStringLiteral("Total score"), &m_dashboardScoreValueLabel), 1, 0);
    metricsLayout->addWidget(makeMetricCard(QStringLiteral("Quizzes completed"), &m_dashboardQuizCountValueLabel), 1, 1);
    layout->addLayout(metricsLayout);

    auto *recentRunsCard = new QFrame;
    recentRunsCard->setObjectName(QStringLiteral("card"));
    auto *recentRunsLayout = new QVBoxLayout(recentRunsCard);
    recentRunsLayout->setContentsMargins(22, 22, 22, 22);
    recentRunsLayout->setSpacing(10);
    recentRunsLayout->addWidget(makeLabel(QStringLiteral("Latest runs"), QStringLiteral("eyebrow"), false));
    recentRunsLayout->addWidget(makeLabel(QStringLiteral("Recent quiz sessions"), QStringLiteral("sectionTitle")));
    recentRunsLayout->addWidget(makeLabel(
        QStringLiteral("Local runs appear instantly. Signed-in sessions can be uploaded and refreshed from the server."),
        QStringLiteral("bodyCopy")
    ));

    m_dashboardRecentRunsList = new QListWidget;
    m_dashboardRecentRunsList->setMinimumHeight(240);
    recentRunsLayout->addWidget(m_dashboardRecentRunsList);
    layout->addWidget(recentRunsCard);

    return wrapInScrollArea(page);
}

QWidget *MainWindow::createQuizPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    layout->addWidget(makeSectionHeader(
        QStringLiteral("Classic mode"),
        QStringLiteral("Jump into a fresh quiz run."),
        QStringLiteral("Pick a category, choose a difficulty, and turn every round into score, XP, season progress, and a synced result when you are signed in.")
    ));

    m_quizStateStack = new QStackedWidget;

    auto *setupPage = new QWidget;
    auto *setupLayout = new QVBoxLayout(setupPage);
    setupLayout->setContentsMargins(0, 0, 0, 0);
    setupLayout->setSpacing(16);

    auto *setupCard = new QFrame;
    setupCard->setObjectName(QStringLiteral("card"));
    auto *setupCardLayout = new QVBoxLayout(setupCard);
    setupCardLayout->setContentsMargins(22, 22, 22, 22);
    setupCardLayout->setSpacing(14);
    setupCardLayout->addWidget(makeLabel(QStringLiteral("Classic quiz"), QStringLiteral("eyebrow"), false));
    setupCardLayout->addWidget(makeLabel(QStringLiteral("Choose how you want to play"), QStringLiteral("sectionTitle")));

    auto *formGrid = new QGridLayout;
    formGrid->setHorizontalSpacing(16);
    formGrid->setVerticalSpacing(12);

    m_categoryCombo = new QComboBox;
    m_difficultyCombo = new QComboBox;
    m_questionCountSpinBox = new QSpinBox;
    m_questionCountSpinBox->setRange(1, 1);

    m_difficultyCombo->addItem(QStringLiteral("Easy"), QStringLiteral("easy"));
    m_difficultyCombo->addItem(QStringLiteral("Medium"), QStringLiteral("medium"));
    m_difficultyCombo->addItem(QStringLiteral("Hard"), QStringLiteral("hard"));
    m_difficultyCombo->setCurrentIndex(1);

    formGrid->addWidget(makeLabel(QStringLiteral("Category"), QStringLiteral("bodyCopy"), false), 0, 0);
    formGrid->addWidget(m_categoryCombo, 1, 0);
    formGrid->addWidget(makeLabel(QStringLiteral("Difficulty"), QStringLiteral("bodyCopy"), false), 0, 1);
    formGrid->addWidget(m_difficultyCombo, 1, 1);
    formGrid->addWidget(makeLabel(QStringLiteral("Question count"), QStringLiteral("bodyCopy"), false), 0, 2);
    formGrid->addWidget(m_questionCountSpinBox, 1, 2);
    setupCardLayout->addLayout(formGrid);

    m_questionPoolStatusLabel = makeLabel(QString(), QStringLiteral("bodyCopy"));
    setupCardLayout->addWidget(m_questionPoolStatusLabel);

    m_startQuizButton = new QPushButton(QStringLiteral("Start Classic Run"));
    m_startQuizButton->setObjectName(QStringLiteral("primaryButton"));
    connect(m_startQuizButton, &QPushButton::clicked, this, [this] { startQuiz(); });
    setupCardLayout->addWidget(m_startQuizButton, 0, Qt::AlignLeft);

    connect(m_categoryCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        updateQuestionPoolStatus();
    });
    connect(m_difficultyCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        updateQuestionPoolStatus();
    });

    setupLayout->addWidget(setupCard);
    setupLayout->addWidget(createCard(
        QStringLiteral("Scoring"),
        QStringLiteral("Fast, accurate runs move you further"),
        QStringLiteral("Difficulty, speed, accuracy, and streak bonus all feed the same reward loop, so strong performances always feel worth chasing.")
    ));
    setupLayout->addStretch(1);

    auto *livePage = new QWidget;
    auto *liveLayout = new QVBoxLayout(livePage);
    liveLayout->setContentsMargins(0, 0, 0, 0);
    liveLayout->setSpacing(16);

    auto *liveCard = new QFrame;
    liveCard->setObjectName(QStringLiteral("card"));
    auto *liveCardLayout = new QVBoxLayout(liveCard);
    liveCardLayout->setContentsMargins(22, 22, 22, 22);
    liveCardLayout->setSpacing(14);

    auto *liveTopRow = new QHBoxLayout;
    liveTopRow->setContentsMargins(0, 0, 0, 0);
    liveTopRow->setSpacing(12);

    m_quizProgressLabel = makeLabel(QStringLiteral("Question 1 of 1"), QStringLiteral("eyebrow"), false);
    m_quizTimerLabel = makeLabel(QStringLiteral("00:00"), QStringLiteral("chip"), false);
    m_quizTimerLabel->setAlignment(Qt::AlignCenter);
    liveTopRow->addWidget(m_quizProgressLabel, 1);
    liveTopRow->addWidget(m_quizTimerLabel, 0, Qt::AlignRight);
    liveCardLayout->addLayout(liveTopRow);

    m_quizMetaLabel = makeLabel(QString(), QStringLiteral("bodyCopy"));
    liveCardLayout->addWidget(m_quizMetaLabel);

    m_quizQuestionLabel = makeLabel(QString(), QStringLiteral("questionText"));
    liveCardLayout->addWidget(m_quizQuestionLabel);

    for (int answerIndex = 0; answerIndex < 4; ++answerIndex) {
        auto *button = new QPushButton;
        button->setObjectName(QStringLiteral("answerButton"));
        connect(button, &QPushButton::clicked, this, [this, answerIndex] {
            handleAnswerSelected(answerIndex);
        });
        liveCardLayout->addWidget(button);
        m_answerButtons.push_back(button);
    }

    m_quizFeedbackLabel = makeLabel(QStringLiteral("Choose the best answer."), QStringLiteral("bodyCopy"));
    liveCardLayout->addWidget(m_quizFeedbackLabel);

    m_nextQuestionButton = new QPushButton(QStringLiteral("Next Question"));
    m_nextQuestionButton->setObjectName(QStringLiteral("primaryButton"));
    m_nextQuestionButton->setEnabled(false);
    connect(m_nextQuestionButton, &QPushButton::clicked, this, [this] { advanceQuiz(); });
    liveCardLayout->addWidget(m_nextQuestionButton, 0, Qt::AlignLeft);

    liveLayout->addWidget(liveCard);
    liveLayout->addStretch(1);

    auto *resultPage = new QWidget;
    auto *resultLayout = new QVBoxLayout(resultPage);
    resultLayout->setContentsMargins(0, 0, 0, 0);
    resultLayout->setSpacing(16);

    auto *resultCard = new QFrame;
    resultCard->setObjectName(QStringLiteral("card"));
    auto *resultCardLayout = new QVBoxLayout(resultCard);
    resultCardLayout->setContentsMargins(22, 22, 22, 22);
    resultCardLayout->setSpacing(12);
    resultCardLayout->addWidget(makeLabel(QStringLiteral("Result"), QStringLiteral("eyebrow"), false));

    m_quizResultHeadlineLabel = new QLabel;
    m_quizResultHeadlineLabel->setObjectName(QStringLiteral("sectionTitle"));
    resultCardLayout->addWidget(m_quizResultHeadlineLabel);

    m_quizResultSummaryLabel = makeLabel(QString(), QStringLiteral("bodyCopy"));
    resultCardLayout->addWidget(m_quizResultSummaryLabel);

    m_quizUploadStatusLabel = makeLabel(QString(), QStringLiteral("bodyCopy"));
    resultCardLayout->addWidget(m_quizUploadStatusLabel);

    auto *resultButtons = new QHBoxLayout;
    resultButtons->setContentsMargins(0, 8, 0, 0);
    resultButtons->setSpacing(12);

    auto *playAgainButton = new QPushButton(QStringLiteral("Play Another Quiz"));
    playAgainButton->setObjectName(QStringLiteral("primaryButton"));
    connect(playAgainButton, &QPushButton::clicked, this, [this] {
        m_quizStateStack->setCurrentIndex(0);
        updateQuestionPoolStatus();
    });

    m_quizUploadButton = new QPushButton(QStringLiteral("Upload Result"));
    m_quizUploadButton->setObjectName(QStringLiteral("secondaryButton"));
    connect(m_quizUploadButton, &QPushButton::clicked, this, [this] { uploadLastResult(); });

    auto *viewDashboardButton = new QPushButton(QStringLiteral("View Dashboard"));
    viewDashboardButton->setObjectName(QStringLiteral("secondaryButton"));
    connect(viewDashboardButton, &QPushButton::clicked, this, [this] { setCurrentPage(DashboardPage); });

    resultButtons->addWidget(playAgainButton);
    resultButtons->addWidget(m_quizUploadButton);
    resultButtons->addWidget(viewDashboardButton);
    resultButtons->addStretch(1);
    resultCardLayout->addLayout(resultButtons);

    resultLayout->addWidget(resultCard);
    resultLayout->addStretch(1);

    m_quizStateStack->addWidget(setupPage);
    m_quizStateStack->addWidget(livePage);
    m_quizStateStack->addWidget(resultPage);
    m_quizStateStack->setCurrentIndex(0);

    layout->addWidget(m_quizStateStack);
    return wrapInScrollArea(page);
}

QWidget *MainWindow::createCommunityPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    layout->addWidget(makeSectionHeader(
        QStringLiteral("Community picks"),
        QStringLiteral("Play quizzes that were built and shared on the website."),
        QStringLiteral("Browse the latest shared quizzes, open one by its share code, and launch it straight into the same in-app quiz flow.")
    ));

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(18);

    auto *directoryCard = new QFrame;
    directoryCard->setObjectName(QStringLiteral("card"));
    auto *directoryLayout = new QVBoxLayout(directoryCard);
    directoryLayout->setContentsMargins(22, 22, 22, 22);
    directoryLayout->setSpacing(12);
    directoryLayout->addWidget(makeLabel(QStringLiteral("Shared quiz list"), QStringLiteral("eyebrow"), false));
    directoryLayout->addWidget(makeLabel(QStringLiteral("Fresh from the website"), QStringLiteral("sectionTitle")));

    auto *directoryToolbar = new QHBoxLayout;
    directoryToolbar->setContentsMargins(0, 0, 0, 0);
    directoryToolbar->setSpacing(12);

    auto *refreshButton = new QPushButton(QStringLiteral("Refresh Shared Quizzes"));
    refreshButton->setObjectName(QStringLiteral("primaryButton"));
    connect(refreshButton, &QPushButton::clicked, this, [this] { refreshCommunityQuizzes(); });

    m_communityStatusLabel = makeLabel(QStringLiteral("Loading shared quizzes..."), QStringLiteral("bodyCopy"));
    directoryToolbar->addWidget(refreshButton);
    directoryToolbar->addWidget(m_communityStatusLabel, 1);
    directoryLayout->addLayout(directoryToolbar);

    m_communityQuizList = new QListWidget;
    m_communityQuizList->setMinimumHeight(420);
    connect(
        m_communityQuizList,
        &QListWidget::currentItemChanged,
        this,
        [this](QListWidgetItem *current, QListWidgetItem *) {
            if (current == nullptr) {
                return;
            }

            loadCommunityQuiz(current->data(Qt::UserRole).toString());
        }
    );
    directoryLayout->addWidget(m_communityQuizList);
    grid->addWidget(directoryCard, 0, 0);

    auto *detailCard = new QFrame;
    detailCard->setObjectName(QStringLiteral("card"));
    auto *detailLayout = new QVBoxLayout(detailCard);
    detailLayout->setContentsMargins(22, 22, 22, 22);
    detailLayout->setSpacing(12);
    detailLayout->addWidget(makeLabel(QStringLiteral("Selected quiz"), QStringLiteral("eyebrow"), false));

    m_communityQuizTitleLabel = new QLabel;
    m_communityQuizTitleLabel->setObjectName(QStringLiteral("sectionTitle"));
    m_communityQuizTitleLabel->setWordWrap(true);
    detailLayout->addWidget(m_communityQuizTitleLabel);

    m_communityQuizMetaLabel = makeLabel(QString(), QStringLiteral("bodyCopy"));
    detailLayout->addWidget(m_communityQuizMetaLabel);

    m_communityQuizDescriptionLabel = makeLabel(QString(), QStringLiteral("bodyCopy"));
    detailLayout->addWidget(m_communityQuizDescriptionLabel);

    m_communityQuizShareLabel = makeLabel(QString(), QStringLiteral("bodyCopy"));
    detailLayout->addWidget(m_communityQuizShareLabel);

    auto *slugRow = new QHBoxLayout;
    slugRow->setContentsMargins(0, 8, 0, 0);
    slugRow->setSpacing(12);

    m_communityQuizSlugEdit = new QLineEdit;
    m_communityQuizSlugEdit->setObjectName(QStringLiteral("settingsInput"));
    m_communityQuizSlugEdit->setPlaceholderText(QStringLiteral("Paste a share code like science-night-shift"));

    auto *openByCodeButton = new QPushButton(QStringLiteral("Open Share Code"));
    openByCodeButton->setObjectName(QStringLiteral("secondaryButton"));
    connect(openByCodeButton, &QPushButton::clicked, this, [this] {
        loadCommunityQuiz(m_communityQuizSlugEdit != nullptr ? m_communityQuizSlugEdit->text() : QString());
    });

    slugRow->addWidget(m_communityQuizSlugEdit, 1);
    slugRow->addWidget(openByCodeButton);
    detailLayout->addLayout(slugRow);

    m_communityPlayButton = new QPushButton(QStringLiteral("Play This Quiz"));
    m_communityPlayButton->setObjectName(QStringLiteral("primaryButton"));
    connect(m_communityPlayButton, &QPushButton::clicked, this, [this] {
        if (!m_activeCommunityQuestions.isEmpty()) {
            startQuizSession(
                m_activeCommunityQuestions,
                m_activeCommunityQuiz.category,
                m_activeCommunityQuiz.difficulty,
                QStringLiteral("community"),
                m_activeCommunityQuiz.title
            );
            return;
        }

        loadCommunityQuiz(
            m_communityQuizSlugEdit != nullptr ? m_communityQuizSlugEdit->text() : QString(),
            true
        );
    });
    detailLayout->addWidget(m_communityPlayButton, 0, Qt::AlignLeft);

    detailLayout->addWidget(createCard(
        QStringLiteral("Share flow"),
        QStringLiteral("Built on the website, played in the app"),
        QStringLiteral("When someone shares a community quiz link, the important bit for the app is the share code at the end of the URL. Paste that code here or pick the quiz from the latest list.")
    ));
    grid->addWidget(detailCard, 0, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

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
        QStringLiteral("One account powers the app and the website."),
        QStringLiteral("Sign in to pull your live stats, upload completed quizzes, and keep season progress and leaderboard results in sync across devices.")
    ));

    auto *summaryCard = new QFrame;
    summaryCard->setObjectName(QStringLiteral("card"));
    auto *summaryLayout = new QVBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(22, 22, 22, 22);
    summaryLayout->setSpacing(10);
    summaryLayout->addWidget(makeLabel(QStringLiteral("Current session"), QStringLiteral("eyebrow"), false));

    m_profileSummaryLabel = new QLabel;
    m_profileSummaryLabel->setObjectName(QStringLiteral("sectionTitle"));
    m_profileSummaryLabel->setWordWrap(true);
    summaryLayout->addWidget(m_profileSummaryLabel);

    m_profileStatusLabel = makeLabel(QString(), QStringLiteral("bodyCopy"));
    summaryLayout->addWidget(m_profileStatusLabel);
    layout->addWidget(summaryCard);

    auto *authCard = new QFrame;
    authCard->setObjectName(QStringLiteral("card"));
    auto *authLayout = new QVBoxLayout(authCard);
    authLayout->setContentsMargins(22, 22, 22, 22);
    authLayout->setSpacing(14);
    authLayout->addWidget(makeLabel(QStringLiteral("Account"), QStringLiteral("eyebrow"), false));
    authLayout->addWidget(makeLabel(QStringLiteral("Manage your session"), QStringLiteral("sectionTitle")));

    m_profileAuthHintLabel = makeLabel(
        QStringLiteral("Use the landing screen to sign in, create an account, or switch back to guest mode."),
        QStringLiteral("bodyCopy")
    );
    authLayout->addWidget(m_profileAuthHintLabel);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setContentsMargins(0, 8, 0, 0);
    buttonRow->setSpacing(12);

    auto *switchAccountButton = new QPushButton(QStringLiteral("Switch Account"));
    switchAccountButton->setObjectName(QStringLiteral("primaryButton"));
    connect(switchAccountButton, &QPushButton::clicked, this, [this] { showLandingPage(); });

    auto *refreshButton = new QPushButton(QStringLiteral("Refresh Stats"));
    refreshButton->setObjectName(QStringLiteral("secondaryButton"));
    connect(refreshButton, &QPushButton::clicked, this, [this] { refreshRemoteStats(); });

    auto *logoutButton = new QPushButton(QStringLiteral("Log Out"));
    logoutButton->setObjectName(QStringLiteral("secondaryButton"));
    connect(logoutButton, &QPushButton::clicked, this, [this] { handleLogout(); });

    auto *connectionButton = new QPushButton(QStringLiteral("Connection Settings"));
    connectionButton->setObjectName(QStringLiteral("secondaryButton"));
    connect(connectionButton, &QPushButton::clicked, this, [this] { setCurrentPage(SettingsPage); });

    buttonRow->addWidget(switchAccountButton);
    buttonRow->addWidget(refreshButton);
    buttonRow->addWidget(logoutButton);
    buttonRow->addWidget(connectionButton);
    buttonRow->addStretch(1);
    authLayout->addLayout(buttonRow);

    layout->addWidget(authCard);
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
        QStringLiteral("See how every run stacks up."),
        QStringLiteral("When a synced run lands, you can refresh here and see where it settles on the board.")
    ));

    auto *tableCard = new QFrame;
    tableCard->setObjectName(QStringLiteral("card"));
    auto *tableLayout = new QVBoxLayout(tableCard);
    tableLayout->setContentsMargins(22, 22, 22, 22);
    tableLayout->setSpacing(12);

    auto *toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(12);

    auto *refreshButton = new QPushButton(QStringLiteral("Refresh Leaderboard"));
    refreshButton->setObjectName(QStringLiteral("primaryButton"));
    connect(refreshButton, &QPushButton::clicked, this, [this] { refreshLeaderboard(); });

    m_leaderboardStatusLabel = makeLabel(QString(), QStringLiteral("bodyCopy"));
    toolbar->addWidget(refreshButton);
    toolbar->addWidget(m_leaderboardStatusLabel, 1);
    tableLayout->addLayout(toolbar);

    m_leaderboardTable = new QTableWidget(0, 5);
    m_leaderboardTable->setHorizontalHeaderLabels({
        QStringLiteral("Rank"),
        QStringLiteral("Player"),
        QStringLiteral("Category"),
        QStringLiteral("XP"),
        QStringLiteral("Score"),
    });
    m_leaderboardTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_leaderboardTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_leaderboardTable->verticalHeader()->hide();
    m_leaderboardTable->horizontalHeader()->setStretchLastSection(true);
    m_leaderboardTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_leaderboardTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_leaderboardTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_leaderboardTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_leaderboardTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_leaderboardTable->setMinimumHeight(360);
    tableLayout->addWidget(m_leaderboardTable);

    layout->addWidget(tableCard);
    return wrapInScrollArea(page);
}

QWidget *MainWindow::createBattlePassPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    layout->addWidget(makeSectionHeader(
        QStringLiteral("Season progress"),
        QStringLiteral("Every completed quiz should move the reward track forward."),
        QStringLiteral("The app updates battle pass progress after each run, then refreshes the synced season state when you are signed in.")
    ));

    auto *progressCard = new QFrame;
    progressCard->setObjectName(QStringLiteral("card"));
    auto *progressLayout = new QVBoxLayout(progressCard);
    progressLayout->setContentsMargins(22, 22, 22, 22);
    progressLayout->setSpacing(10);
    progressLayout->addWidget(makeLabel(QStringLiteral("Season"), QStringLiteral("eyebrow"), false));

    m_battlePassSeasonLabel = new QLabel;
    m_battlePassSeasonLabel->setObjectName(QStringLiteral("sectionTitle"));
    m_battlePassSeasonLabel->setWordWrap(true);
    progressLayout->addWidget(m_battlePassSeasonLabel);

    m_battlePassTierLabel = makeLabel(QString(), QStringLiteral("bodyCopy"));
    progressLayout->addWidget(m_battlePassTierLabel);

    m_battlePassProgressLabel = makeLabel(QString(), QStringLiteral("bodyCopy"));
    progressLayout->addWidget(m_battlePassProgressLabel);

    m_battlePassProgressBar = new QProgressBar;
    progressLayout->addWidget(m_battlePassProgressBar);
    layout->addWidget(progressCard);

    auto *rewardsCard = new QFrame;
    rewardsCard->setObjectName(QStringLiteral("card"));
    auto *rewardsLayout = new QVBoxLayout(rewardsCard);
    rewardsLayout->setContentsMargins(22, 22, 22, 22);
    rewardsLayout->setSpacing(10);
    rewardsLayout->addWidget(makeLabel(QStringLiteral("Rewards"), QStringLiteral("eyebrow"), false));
    rewardsLayout->addWidget(makeLabel(QStringLiteral("Season reward track"), QStringLiteral("sectionTitle")));

    m_battlePassRewardsList = new QListWidget;
    m_battlePassRewardsList->setMinimumHeight(320);
    rewardsLayout->addWidget(m_battlePassRewardsList);
    layout->addWidget(rewardsCard);

    return wrapInScrollArea(page);
}

QWidget *MainWindow::createSettingsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    layout->addWidget(makeSectionHeader(
        QStringLiteral("Connection help"),
        QStringLiteral("Change the website address only if you are switching environments."),
        QStringLiteral("Most players will never need this page. It is here for local testing, support, or moving between a local site and your live website.")
    ));

    auto *settingsCard = new QFrame;
    settingsCard->setObjectName(QStringLiteral("card"));
    auto *settingsLayout = new QVBoxLayout(settingsCard);
    settingsLayout->setContentsMargins(22, 22, 22, 22);
    settingsLayout->setSpacing(12);
    settingsLayout->addWidget(makeLabel(QStringLiteral("Website address"), QStringLiteral("eyebrow"), false));
    settingsLayout->addWidget(makeLabel(QStringLiteral("Where the app syncs"), QStringLiteral("sectionTitle")));
    settingsLayout->addWidget(makeLabel(
        QStringLiteral("The app stores this address locally and uses it for sign-in, stat refreshes, shared quizzes, leaderboard reads, and quiz result uploads."),
        QStringLiteral("bodyCopy")
    ));

    m_apiBaseUrlEdit = new QLineEdit(AppConfig::loadApiBaseUrl().toString());
    m_apiBaseUrlEdit->setObjectName(QStringLiteral("settingsInput"));
    settingsLayout->addWidget(m_apiBaseUrlEdit);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setContentsMargins(0, 8, 0, 0);
    buttonRow->setSpacing(12);

    auto *saveButton = new QPushButton(QStringLiteral("Save Website Address"));
    saveButton->setObjectName(QStringLiteral("primaryButton"));
    connect(saveButton, &QPushButton::clicked, this, [this] { saveApiBaseUrl(); });

    auto *resetButton = new QPushButton(QStringLiteral("Use Default Address"));
    resetButton->setObjectName(QStringLiteral("secondaryButton"));
    connect(resetButton, &QPushButton::clicked, this, [this] {
        m_apiBaseUrlEdit->setText(AppConfig::defaultApiBaseUrl().toString());
        saveApiBaseUrl();
    });

    auto *backButton = new QPushButton(QStringLiteral("Back to Profile"));
    backButton->setObjectName(QStringLiteral("secondaryButton"));
    connect(backButton, &QPushButton::clicked, this, [this] { setCurrentPage(ProfilePage); });

    buttonRow->addWidget(saveButton);
    buttonRow->addWidget(resetButton);
    buttonRow->addWidget(backButton);
    buttonRow->addStretch(1);
    settingsLayout->addLayout(buttonRow);
    layout->addWidget(settingsCard);

    layout->addWidget(createCard(
        QStringLiteral("When to use this"),
        QStringLiteral("Only change the address if something is clearly wrong"),
        QStringLiteral("If sign-in, shared quizzes, or synced uploads stop working because you are pointing at the wrong website, update the address here and try again.")
    ));

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
    button->setProperty("pageIndex", index);
    connect(button, &QPushButton::clicked, this, [this, index] { setCurrentPage(index); });
    return button;
}

// Shell and page routing
void MainWindow::showLandingPage()
{
    if (m_rootStack != nullptr) {
        m_rootStack->setCurrentIndex(0);
    }

    statusBar()->showMessage(QStringLiteral("Choose how you want to start."), 3000);
}

void MainWindow::showMainShell(int pageIndex)
{
    if (m_rootStack != nullptr && m_mainShellPage != nullptr) {
        m_rootStack->setCurrentWidget(m_mainShellPage);
    }

    setCurrentPage(pageIndex);
}

void MainWindow::setCurrentPage(int index)
{
    if (m_pageStack == nullptr) {
        return;
    }

    m_pageStack->setCurrentIndex(index);

    const auto buttons = findChildren<QPushButton *>(QStringLiteral("navButton"));
    for (auto *button : buttons) {
        button->setChecked(button->property("pageIndex").toInt() == index);
    }

    switch (index) {
    case DashboardPage:
        m_pageTitleLabel->setText(QStringLiteral("Dashboard"));
        updateDashboardUi();
        break;
    case QuizPage:
        m_pageTitleLabel->setText(QStringLiteral("Quiz"));
        updateQuestionPoolStatus();
        break;
    case CommunityPage:
        m_pageTitleLabel->setText(QStringLiteral("Community"));
        updateCommunityQuizUi();
        break;
    case ProfilePage:
        m_pageTitleLabel->setText(QStringLiteral("Profile"));
        updateProfileUi();
        break;
    case LeaderboardsPage:
        m_pageTitleLabel->setText(QStringLiteral("Leaderboards"));
        break;
    case BattlePassPage:
        m_pageTitleLabel->setText(QStringLiteral("Battle Pass"));
        updateBattlePassUi();
        break;
    case SettingsPage:
        m_pageTitleLabel->setText(QStringLiteral("Settings"));
        break;
    default:
        break;
    }
}

void MainWindow::saveApiBaseUrl()
{
    const QUrl url = QUrl::fromUserInput(m_apiBaseUrlEdit->text().trimmed());

    if (!url.isValid() || url.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("Invalid address"),
            QStringLiteral("Enter a valid website address such as https://quizforge.chococookie.org.")
        );
        return;
    }

    AppConfig::saveApiBaseUrl(url);

    if (m_apiClient != nullptr) {
        m_apiClient->setBaseUrl(url);
    }

    updateApiStatus();
    statusBar()->showMessage(QStringLiteral("Website address saved."), 3000);
}

void MainWindow::updateApiStatus()
{
    if (m_statusLabel == nullptr) {
        return;
    }

    const QUrl url = m_apiClient != nullptr ? m_apiClient->baseUrl() : AppConfig::loadApiBaseUrl();
    const QString syncState = m_playerProfile.signedIn
                                  ? QStringLiteral("Cloud sync on")
                                  : (m_hasPendingUpload
                                         ? QStringLiteral("Result waiting")
                                         : QStringLiteral("Guest mode"));
    const QString targetState = isLocalUrl(url)
                                    ? QStringLiteral("local website")
                                    : QStringLiteral("live website");
    m_statusLabel->setText(QStringLiteral("%1 | %2").arg(syncState, targetState));
}

// Quiz setup and gameplay flow
void MainWindow::updateQuizTimer()
{
    if (m_quizTimerLabel == nullptr) {
        return;
    }

    const int elapsed = m_quizTimer.isValid() ? static_cast<int>(m_quizTimer.elapsed()) : 0;
    m_quizTimerLabel->setText(formatDuration(elapsed));
}

void MainWindow::loadQuestions()
{
    QString errorMessage;
    m_allQuestions = QuestionBank::loadFromDefaultLocation(&errorMessage);

    if (m_allQuestions.isEmpty()) {
        statusBar()->showMessage(
            errorMessage.trimmed().isEmpty()
                ? QStringLiteral("No quiz questions could be loaded.")
                : errorMessage,
            5000
        );
        return;
    }

    statusBar()->showMessage(
        QStringLiteral("Loaded %1 questions.").arg(formatNumber(m_allQuestions.size())),
        4000
    );
}

void MainWindow::populateQuestionFilters()
{
    if (m_categoryCombo == nullptr || m_difficultyCombo == nullptr) {
        return;
    }

    const bool categorySignalsBlocked = m_categoryCombo->blockSignals(true);
    const bool difficultySignalsBlocked = m_difficultyCombo->blockSignals(true);

    m_categoryCombo->clear();
    m_categoryCombo->addItem(QStringLiteral("Mixed"), QStringLiteral("Mixed"));

    const QStringList categories = QuestionBank::availableCategories(m_allQuestions);
    for (const auto &category : categories) {
        m_categoryCombo->addItem(category, category);
    }

    if (m_difficultyCombo->count() == 0) {
        m_difficultyCombo->addItem(QStringLiteral("Easy"), QStringLiteral("easy"));
        m_difficultyCombo->addItem(QStringLiteral("Medium"), QStringLiteral("medium"));
        m_difficultyCombo->addItem(QStringLiteral("Hard"), QStringLiteral("hard"));
    }

    m_difficultyCombo->setCurrentIndex(1);

    m_categoryCombo->blockSignals(categorySignalsBlocked);
    m_difficultyCombo->blockSignals(difficultySignalsBlocked);
}

void MainWindow::updateQuestionPoolStatus()
{
    if (m_categoryCombo == nullptr || m_difficultyCombo == nullptr ||
        m_questionCountSpinBox == nullptr || m_questionPoolStatusLabel == nullptr ||
        m_startQuizButton == nullptr) {
        return;
    }

    const QString category = m_categoryCombo->currentData().toString();
    const QString difficulty = m_difficultyCombo->currentData().toString();
    const QVector<QuizQuestion> filtered = QuestionBank::filteredQuestions(
        m_allQuestions,
        category,
        difficulty
    );
    const int available = filtered.size();

    if (available <= 0) {
        m_questionCountSpinBox->setRange(1, 1);
        m_questionCountSpinBox->setValue(1);
        m_questionCountSpinBox->setEnabled(false);
        m_startQuizButton->setEnabled(false);
        m_questionPoolStatusLabel->setText(QStringLiteral("No questions available for that combination yet."));
        return;
    }

    const int suggestedCount = m_questionCountSpinBox->value() > 0
                                   ? m_questionCountSpinBox->value()
                                   : std::min(available, 5);
    m_questionCountSpinBox->setRange(1, available);
    m_questionCountSpinBox->setValue(std::clamp(suggestedCount, 1, available));
    m_questionCountSpinBox->setEnabled(true);
    m_startQuizButton->setEnabled(true);
    m_questionPoolStatusLabel->setText(
        QStringLiteral("%1 questions available for %2 on %3.")
            .arg(formatNumber(available))
            .arg(category.trimmed().isEmpty() ? QStringLiteral("Mixed") : category)
            .arg(prettifyToken(difficulty))
    );
}

void MainWindow::startQuiz()
{
    const QString category = m_categoryCombo != nullptr
                                 ? m_categoryCombo->currentData().toString()
                                 : QStringLiteral("Mixed");
    const QString difficulty = m_difficultyCombo != nullptr
                                   ? m_difficultyCombo->currentData().toString()
                                   : QStringLiteral("medium");

    QVector<QuizQuestion> filtered = QuestionBank::filteredQuestions(
        m_allQuestions,
        category,
        difficulty
    );

    if (filtered.isEmpty()) {
        QMessageBox::information(
            this,
            QStringLiteral("No questions available"),
            QStringLiteral("There are no questions available for that category and difficulty yet.")
        );
        return;
    }

    std::mt19937 rng(QRandomGenerator::global()->generate());
    std::shuffle(filtered.begin(), filtered.end(), rng);

    const int requestedCount = m_questionCountSpinBox != nullptr
                                   ? m_questionCountSpinBox->value()
                                   : std::min(static_cast<int>(filtered.size()), 5);
    const int questionCount = std::min(requestedCount, static_cast<int>(filtered.size()));

    QVector<QuizQuestion> selectedQuestions;
    selectedQuestions.reserve(questionCount);
    for (int index = 0; index < questionCount; ++index) {
        selectedQuestions.push_back(filtered.at(index));
    }

    startQuizSession(
        selectedQuestions,
        category,
        difficulty,
        QStringLiteral("classic"),
        QStringLiteral("Classic Quiz")
    );
}

void MainWindow::startQuizSession(
    const QVector<QuizQuestion> &questions,
    const QString &category,
    const QString &difficulty,
    const QString &quizType,
    const QString &title
)
{
    if (questions.isEmpty()) {
        QMessageBox::information(
            this,
            QStringLiteral("Quiz unavailable"),
            QStringLiteral("That quiz could not be started because no questions were available.")
        );
        return;
    }

    m_quizSession = {};
    m_quizSession.questions = questions;
    m_quizSession.quizType = quizType;
    m_quizSession.title = title;
    m_quizSession.category = category;
    m_quizSession.difficulty = difficulty;
    m_quizSession.currentIndex = 0;
    m_quizSession.correctAnswers = 0;
    m_quizSession.answeredCurrentQuestion = false;
    m_quizSession.active = true;

    m_quizStateStack->setCurrentIndex(1);
    m_quizTimer.start();
    m_quizUiTimer->start();
    updateQuizTimer();
    showCurrentQuestion();
    setCurrentPage(QuizPage);

    statusBar()->showMessage(QStringLiteral("%1 started.").arg(title), 2000);
}

void MainWindow::showCurrentQuestion()
{
    if (!m_quizSession.active || m_quizSession.currentIndex < 0 ||
        m_quizSession.currentIndex >= m_quizSession.questions.size()) {
        finishQuiz();
        return;
    }

    const QuizQuestion &question = m_quizSession.questions.at(m_quizSession.currentIndex);

    m_quizProgressLabel->setText(
        QStringLiteral("Question %1 of %2")
            .arg(m_quizSession.currentIndex + 1)
            .arg(m_quizSession.questions.size())
    );
    m_quizMetaLabel->setText(
        m_quizSession.title.trimmed().isEmpty()
            ? QStringLiteral("%1  |  %2 difficulty")
                  .arg(question.category, prettifyToken(question.difficulty))
            : QStringLiteral("%1  |  %2  |  %3 difficulty")
                  .arg(m_quizSession.title, question.category, prettifyToken(question.difficulty))
    );
    m_quizQuestionLabel->setText(question.question);
    m_quizFeedbackLabel->setObjectName(QStringLiteral("bodyCopy"));
    m_quizFeedbackLabel->style()->unpolish(m_quizFeedbackLabel);
    m_quizFeedbackLabel->style()->polish(m_quizFeedbackLabel);
    m_quizFeedbackLabel->setText(QStringLiteral("Choose the best answer."));

    resetAnswerButtons();

    const int currentQuestionIndex = m_quizSession.currentIndex;

    for (int index = 0; index < m_answerButtons.size(); ++index) {
        auto *button = m_answerButtons.at(index);
        if (index < question.answers.size()) {
            button->setText(question.answers.at(index));
            button->hide();
            button->setEnabled(false);

            QPointer<QPushButton> safeButton(button);
            // Staggered reveal adds motion without using opacity/graphics effects,
            // which were unstable in this app on some machines.
            QTimer::singleShot(50 + (index * 70), this, [this, safeButton, currentQuestionIndex] {
                if (safeButton.isNull() ||
                    !m_quizSession.active ||
                    m_quizSession.currentIndex != currentQuestionIndex ||
                    m_quizSession.answeredCurrentQuestion) {
                    return;
                }

                safeButton->show();
                safeButton->setEnabled(true);
            });
        } else {
            button->hide();
        }
    }

    m_nextQuestionButton->setText(
        m_quizSession.currentIndex + 1 >= m_quizSession.questions.size()
            ? QStringLiteral("See Results")
            : QStringLiteral("Next Question")
    );
    m_nextQuestionButton->setEnabled(false);
}

void MainWindow::handleAnswerSelected(int answerIndex)
{
    if (!m_quizSession.active || m_quizSession.answeredCurrentQuestion ||
        m_quizSession.currentIndex < 0 ||
        m_quizSession.currentIndex >= m_quizSession.questions.size()) {
        return;
    }

    const QuizQuestion &question = m_quizSession.questions.at(m_quizSession.currentIndex);
    if (answerIndex < 0 || answerIndex >= question.answers.size()) {
        return;
    }

    m_quizSession.answeredCurrentQuestion = true;

    const bool isCorrect = answerIndex == question.correctIndex;
    if (isCorrect) {
        ++m_quizSession.correctAnswers;
    }

    for (int index = 0; index < m_answerButtons.size(); ++index) {
        auto *button = m_answerButtons.at(index);
        button->setEnabled(false);

        if (!button->isVisible()) {
            continue;
        }

        if (index == question.correctIndex) {
            button->setStyleSheet(
                QStringLiteral("background: rgba(88, 214, 200, 0.16); border: 2px solid #58d6c8; color: #f6f1e9;")
            );
        } else if (index == answerIndex) {
            button->setStyleSheet(
                QStringLiteral("background: rgba(255, 143, 143, 0.16); border: 2px solid #ff8f8f; color: #f6f1e9;")
            );
        }
    }

    m_quizFeedbackLabel->setObjectName(
        isCorrect ? QStringLiteral("feedbackPositive") : QStringLiteral("feedbackNegative")
    );
    m_quizFeedbackLabel->style()->unpolish(m_quizFeedbackLabel);
    m_quizFeedbackLabel->style()->polish(m_quizFeedbackLabel);
    m_quizFeedbackLabel->setText(
        QStringLiteral("%1 %2")
            .arg(isCorrect ? QStringLiteral("Correct.") : QStringLiteral("Not quite."))
            .arg(question.explanation)
    );
    m_nextQuestionButton->setEnabled(true);
}

void MainWindow::advanceQuiz()
{
    if (!m_quizSession.active || !m_quizSession.answeredCurrentQuestion) {
        return;
    }

    ++m_quizSession.currentIndex;
    m_quizSession.answeredCurrentQuestion = false;

    if (m_quizSession.currentIndex >= m_quizSession.questions.size()) {
        finishQuiz();
        return;
    }

    showCurrentQuestion();
}

void MainWindow::finishQuiz()
{
    const int totalQuestions = m_quizSession.questions.size();
    if (totalQuestions <= 0) {
        m_quizStateStack->setCurrentIndex(0);
        return;
    }

    m_quizUiTimer->stop();

    const int correctAnswers = std::clamp(m_quizSession.correctAnswers, 0, totalQuestions);
    const int durationMs = m_quizTimer.isValid() ? static_cast<int>(m_quizTimer.elapsed()) : 0;
    const bool streakBonus = m_playerProfile.currentStreak > 0;
    const int score = calculateScore(m_quizSession.difficulty, correctAnswers, totalQuestions);
    const int xpEarned = calculateXp(
        m_quizSession.difficulty,
        correctAnswers,
        totalQuestions,
        durationMs,
        streakBonus
    );
    const int accuracy = calculateAccuracyPercent(correctAnswers, totalQuestions);

    RecentRun run;
    run.category = !m_quizSession.title.trimmed().isEmpty()
                       ? m_quizSession.title
                       : (m_quizSession.category.trimmed().isEmpty()
                              ? QStringLiteral("Mixed")
                              : m_quizSession.category);
    run.difficulty = m_quizSession.difficulty;
    run.score = score;
    run.xpEarned = xpEarned;
    run.accuracy = accuracy;
    run.durationMs = durationMs;
    run.uploaded = false;
    run.playedAtText = QDateTime::currentDateTime().toString(QStringLiteral("dd MMM HH:mm"));

    applyLocalQuizResult(run);

    m_lastUploadPayload.quizType = m_quizSession.quizType.trimmed().isEmpty()
                                       ? QStringLiteral("classic")
                                       : m_quizSession.quizType;
    m_lastUploadPayload.category = m_quizSession.category.trimmed().isEmpty()
                                       ? QStringLiteral("General")
                                       : m_quizSession.category;
    m_lastUploadPayload.difficulty = m_quizSession.difficulty;
    m_lastUploadPayload.correctAnswers = correctAnswers;
    m_lastUploadPayload.totalQuestions = totalQuestions;
    m_lastUploadPayload.durationMs = durationMs;
    m_lastUploadPayload.streakBonus = streakBonus;
    m_hasPendingUpload = true;

    m_quizResultHeadlineLabel->setText(
        accuracy >= 90
            ? QStringLiteral("Brilliant run.")
            : accuracy >= 70
                  ? QStringLiteral("Solid performance.")
                  : QStringLiteral("Run complete.")
    );
    m_quizResultSummaryLabel->setText(
        QStringLiteral("%1\n%2/%3 correct  |  %4%% accuracy  |  Score %5  |  +%6 XP  |  %7")
            .arg(run.category)
            .arg(correctAnswers)
            .arg(totalQuestions)
            .arg(accuracy)
            .arg(formatNumber(score))
            .arg(formatNumber(xpEarned))
            .arg(formatDuration(durationMs))
    );

    if (m_playerProfile.signedIn && !m_accessToken.trimmed().isEmpty()) {
        m_quizUploadStatusLabel->setText(QStringLiteral("Signed in. Syncing this result now..."));
        m_quizUploadButton->setEnabled(false);
    } else {
        m_quizUploadStatusLabel->setText(
            QStringLiteral("Saved locally. Sign in if you want this result to count on the website too.")
        );
        m_quizUploadButton->setEnabled(false);
    }

    m_quizSession = {};
    m_quizStateStack->setCurrentIndex(2);

    if (m_playerProfile.signedIn && !m_accessToken.trimmed().isEmpty()) {
        uploadLastResult();
    }
}

void MainWindow::resetAnswerButtons()
{
    for (auto *button : m_answerButtons) {
        button->setStyleSheet(QString());
        button->setEnabled(true);
    }
}

// Dashboard, profile, and progression UI refresh
void MainWindow::updateDashboardUi()
{
    if (m_dashboardWelcomeLabel == nullptr) {
        return;
    }

    const QString playerName = !m_playerProfile.displayName.trimmed().isEmpty()
                                   ? m_playerProfile.displayName
                                   : (!m_playerProfile.username.trimmed().isEmpty()
                                          ? m_playerProfile.username
                                          : QStringLiteral("Local Challenger"));

    m_dashboardWelcomeLabel->setText(
        QStringLiteral("Welcome back, %1").arg(playerName)
    );

    m_dashboardSeasonValueLabel->setText(
        m_playerProfile.signedIn
            ? QStringLiteral("%1  |  %2 days left  |  %3  |  Title: %4")
                  .arg(m_seasonName)
                  .arg(m_daysLeftInSeason)
                  .arg(m_lastSyncMessage)
                  .arg(m_playerProfile.equippedTitleName)
            : QStringLiteral("Guest mode is active. Your runs still earn local XP and season progress.")
    );

    m_dashboardLevelValueLabel->setText(QString::number(std::max(m_playerProfile.level, 1)));
    m_dashboardXpValueLabel->setText(formatNumber(std::max(m_playerProfile.totalXp, 0)));
    m_dashboardScoreValueLabel->setText(formatNumber(std::max(m_playerProfile.totalScore, 0)));
    m_dashboardQuizCountValueLabel->setText(formatNumber(std::max(m_playerProfile.quizzesCompleted, 0)));

    updateRecentRunsUi();
    updateApiStatus();
}

void MainWindow::updateProfileUi()
{
    if (m_profileSummaryLabel == nullptr || m_profileStatusLabel == nullptr) {
        return;
    }

    const QString displayName = !m_playerProfile.displayName.trimmed().isEmpty()
                                    ? m_playerProfile.displayName
                                    : QStringLiteral("Local Challenger");
    const QString usernameText = m_playerProfile.username.trimmed().isEmpty()
                                     ? QStringLiteral("not set")
                                     : QStringLiteral("@%1").arg(m_playerProfile.username);

    if (m_playerProfile.signedIn) {
        m_profileSummaryLabel->setText(
            QStringLiteral("%1  |  %2  |  %3  |  Level %4  |  %5 XP  |  %6 cosmetics")
                .arg(displayName)
                .arg(usernameText)
                .arg(m_playerProfile.equippedTitleName)
                .arg(m_playerProfile.level)
                .arg(formatNumber(m_playerProfile.totalXp))
                .arg(formatNumber(m_playerProfile.cosmeticCount))
        );
    } else {
        m_profileSummaryLabel->setText(
            QStringLiteral("%1 is playing locally. Sign in to sync stats, uploads, shared quizzes, and season progress.")
                .arg(displayName)
        );
    }

    QString statusText = m_lastSyncMessage;
    if (m_playerProfile.lastSyncedAt.isValid()) {
        statusText += QStringLiteral(" Last update: %1.")
                          .arg(m_playerProfile.lastSyncedAt.toString(QStringLiteral("dd MMM HH:mm")));
    }
    if (m_playerProfile.signedIn) {
        statusText += QStringLiteral(" Equipped frame: %1. Hover effect: %2.")
                          .arg(m_playerProfile.equippedAvatarFrame)
                          .arg(m_playerProfile.equippedHoverAnimation);
    }
    m_profileStatusLabel->setText(statusText);

    if (m_profileAuthHintLabel != nullptr) {
        m_profileAuthHintLabel->setText(
            m_playerProfile.signedIn
                ? QStringLiteral("You are signed in. Use Switch Account if you want to go back to the welcome screen.")
                : QStringLiteral("Guest mode is active. Use Switch Account to sign in or create an account.")
        );
    }

    if (m_playerProfile.signedIn &&
        m_emailEdit != nullptr &&
        m_playerProfile.email != m_emailEdit->text()) {
        m_emailEdit->setText(m_playerProfile.email);
    }

    updateApiStatus();
}

void MainWindow::updateBattlePassUi()
{
    if (m_battlePassSeasonLabel == nullptr ||
        m_battlePassTierLabel == nullptr ||
        m_battlePassProgressLabel == nullptr ||
        m_battlePassProgressBar == nullptr ||
        m_battlePassRewardsList == nullptr) {
        return;
    }

    const int currentTier = std::max(m_playerProfile.battlePassTier, 1);
    const int currentXp = std::max(m_playerProfile.battlePassXp, 0);
    const int floorXp = currentBattlePassFloorXp();
    const int nextTierXp = computedNextBattlePassXp();
    const int tierSpan = std::max(nextTierXp - floorXp, 1);
    const int tierProgress = std::clamp(currentXp - floorXp, 0, tierSpan);
    const int xpRemaining = std::max(nextTierXp - currentXp, 0);

    m_battlePassSeasonLabel->setText(
        QStringLiteral("%1  |  %2 days left").arg(m_seasonName).arg(m_daysLeftInSeason)
    );
    m_battlePassTierLabel->setText(
        QStringLiteral("Tier %1  |  %2 XP total").arg(currentTier).arg(formatNumber(currentXp))
    );
    m_battlePassProgressLabel->setText(
        QStringLiteral("%1 XP until the next reward.").arg(formatNumber(xpRemaining))
    );

    m_battlePassProgressBar->setRange(0, tierSpan);
    m_battlePassProgressBar->setFormat(
        QStringLiteral("%1 / %2")
            .arg(formatNumber(currentXp - floorXp))
            .arg(formatNumber(tierSpan))
    );
    // Animate the progress bar's built-in value property instead of layering custom effects onto the widget.
    animateBattlePassProgress(tierProgress);

    m_battlePassRewardsList->clear();
    for (const auto &reward : rewardTrack()) {
        const bool unlocked = currentXp >= reward.xpRequired;
        const QString state = unlocked
                                  ? QStringLiteral("Unlocked")
                                  : QStringLiteral("%1 XP to go")
                                        .arg(formatNumber(std::max(reward.xpRequired - currentXp, 0)));
        m_battlePassRewardsList->addItem(
            QStringLiteral("Tier %1  |  %2  |  %3  |  %4")
                .arg(reward.tier)
                .arg(prettifyToken(reward.rarity))
                .arg(reward.rewardName)
                .arg(state)
        );
    }
}

void MainWindow::animateBattlePassProgress(int targetValue)
{
    if (m_battlePassProgressBar == nullptr) {
        return;
    }

    const int clampedTarget = std::clamp(
        targetValue,
        m_battlePassProgressBar->minimum(),
        m_battlePassProgressBar->maximum()
    );

    if (m_battlePassProgressAnimation != nullptr) {
        m_battlePassProgressAnimation->stop();
        m_battlePassProgressAnimation->deleteLater();
        m_battlePassProgressAnimation = nullptr;
    }

    const int startValue = m_battlePassProgressBar->value();
    if (startValue == clampedTarget) {
        m_battlePassProgressBar->setValue(clampedTarget);
        return;
    }

    m_battlePassProgressAnimation = new QPropertyAnimation(m_battlePassProgressBar, "value", this);
    m_battlePassProgressAnimation->setDuration(320);
    m_battlePassProgressAnimation->setStartValue(startValue);
    m_battlePassProgressAnimation->setEndValue(clampedTarget);
    m_battlePassProgressAnimation->setEasingCurve(QEasingCurve::OutCubic);

    connect(
        m_battlePassProgressAnimation,
        &QPropertyAnimation::finished,
        this,
        [this] {
            if (m_battlePassProgressAnimation != nullptr) {
                m_battlePassProgressAnimation->deleteLater();
                m_battlePassProgressAnimation = nullptr;
            }
        }
    );

    m_battlePassProgressAnimation->start();
}

void MainWindow::updateRecentRunsUi()
{
    if (m_dashboardRecentRunsList == nullptr) {
        return;
    }

    m_dashboardRecentRunsList->clear();

    if (m_recentRuns.isEmpty()) {
        m_dashboardRecentRunsList->addItem(QStringLiteral("No runs yet. Head to Quiz and play your first round."));
        return;
    }

    for (const auto &run : m_recentRuns) {
        const QString difficulty = run.difficulty.trimmed().isEmpty()
                                       ? QStringLiteral("Standard")
                                       : prettifyToken(run.difficulty);
        const QString syncState = run.uploaded ? QStringLiteral("Synced") : QStringLiteral("Pending upload");
        const QString duration = run.durationMs > 0 ? formatDuration(run.durationMs) : QStringLiteral("--:--");

        m_dashboardRecentRunsList->addItem(
            QStringLiteral("%1  |  %2  |  Score %3  |  +%4 XP  |  %5%%  |  %6  |  %7  |  %8")
                .arg(run.category)
                .arg(difficulty)
                .arg(formatNumber(run.score))
                .arg(formatNumber(run.xpEarned))
                .arg(run.accuracy)
                .arg(duration)
                .arg(syncState)
                .arg(run.playedAtText)
        );
    }
}

// Supporting lists and cards
void MainWindow::updateCommunityQuizUi()
{
    if (m_communityQuizTitleLabel == nullptr ||
        m_communityQuizMetaLabel == nullptr ||
        m_communityQuizDescriptionLabel == nullptr ||
        m_communityQuizShareLabel == nullptr ||
        m_communityPlayButton == nullptr) {
        return;
    }

    if (m_activeCommunityQuiz.slug.trimmed().isEmpty()) {
        m_communityQuizTitleLabel->setText(QStringLiteral("Pick a shared quiz"));
        m_communityQuizMetaLabel->setText(
            QStringLiteral("Select a quiz from the list or paste a share code to load one directly.")
        );
        m_communityQuizDescriptionLabel->setText(
            QStringLiteral("Community quizzes are created on the website and pulled into the desktop app when you refresh this page.")
        );
        m_communityQuizShareLabel->setText(QStringLiteral("Share code ready: waiting for a selection."));
        m_communityPlayButton->setEnabled(false);
        return;
    }

    m_communityQuizTitleLabel->setText(m_activeCommunityQuiz.title);
    m_communityQuizMetaLabel->setText(
        QStringLiteral("%1  |  %2 difficulty  |  %3 questions  |  Shared by %4")
            .arg(m_activeCommunityQuiz.category)
            .arg(prettifyToken(m_activeCommunityQuiz.difficulty))
            .arg(formatNumber(m_activeCommunityQuiz.questionCount))
            .arg(m_activeCommunityQuiz.authorName.trimmed().isEmpty()
                     ? QStringLiteral("the community")
                     : m_activeCommunityQuiz.authorName)
    );
    m_communityQuizDescriptionLabel->setText(
        m_activeCommunityQuiz.description.trimmed().isEmpty()
            ? QStringLiteral("No extra description was added for this quiz.")
            : m_activeCommunityQuiz.description
    );
    m_communityQuizShareLabel->setText(
        QStringLiteral("Share code: %1").arg(m_activeCommunityQuiz.slug)
    );
    m_communityPlayButton->setEnabled(!m_activeCommunityQuestions.isEmpty());
}

void MainWindow::updateLeaderboardStatus(const QString &message)
{
    if (m_leaderboardStatusLabel != nullptr) {
        m_leaderboardStatusLabel->setText(message);
    }
}

// Result aggregation and account entry points
void MainWindow::applyLocalQuizResult(const RecentRun &run)
{
    const int previousRuns = std::max(m_playerProfile.quizzesCompleted, 0);
    const int previousAccuracy = std::max(m_playerProfile.accuracy, 0);

    m_playerProfile.totalScore += run.score;
    m_playerProfile.totalXp += run.xpEarned;
    m_playerProfile.quizzesCompleted = previousRuns + 1;
    m_playerProfile.accuracy = static_cast<int>(std::round(
        ((previousAccuracy * previousRuns) + run.accuracy) /
        static_cast<double>(m_playerProfile.quizzesCompleted)
    ));
    m_playerProfile.level = calculateLevelFromXp(m_playerProfile.totalXp);

    if (run.accuracy >= 80) {
        ++m_playerProfile.currentStreak;
    } else {
        m_playerProfile.currentStreak = 0;
    }

    m_playerProfile.battlePassXp += run.xpEarned;
    m_playerProfile.battlePassTier = std::max((m_playerProfile.battlePassXp / 200) + 1, 1);
    m_playerProfile.nextBattlePassXp = std::max(m_playerProfile.battlePassTier * 200, 200);
    m_playerProfile.lastSyncedAt = QDateTime::currentDateTime();

    m_recentRuns.prepend(run);
    while (m_recentRuns.size() > 12) {
        m_recentRuns.removeLast();
    }

    updateDashboardUi();
    updateProfileUi();
    updateBattlePassUi();
}

void MainWindow::handleLogin()
{
    if (m_apiClient == nullptr) {
        return;
    }

    const QString email = m_emailEdit != nullptr ? m_emailEdit->text().trimmed() : QString();
    const QString password = m_passwordEdit != nullptr ? m_passwordEdit->text() : QString();

    if (email.isEmpty() || password.isEmpty()) {
        QMessageBox::information(
            this,
            QStringLiteral("Missing details"),
            QStringLiteral("Enter both your email and password to sign in.")
        );
        return;
    }

    if (m_profileStatusLabel != nullptr) {
        m_profileStatusLabel->setText(QStringLiteral("Signing in..."));
    }

    QNetworkReply *reply = m_apiClient->login(email, password);

    connect(reply, &QNetworkReply::finished, this, [this, reply, email] {
        const QByteArray payload = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            m_lastSyncMessage = QStringLiteral("Sign-in failed: %1").arg(extractReplyError(reply, payload));
            statusBar()->showMessage(m_lastSyncMessage, 5000);
            updateProfileUi();
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            m_lastSyncMessage = QStringLiteral("Sign-in failed: invalid server response.");
            statusBar()->showMessage(m_lastSyncMessage, 5000);
            updateProfileUi();
            reply->deleteLater();
            return;
        }

        const QJsonObject root = document.object();
        const QJsonObject session = root.value(QStringLiteral("session")).toObject();
        const QString accessToken = session.value(QStringLiteral("accessToken")).toString(
            session.value(QStringLiteral("access_token")).toString()
        );

        if (accessToken.trimmed().isEmpty()) {
            m_lastSyncMessage = QStringLiteral("Sign-in failed: no access token was returned.");
            statusBar()->showMessage(m_lastSyncMessage, 5000);
            updateProfileUi();
            reply->deleteLater();
            return;
        }

        const QJsonObject userObject = root.value(QStringLiteral("user")).toObject();
        const QJsonObject metadata = userObject.value(QStringLiteral("user_metadata")).toObject();

        m_accessToken = accessToken;
        m_playerProfile.signedIn = true;
        m_playerProfile.email = userObject.value(QStringLiteral("email")).toString(email);
        m_playerProfile.displayName = metadata.value(QStringLiteral("display_name")).toString();
        if (m_playerProfile.displayName.trimmed().isEmpty()) {
            m_playerProfile.displayName = m_playerProfile.email.section(QLatin1Char('@'), 0, 0);
        }
        m_playerProfile.username = metadata.value(QStringLiteral("username")).toString();
        m_playerProfile.lastSyncedAt = QDateTime::currentDateTime();
        m_lastSyncMessage = QStringLiteral("Signed in. Syncing dashboard data...");

        if (m_passwordEdit != nullptr) {
            m_passwordEdit->clear();
        }

        updateProfileUi();
        showMainShell(DashboardPage);
        refreshRemoteStats();
        refreshLeaderboard();

        if (m_hasPendingUpload) {
            uploadLastResult();
        }

        reply->deleteLater();
    });
}

void MainWindow::handleSignup()
{
    const QUrl signupUrl(QStringLiteral("https://quizforge.chococookie.org/signup"));

    // Account creation is delegated to the live website so the app stays aligned with the web auth flow.
    statusBar()->showMessage(QStringLiteral("Opening the QuizForge account creation page..."), 3000);

    if (!QDesktopServices::openUrl(signupUrl)) {
        statusBar()->showMessage(
            QStringLiteral("Open this page to create an account: %1").arg(signupUrl.toString()),
            7000
        );
    }
}

void MainWindow::handleLogout()
{
    m_accessToken.clear();
    m_playerProfile.signedIn = false;
    m_playerProfile.email.clear();
    m_playerProfile.lastSyncedAt = {};
    m_playerProfile.equippedProfilePicture = QStringLiteral("starter-badge");
    m_playerProfile.equippedAvatarFrame = QStringLiteral("clean-frame");
    m_playerProfile.equippedTitle = QStringLiteral("quiz-player");
    m_playerProfile.equippedTitleName = QStringLiteral("Quiz Player");
    m_playerProfile.equippedHoverAnimation = QStringLiteral("soft-lift");
    m_playerProfile.cosmeticCount = 0;
    m_lastSyncMessage = QStringLiteral("Guest mode");

    if (m_quizUploadStatusLabel != nullptr && m_hasPendingUpload) {
        m_quizUploadStatusLabel->setText(
            QStringLiteral("This run is stored locally. Sign in again to upload it.")
        );
    }

    updateDashboardUi();
    updateProfileUi();
    showLandingPage();
}

void MainWindow::continueAsGuest()
{
    m_accessToken.clear();
    m_playerProfile.signedIn = false;
    m_playerProfile.email.clear();
    m_playerProfile.lastSyncedAt = {};
    m_playerProfile.equippedProfilePicture = QStringLiteral("starter-badge");
    m_playerProfile.equippedAvatarFrame = QStringLiteral("clean-frame");
    m_playerProfile.equippedTitle = QStringLiteral("quiz-player");
    m_playerProfile.equippedTitleName = QStringLiteral("Quiz Player");
    m_playerProfile.equippedHoverAnimation = QStringLiteral("soft-lift");
    m_playerProfile.cosmeticCount = 0;
    m_lastSyncMessage = QStringLiteral("Guest mode");

    updateDashboardUi();
    updateProfileUi();
    showMainShell(DashboardPage);
}

// Remote sync, community content, and uploads
void MainWindow::refreshRemoteStats()
{
    if (m_apiClient == nullptr) {
        return;
    }

    if (m_accessToken.trimmed().isEmpty()) {
        m_lastSyncMessage = QStringLiteral("Guest mode. Sign in to pull synced stats.");
        updateProfileUi();
        return;
    }

    m_profileStatusLabel->setText(QStringLiteral("Refreshing synced stats..."));
    QNetworkReply *reply = m_apiClient->fetchPlayerStats(m_accessToken);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const QByteArray payload = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            m_lastSyncMessage = QStringLiteral("Stats refresh failed: %1").arg(extractReplyError(reply, payload));
            updateProfileUi();
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            m_lastSyncMessage = QStringLiteral("Stats refresh failed: invalid server response.");
            updateProfileUi();
            reply->deleteLater();
            return;
        }

        const QJsonObject root = document.object();
        const bool isPreview = valueToBool(root.value(QStringLiteral("isPreview")), false);

        const QString playerName = root.value(QStringLiteral("playerName")).toString();
        if (!playerName.trimmed().isEmpty()) {
            m_playerProfile.displayName = playerName;
        }

        m_playerProfile.level = std::max(valueToInt(root.value(QStringLiteral("level")), m_playerProfile.level), 1);
        m_playerProfile.totalXp = std::max(valueToInt(root.value(QStringLiteral("totalXp")), m_playerProfile.totalXp), 0);
        m_playerProfile.totalScore = std::max(valueToInt(root.value(QStringLiteral("totalScore")), m_playerProfile.totalScore), 0);
        m_playerProfile.quizzesCompleted = std::max(valueToInt(root.value(QStringLiteral("quizzesCompleted")), m_playerProfile.quizzesCompleted), 0);
        m_playerProfile.accuracy = std::clamp(valueToInt(root.value(QStringLiteral("accuracy")), m_playerProfile.accuracy), 0, 100);
        m_playerProfile.currentStreak = std::max(valueToInt(root.value(QStringLiteral("streak")), m_playerProfile.currentStreak), 0);
        m_playerProfile.signedIn = valueToBool(root.value(QStringLiteral("signedIn")), true);

        const QJsonObject season = root.value(QStringLiteral("season")).toObject();
        if (!season.isEmpty()) {
            const QString seasonName = season.value(QStringLiteral("name")).toString();
            if (!seasonName.trimmed().isEmpty()) {
                m_seasonName = seasonName;
            }

            m_daysLeftInSeason = std::max(valueToInt(season.value(QStringLiteral("daysLeft")), m_daysLeftInSeason), 0);
            m_playerProfile.battlePassTier = std::max(valueToInt(season.value(QStringLiteral("currentTier")), m_playerProfile.battlePassTier), 1);
            m_playerProfile.battlePassXp = std::max(valueToInt(season.value(QStringLiteral("currentXp")), m_playerProfile.battlePassXp), 0);
            m_playerProfile.nextBattlePassXp = std::max(valueToInt(season.value(QStringLiteral("nextTierXp")), m_playerProfile.nextBattlePassXp), 200);
        } else {
            m_playerProfile.level = calculateLevelFromXp(m_playerProfile.totalXp);
            m_playerProfile.battlePassXp = std::max(m_playerProfile.battlePassXp, m_playerProfile.totalXp);
            m_playerProfile.battlePassTier = std::max((m_playerProfile.battlePassXp / 200) + 1, 1);
            m_playerProfile.nextBattlePassXp = std::max(m_playerProfile.battlePassTier * 200, 200);
        }

        const QJsonObject customization = root.value(QStringLiteral("customization")).toObject();
        if (!customization.isEmpty()) {
            const QJsonObject equipped = customization.value(QStringLiteral("equipped")).toObject();
            m_playerProfile.equippedProfilePicture = equipped
                .value(QStringLiteral("profile_picture"))
                .toString(m_playerProfile.equippedProfilePicture);
            m_playerProfile.equippedAvatarFrame = equipped
                .value(QStringLiteral("avatar_frame"))
                .toString(m_playerProfile.equippedAvatarFrame);
            m_playerProfile.equippedTitle = equipped
                .value(QStringLiteral("title"))
                .toString(m_playerProfile.equippedTitle);
            m_playerProfile.equippedHoverAnimation = equipped
                .value(QStringLiteral("hover_animation"))
                .toString(m_playerProfile.equippedHoverAnimation);

            const QJsonArray inventory = customization.value(QStringLiteral("inventory")).toArray();
            m_playerProfile.cosmeticCount = inventory.size();
            for (const auto &itemValue : inventory) {
                const QJsonObject item = itemValue.toObject();
                if (item.value(QStringLiteral("key")).toString() == m_playerProfile.equippedTitle) {
                    const QString titleName = item.value(QStringLiteral("name")).toString();
                    if (!titleName.trimmed().isEmpty()) {
                        m_playerProfile.equippedTitleName = titleName;
                    }
                    break;
                }
            }
        }

        if (!m_hasPendingUpload) {
            m_recentRuns.clear();

            const QJsonArray recentResults = root.value(QStringLiteral("recentResults")).toArray();
            for (const auto &resultValue : recentResults) {
                const QJsonObject result = resultValue.toObject();
                RecentRun run;
                run.category = result.value(QStringLiteral("category")).toString(QStringLiteral("General"));
                run.difficulty = QStringLiteral("synced");
                run.score = std::max(valueToInt(result.value(QStringLiteral("score"))), 0);
                run.xpEarned = std::max(valueToInt(result.value(QStringLiteral("xpEarned"))), 0);
                run.accuracy = std::clamp(valueToInt(result.value(QStringLiteral("accuracy"))), 0, 100);
                run.durationMs = 0;
                run.uploaded = true;
                run.playedAtText = formatIsoDateTime(result.value(QStringLiteral("playedAt")).toString());
                m_recentRuns.push_back(run);
            }
        }

        m_playerProfile.lastSyncedAt = QDateTime::currentDateTime();
        m_lastSyncMessage = isPreview
                                ? QStringLiteral("Sample stats loaded while live sync finishes.")
                                : QStringLiteral("Stats synced successfully.");

        updateDashboardUi();
        updateProfileUi();
        updateBattlePassUi();
        reply->deleteLater();
    });
}

void MainWindow::refreshCommunityQuizzes()
{
    if (m_apiClient == nullptr || m_communityQuizList == nullptr || m_communityStatusLabel == nullptr) {
        return;
    }

    m_communityStatusLabel->setText(QStringLiteral("Refreshing shared quizzes..."));
    QNetworkReply *reply = m_apiClient->fetchCommunityQuizzes();

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const QByteArray payload = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            m_communityStatusLabel->setText(
                QStringLiteral("Could not load shared quizzes: %1")
                    .arg(extractReplyError(reply, payload))
            );
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            m_communityStatusLabel->setText(QStringLiteral("Could not load shared quizzes right now."));
            reply->deleteLater();
            return;
        }

        const QJsonObject root = document.object();
        const bool isPreview = valueToBool(root.value(QStringLiteral("isPreview")), false);
        const QJsonArray quizzes = root.value(QStringLiteral("quizzes")).toArray();
        const QString updatedAt = formatIsoDateTime(root.value(QStringLiteral("updatedAt")).toString());

        m_communityQuizzes.clear();
        m_communityQuizList->clear();

        for (const auto &quizValue : quizzes) {
            const QJsonObject quizObject = quizValue.toObject();
            CommunityQuizSummary quiz;
            quiz.slug = quizObject.value(QStringLiteral("slug")).toString();
            quiz.title = quizObject.value(QStringLiteral("title")).toString(QStringLiteral("Shared quiz"));
            quiz.description = quizObject.value(QStringLiteral("description")).toString();
            quiz.category = quizObject.value(QStringLiteral("category")).toString(QStringLiteral("Community"));
            quiz.difficulty = quizObject.value(QStringLiteral("difficulty")).toString(QStringLiteral("medium"));
            quiz.authorName = quizObject.value(QStringLiteral("authorName")).toString();
            quiz.questionCount = std::max(valueToInt(quizObject.value(QStringLiteral("questionCount"))), 0);
            quiz.createdAtText = formatIsoDateTime(quizObject.value(QStringLiteral("createdAt")).toString());
            quiz.sharePath = quizObject.value(QStringLiteral("sharePath")).toString(
                QStringLiteral("/community-quizzes/%1").arg(quiz.slug)
            );

            if (quiz.slug.trimmed().isEmpty()) {
                continue;
            }

            m_communityQuizzes.push_back(quiz);

            auto *item = new QListWidgetItem(
                QStringLiteral("%1\n%2  |  %3 questions  |  %4")
                    .arg(quiz.title)
                    .arg(quiz.category)
                    .arg(formatNumber(quiz.questionCount))
                    .arg(quiz.authorName.trimmed().isEmpty()
                             ? QStringLiteral("Community share")
                             : QStringLiteral("By %1").arg(quiz.authorName))
            );
            item->setData(Qt::UserRole, quiz.slug);
            m_communityQuizList->addItem(item);
        }

        if (m_communityQuizzes.isEmpty()) {
            m_activeCommunityQuiz = {};
            m_activeCommunityQuestions.clear();
            updateCommunityQuizUi();
            m_communityStatusLabel->setText(
                isPreview
                    ? QStringLiteral("Sample community quizzes are not available yet.")
                    : QStringLiteral("No shared quizzes have been published yet.")
            );
            reply->deleteLater();
            return;
        }

        QString slugToSelect = m_activeCommunityQuiz.slug;
        if (slugToSelect.trimmed().isEmpty()) {
            slugToSelect = m_communityQuizzes.first().slug;
        }

        int selectedRow = 0;
        for (int index = 0; index < m_communityQuizList->count(); ++index) {
            if (m_communityQuizList->item(index)->data(Qt::UserRole).toString() == slugToSelect) {
                selectedRow = index;
                break;
            }
        }

        m_communityQuizList->setCurrentRow(selectedRow);
        m_communityStatusLabel->setText(
            isPreview
                ? QStringLiteral("Sample shared quizzes loaded at %1.").arg(updatedAt)
                : QStringLiteral("Shared quizzes updated at %1.").arg(updatedAt)
        );
        reply->deleteLater();
    });
}

void MainWindow::loadCommunityQuiz(const QString &slug, bool startImmediately)
{
    if (m_apiClient == nullptr || m_communityStatusLabel == nullptr) {
        return;
    }

    const QString trimmedSlug = slug.trimmed();
    if (trimmedSlug.isEmpty()) {
        m_communityStatusLabel->setText(QStringLiteral("Enter or select a share code first."));
        return;
    }

    if (m_communityQuizSlugEdit != nullptr && m_communityQuizSlugEdit->text().trimmed() != trimmedSlug) {
        m_communityQuizSlugEdit->setText(trimmedSlug);
    }

    m_communityStatusLabel->setText(QStringLiteral("Opening shared quiz..."));
    QNetworkReply *reply = m_apiClient->fetchCommunityQuiz(trimmedSlug);

    connect(reply, &QNetworkReply::finished, this, [this, reply, trimmedSlug, startImmediately] {
        const QByteArray payload = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            m_communityStatusLabel->setText(
                QStringLiteral("Could not open that quiz: %1")
                    .arg(extractReplyError(reply, payload))
            );
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            m_communityStatusLabel->setText(QStringLiteral("That share code did not return a valid quiz."));
            reply->deleteLater();
            return;
        }

        const QJsonObject root = document.object();
        const bool isPreview = valueToBool(root.value(QStringLiteral("isPreview")), false);
        const QJsonObject quizObject = root.value(QStringLiteral("quiz")).toObject();
        const QJsonArray questionArray = quizObject.value(QStringLiteral("questions")).toArray();

        QVector<QuizQuestion> questions;
        questions.reserve(questionArray.size());

        for (const auto &questionValue : questionArray) {
            const QJsonObject questionObject = questionValue.toObject();
            QuizQuestion question;
            question.id = questionObject.value(QStringLiteral("id")).toString();
            question.category = quizObject.value(QStringLiteral("category")).toString(QStringLiteral("Community"));
            question.difficulty = quizObject.value(QStringLiteral("difficulty")).toString(QStringLiteral("medium"));
            question.question = questionObject.value(QStringLiteral("prompt")).toString();

            const QJsonArray answers = questionObject.value(QStringLiteral("answers")).toArray();
            for (const auto &answer : answers) {
                question.answers.push_back(answer.toString());
            }

            question.correctIndex = valueToInt(questionObject.value(QStringLiteral("correctIndex")), -1);
            question.explanation = questionObject.value(QStringLiteral("explanation")).toString();

            if (question.isValid()) {
                questions.push_back(question);
            }
        }

        if (questions.isEmpty()) {
            m_communityStatusLabel->setText(QStringLiteral("That quiz is not ready to play yet."));
            reply->deleteLater();
            return;
        }

        m_activeCommunityQuiz = {};
        m_activeCommunityQuiz.slug = quizObject.value(QStringLiteral("slug")).toString(trimmedSlug);
        m_activeCommunityQuiz.title = quizObject.value(QStringLiteral("title")).toString(QStringLiteral("Shared quiz"));
        m_activeCommunityQuiz.description = quizObject.value(QStringLiteral("description")).toString();
        m_activeCommunityQuiz.category = quizObject.value(QStringLiteral("category")).toString(QStringLiteral("Community"));
        m_activeCommunityQuiz.difficulty = quizObject.value(QStringLiteral("difficulty")).toString(QStringLiteral("medium"));
        m_activeCommunityQuiz.authorName = quizObject.value(QStringLiteral("authorName")).toString();
        m_activeCommunityQuiz.questionCount = std::max(valueToInt(quizObject.value(QStringLiteral("questionCount"))), static_cast<int>(questions.size()));
        m_activeCommunityQuiz.createdAtText = formatIsoDateTime(quizObject.value(QStringLiteral("createdAt")).toString());
        m_activeCommunityQuiz.sharePath = quizObject.value(QStringLiteral("sharePath")).toString(
            QStringLiteral("/community-quizzes/%1").arg(trimmedSlug)
        );
        m_activeCommunityQuestions = questions;
        updateCommunityQuizUi();

        if (m_communityQuizList != nullptr) {
            for (int index = 0; index < m_communityQuizList->count(); ++index) {
                auto *item = m_communityQuizList->item(index);
                if (item != nullptr && item->data(Qt::UserRole).toString() == trimmedSlug) {
                    const bool blocked = m_communityQuizList->blockSignals(true);
                    m_communityQuizList->setCurrentRow(index);
                    m_communityQuizList->blockSignals(blocked);
                    break;
                }
            }
        }

        m_communityStatusLabel->setText(
            isPreview
                ? QStringLiteral("Sample shared quiz loaded.")
                : QStringLiteral("\"%1\" is ready to play.").arg(m_activeCommunityQuiz.title)
        );

        if (startImmediately) {
            startQuizSession(
                m_activeCommunityQuestions,
                m_activeCommunityQuiz.category,
                m_activeCommunityQuiz.difficulty,
                QStringLiteral("community"),
                m_activeCommunityQuiz.title
            );
        }

        reply->deleteLater();
    });
}

void MainWindow::refreshLeaderboard()
{
    if (m_apiClient == nullptr || m_leaderboardTable == nullptr) {
        return;
    }

    updateLeaderboardStatus(QStringLiteral("Refreshing leaderboard..."));
    QNetworkReply *reply = m_apiClient->fetchLeaderboard();

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const QByteArray payload = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            updateLeaderboardStatus(
                QStringLiteral("Leaderboard refresh failed: %1").arg(extractReplyError(reply, payload))
            );
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            updateLeaderboardStatus(QStringLiteral("Leaderboard refresh failed: invalid server response."));
            reply->deleteLater();
            return;
        }

        const QJsonObject root = document.object();
        const bool isPreview = valueToBool(root.value(QStringLiteral("isPreview")), false);
        const QJsonArray entries = root.value(QStringLiteral("entries")).toArray();
        const QString updatedAt = formatIsoDateTime(root.value(QStringLiteral("updatedAt")).toString());

        m_leaderboardTable->setRowCount(entries.size());

        for (int row = 0; row < entries.size(); ++row) {
            const QJsonObject entry = entries.at(row).toObject();
            const int rank = std::max(valueToInt(entry.value(QStringLiteral("rank")), row + 1), 1);
            const QString username = entry.value(QStringLiteral("username")).toString(
                QStringLiteral("Player %1").arg(row + 1)
            );
            const QString category = entry.value(QStringLiteral("category")).toString(QStringLiteral("General"));
            const int xp = std::max(valueToInt(entry.value(QStringLiteral("xp"))), 0);
            const int score = std::max(valueToInt(entry.value(QStringLiteral("score"))), 0);

            auto *rankItem = new QTableWidgetItem(QString::number(rank));
            rankItem->setTextAlignment(Qt::AlignCenter);
            auto *playerItem = new QTableWidgetItem(username);
            auto *categoryItem = new QTableWidgetItem(category);
            auto *xpItem = new QTableWidgetItem(formatNumber(xp));
            xpItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            auto *scoreItem = new QTableWidgetItem(formatNumber(score));
            scoreItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

            m_leaderboardTable->setItem(row, 0, rankItem);
            m_leaderboardTable->setItem(row, 1, playerItem);
            m_leaderboardTable->setItem(row, 2, categoryItem);
            m_leaderboardTable->setItem(row, 3, xpItem);
            m_leaderboardTable->setItem(row, 4, scoreItem);
        }

        updateLeaderboardStatus(
            isPreview
                ? QStringLiteral("Sample leaderboard loaded at %1.").arg(updatedAt)
                : QStringLiteral("Leaderboard synced at %1.").arg(updatedAt)
        );
        reply->deleteLater();
    });
}

void MainWindow::uploadLastResult()
{
    if (m_apiClient == nullptr || !m_hasPendingUpload) {
        if (m_quizUploadStatusLabel != nullptr) {
            m_quizUploadStatusLabel->setText(QStringLiteral("No pending result to upload."));
        }
        return;
    }

    if (m_accessToken.trimmed().isEmpty()) {
        m_quizUploadStatusLabel->setText(QStringLiteral("Sign in first if you want this result to sync to the website."));
        m_quizUploadButton->setEnabled(false);
        return;
    }

    m_quizUploadStatusLabel->setText(QStringLiteral("Uploading result..."));
    m_quizUploadButton->setEnabled(false);

    QNetworkReply *reply = m_apiClient->uploadQuizResult(m_lastUploadPayload, m_accessToken);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const QByteArray payload = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            m_quizUploadStatusLabel->setText(
                QStringLiteral("Upload failed: %1").arg(extractReplyError(reply, payload))
            );
            m_quizUploadButton->setEnabled(true);
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            m_quizUploadStatusLabel->setText(QStringLiteral("Upload failed: invalid server response."));
            m_quizUploadButton->setEnabled(true);
            reply->deleteLater();
            return;
        }

        const QJsonObject root = document.object();
        const QString responseError = root.value(QStringLiteral("error")).toString();
        if (!responseError.trimmed().isEmpty()) {
            m_quizUploadStatusLabel->setText(QStringLiteral("Upload failed: %1").arg(responseError));
            m_quizUploadButton->setEnabled(true);
            reply->deleteLater();
            return;
        }

        const QString mode = root.value(QStringLiteral("mode")).toString();
        m_hasPendingUpload = false;

        if (!m_recentRuns.isEmpty()) {
            m_recentRuns[0].uploaded = true;
        }

        m_quizUploadStatusLabel->setText(
            mode == QStringLiteral("preview")
                ? QStringLiteral("Result checked using sample website data.")
                : QStringLiteral("Result uploaded successfully.")
        );
        m_quizUploadButton->setEnabled(false);
        m_lastSyncMessage = QStringLiteral("Latest run uploaded.");

        updateRecentRunsUi();
        refreshRemoteStats();
        refreshLeaderboard();

        reply->deleteLater();
    });
}

int MainWindow::currentBattlePassFloorXp() const
{
    return std::max((std::max(m_playerProfile.battlePassTier, 1) - 1) * 200, 0);
}

// Battle pass progression math helpers
int MainWindow::computedNextBattlePassXp() const
{
    return std::max(
        m_playerProfile.nextBattlePassXp,
        std::max(std::max(m_playerProfile.battlePassTier, 1) * 200, 200)
    );
}
