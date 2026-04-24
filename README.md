# Quiz App

`Quiz App` is the desktop client for your quiz game, built with `C++`, `Qt 6`, and `CMake`.

The starter scaffold is set up for:

- Windows and macOS desktop builds
- a Qt Widgets based UI shell
- a future shared account flow with the website
- quiz, profile, leaderboard, battle pass, and settings screens
- API integration points for login, stats sync, and score uploads

## Why This Stack

- `Qt 6` gives you one desktop UI codebase for both Windows and macOS.
- `CMake` gives you one build system across both platforms.
- `Qt Network` is enough to integrate with your existing website endpoints.
- `Qt Widgets` is a good v1 choice because it is faster to build and easier to maintain than a more animated QML UI.

## Project Structure

```text
src/
  main.cpp
  ui/
    mainwindow.h
    mainwindow.cpp
  network/
    apiclient.h
    apiclient.cpp
  models/
    playerprofile.h
    quizresultpayload.h
  utils/
    appconfig.h
    appconfig.cpp
assets/
data/
  sample_questions.json
```

## Requirements

- `Qt 6.5+`
- `CMake 3.21+`
- a C++20 compiler

Recommended toolchains:

- Windows: `MSVC 2022`
- macOS: `Xcode / Apple Clang`

## Build Locally

### Windows

```powershell
cmake -S . -B build -D CMAKE_PREFIX_PATH="C:\Qt\6.8.0\msvc2022_64"
cmake --build build --config Release
```

### macOS

```bash
cmake -S . -B build -D CMAKE_PREFIX_PATH="$HOME/Qt/6.8.0/macos"
cmake --build build --config Release
```

If you use Qt Creator, you can open the folder directly as a CMake project.

## Run

From the build output:

- Windows: `build/bin/QuizApp.exe`
- macOS: `build/bin/QuizApp.app`

## Website Integration Targets

This app is scaffolded to talk to the website you already built:

- `POST /api/auth/login`
- `POST /api/auth/signup`
- `GET /api/me/stats`
- `GET /api/leaderboard`
- `POST /api/game/upload-result`

The default API base URL is `http://localhost:3000`.

## Packaging Later

Once the app is compiling, you can package it with:

- `windeployqt` on Windows
- `macdeployqt` on macOS

Official Qt deployment docs:

- [Windows deployment](https://doc.qt.io/qt-6/windows-deployment.html)
- [macOS deployment](https://doc.qt.io/qt-6/macos-deployment.html)

## Recommended Next Build Steps

1. Replace placeholder pages with the real quiz loop.
2. Add login/signup dialogs that call the existing website API.
3. Add question loading and answer checking.
4. Upload completed quiz results to the website.
5. Pull leaderboard and battle pass data into the desktop UI.

