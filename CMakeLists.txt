cmake_minimum_required(VERSION 3.21)

project(
    QuizApp
    VERSION 0.1.0
    DESCRIPTION "Cross-platform quiz game desktop app"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

qt_standard_project_setup()

qt_add_executable(QuizApp
    WIN32
    MACOSX_BUNDLE
    src/main.cpp
    src/models/playerprofile.h
    src/models/quizresultpayload.h
    src/models/quizquestion.h
    src/network/apiclient.cpp
    src/network/apiclient.h
    src/ui/mainwindow_functional.cpp
    src/ui/mainwindow.h
    src/utils/appconfig.cpp
    src/utils/appconfig.h
    src/utils/questionbank.cpp
    src/utils/questionbank.h
    assets/resources.qrc
    assets/app_icon.rc
)

target_include_directories(QuizApp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(QuizApp
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
)

target_compile_definitions(QuizApp PRIVATE
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
)

#  IMPORTANT: consistent output location across all platforms/configs
set_target_properties(QuizApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(QuizApp PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "QuizApp"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.quizforge.desktop"
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    WIN32_EXECUTABLE TRUE
)

install(TARGETS QuizApp
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
