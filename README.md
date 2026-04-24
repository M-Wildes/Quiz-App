# Quiz App

`Quiz App` is the desktop client for `QuizForge`, built with `C++`, `Qt 6`, and `CMake`.

This repository contains the standalone Windows and macOS desktop experience for the quiz platform. The app is designed to complement the live website rather than mirror it exactly, with its own desktop-focused interface, local quiz flow, and direct integration with the live QuizForge backend.

## What The App Includes

- a Qt Widgets desktop UI
- local quiz gameplay using a bundled sample question bank
- account sign-in and sign-up hooks
- synced stats, leaderboard, and result upload support
- community quiz browsing and loading
- desktop-side progression features like XP, recent runs, and battle pass tracking

## Repo Layout

```text
src/    application source code
data/   sample quiz content used by the app
assets/ placeholder folder for desktop-specific assets
```

## Backend

By default, the app targets the live QuizForge backend at `https://quizforge.chococookie.org`.

If needed for testing, the backend URL can still be overridden through the app settings or the `QUIZFORGE_API_BASE_URL` environment variable.
