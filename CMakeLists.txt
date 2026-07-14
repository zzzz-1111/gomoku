cmake_minimum_required(VERSION 3.16)

project(gomoku VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Sql)

set(PROJECT_SOURCES
        src/app/main.cpp
        src/ai/ai_engine.cpp
        src/ai/board_evaluator.cpp
        src/ai/ai_turn_actor.cpp
        src/core/game_controller.cpp
        src/core/game_controller.h
        src/core/game_rule.cpp
        src/core/game_rule.h
        src/core/game_state.h
        src/core/turn_context.h
        src/core/turn_actor.h
        src/core/human_turn_actor.h
        src/data/database_manager.cpp
        src/data/database_manager.h
        src/data/models.h
        src/network/protocol.cpp
        src/network/network_manager.cpp
        src/network/protocol.h
        src/network/network_manager.h
        src/record/record_manager.cpp
        src/record/record_manager.h
        src/ui/history_dialog.cpp
        src/ui/history_dialog.h
        src/ui/replay_dialog.cpp
        src/ui/replay_dialog.h
        src/ui/chessboard_widget.cpp
        src/ui/chessboard_widget.h
        src/ui/match_intro_overlay.cpp
        src/ui/match_intro_overlay.h
        src/ui/mainwindow.cpp
        src/ui/mainwindow.h
        src/ui/mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(gomoku
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        res.qrc
    )
else()
    if(ANDROID)
        add_library(gomoku SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(gomoku
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(gomoku PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(gomoku PRIVATE Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Sql)

target_include_directories(gomoku PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.gomoku)
endif()
set_target_properties(gomoku PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS gomoku
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(gomoku)
endif()
