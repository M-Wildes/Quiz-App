#pragma once

#include <QElapsedTimer>
#include <QMainWindow>
#include <QVector>

#include "models/playerprofile.h"
#include "models/quizquestion.h"
#include "models/quizresultpayload.h"

class ApiClient;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QPropertyAnimation;
class QSpinBox;
class QStackedWidget;
class QTableWidget;
class QTimer;
class QWidget;

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(ApiClient *apiClient, QWidget *parent = nullptr);

private:
    enum PageIndex {
        DashboardPage = 0,
        QuizPage,
        CommunityPage,
        ProfilePage,
        LeaderboardsPage,
        BattlePassPage,
        SettingsPage
    };

    struct RecentRun
    {
        QString category;
        QString difficulty;
        int score = 0;
        int xpEarned = 0;
        int accuracy = 0;
        int durationMs = 0;
        bool uploaded = false;
        QString playedAtText;
    };

    struct QuizSession
    {
        QVector<QuizQuestion> questions;
        QString quizType;
        QString title;
        QString category;
        QString difficulty;
        int currentIndex = 0;
        int correctAnswers = 0;
        bool answeredCurrentQuestion = false;
        bool active = false;
    };

    struct CommunityQuizSummary
    {
        QString slug;
        QString title;
        QString description;
        QString category;
        QString difficulty;
        QString authorName;
        QString createdAtText;
        QString sharePath;
        int questionCount = 0;
    };

    [[nodiscard]] QWidget *createDashboardPage();
    [[nodiscard]] QWidget *createQuizPage();
    [[nodiscard]] QWidget *createCommunityPage();
    [[nodiscard]] QWidget *createLandingPage();
    [[nodiscard]] QWidget *createProfilePage();
    [[nodiscard]] QWidget *createLeaderboardsPage();
    [[nodiscard]] QWidget *createBattlePassPage();
    [[nodiscard]] QWidget *createSettingsPage();
    [[nodiscard]] QWidget *wrapInScrollArea(QWidget *content) const;
    [[nodiscard]] QWidget *createCard(
        const QString &eyebrow,
        const QString &title,
        const QString &body
    ) const;
    [[nodiscard]] QPushButton *createNavButton(const QString &text, int index);

    void showLandingPage();
    void showMainShell(int pageIndex = DashboardPage);
    void setCurrentPage(int index);
    void loadQuestions();
    void populateQuestionFilters();
    void startQuiz();
    void startQuizSession(
        const QVector<QuizQuestion> &questions,
        const QString &category,
        const QString &difficulty,
        const QString &quizType,
        const QString &title
    );
    void showCurrentQuestion();
    void handleAnswerSelected(int answerIndex);
    void advanceQuiz();
    void finishQuiz();
    void resetAnswerButtons();
    void saveApiBaseUrl();
    void updateApiStatus();
    void updateQuizTimer();
    void updateDashboardUi();
    void updateProfileUi();
    void updateBattlePassUi();
    void updateRecentRunsUi();
    void updateQuestionPoolStatus();
    void updateCommunityQuizUi();
    void updateLeaderboardStatus(const QString &message);
    void animateBattlePassProgress(int targetValue);
    void applyLocalQuizResult(const RecentRun &run);
    void handleLogin();
    void handleSignup();
    void handleLogout();
    void continueAsGuest();
    void refreshRemoteStats();
    void refreshCommunityQuizzes();
    void loadCommunityQuiz(const QString &slug, bool startImmediately = false);
    void refreshLeaderboard();
    void uploadLastResult();

    [[nodiscard]] int currentBattlePassFloorXp() const;
    [[nodiscard]] int computedNextBattlePassXp() const;

    ApiClient *m_apiClient = nullptr;
    QStackedWidget *m_rootStack = nullptr;
    QWidget *m_mainShellPage = nullptr;
    QStackedWidget *m_pageStack = nullptr;
    QLabel *m_pageTitleLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLineEdit *m_apiBaseUrlEdit = nullptr;
    QLabel *m_landingStatusLabel = nullptr;

    QVector<QuizQuestion> m_allQuestions;
    QVector<RecentRun> m_recentRuns;
    QVector<CommunityQuizSummary> m_communityQuizzes;
    QVector<QuizQuestion> m_activeCommunityQuestions;
    QuizSession m_quizSession;
    QuizResultPayload m_lastUploadPayload;
    bool m_hasPendingUpload = false;

    PlayerProfile m_playerProfile;
    QString m_accessToken;
    QString m_seasonName = QStringLiteral("Season 01: Opening Bell");
    int m_daysLeftInSeason = 30;
    QString m_lastSyncMessage = QStringLiteral("Guest mode");

    QElapsedTimer m_quizTimer;
    QTimer *m_quizUiTimer = nullptr;

    QLabel *m_dashboardWelcomeLabel = nullptr;
    QLabel *m_dashboardLevelValueLabel = nullptr;
    QLabel *m_dashboardXpValueLabel = nullptr;
    QLabel *m_dashboardScoreValueLabel = nullptr;
    QLabel *m_dashboardQuizCountValueLabel = nullptr;
    QLabel *m_dashboardSeasonValueLabel = nullptr;
    QListWidget *m_dashboardRecentRunsList = nullptr;

    QComboBox *m_categoryCombo = nullptr;
    QComboBox *m_difficultyCombo = nullptr;
    QSpinBox *m_questionCountSpinBox = nullptr;
    QLabel *m_questionPoolStatusLabel = nullptr;
    QStackedWidget *m_quizStateStack = nullptr;
    QPushButton *m_startQuizButton = nullptr;
    QLabel *m_quizProgressLabel = nullptr;
    QLabel *m_quizTimerLabel = nullptr;
    QLabel *m_quizQuestionLabel = nullptr;
    QLabel *m_quizMetaLabel = nullptr;
    QLabel *m_quizFeedbackLabel = nullptr;
    QVector<QPushButton *> m_answerButtons;
    QPushButton *m_nextQuestionButton = nullptr;
    QLabel *m_quizResultHeadlineLabel = nullptr;
    QLabel *m_quizResultSummaryLabel = nullptr;
    QLabel *m_quizUploadStatusLabel = nullptr;
    QPushButton *m_quizUploadButton = nullptr;

    QLabel *m_communityStatusLabel = nullptr;
    QListWidget *m_communityQuizList = nullptr;
    QLabel *m_communityQuizTitleLabel = nullptr;
    QLabel *m_communityQuizMetaLabel = nullptr;
    QLabel *m_communityQuizDescriptionLabel = nullptr;
    QLabel *m_communityQuizShareLabel = nullptr;
    QLineEdit *m_communityQuizSlugEdit = nullptr;
    QPushButton *m_communityPlayButton = nullptr;
    CommunityQuizSummary m_activeCommunityQuiz;

    QLabel *m_profileStatusLabel = nullptr;
    QLabel *m_profileSummaryLabel = nullptr;
    QLabel *m_profileAuthHintLabel = nullptr;
    QLineEdit *m_emailEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLineEdit *m_displayNameEdit = nullptr;
    QLineEdit *m_usernameEdit = nullptr;

    QLabel *m_leaderboardStatusLabel = nullptr;
    QTableWidget *m_leaderboardTable = nullptr;

    QLabel *m_battlePassSeasonLabel = nullptr;
    QLabel *m_battlePassTierLabel = nullptr;
    QLabel *m_battlePassProgressLabel = nullptr;
    QProgressBar *m_battlePassProgressBar = nullptr;
    QListWidget *m_battlePassRewardsList = nullptr;
    QPropertyAnimation *m_battlePassProgressAnimation = nullptr;
};
