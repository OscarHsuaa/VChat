cmake_minimum_required(VERSION 3.19)
project(VChatStreamingTests LANGUAGES CXX)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network Widgets Svg)
qt_standard_project_setup()
set(CMAKE_AUTORCC ON)
qt_add_executable(VChatStreamingTest stream_test.cpp
    ../ChatServer/aiservice.cpp ../ChatServer/aiservice.h
    ../ChatServer/airoom.cpp ../ChatServer/airoom.h)
target_include_directories(VChatStreamingTest PRIVATE ../ChatServer)
target_link_libraries(VChatStreamingTest PRIVATE Qt::Core Qt::Network)
qt_add_executable(VChatConcurrentAiTest concurrent_ai_test.cpp
    ../ChatServer/aiservice.cpp ../ChatServer/aiservice.h
    ../ChatServer/airoom.cpp ../ChatServer/airoom.h)
target_include_directories(VChatConcurrentAiTest PRIVATE ../ChatServer)
target_link_libraries(VChatConcurrentAiTest PRIVATE Qt::Core Qt::Network)
qt_add_executable(VChatStreamUiTest ui_stream_test.cpp
    ../ChatDemo_1/mainwindow.cpp ../ChatDemo_1/mainwindow.h ../ChatDemo_1/mainwindow.ui
    ../ChatDemo_1/aimarkdownwidget.cpp ../ChatDemo_1/aimarkdownwidget.h
    ../ChatDemo_1/mentioninput.cpp ../ChatDemo_1/mentioninput.h
    ../ChatDemo_1/roomsidebarwidget.cpp ../ChatDemo_1/roomsidebarwidget.h
    ../ChatDemo_1/roomcreatedialog.cpp ../ChatDemo_1/roomcreatedialog.h
    ../ChatDemo_1/roominvitedialog.cpp ../ChatDemo_1/roominvitedialog.h
    ../ChatDemo_1/roommanagementdialog.cpp ../ChatDemo_1/roommanagementdialog.h
    ../ChatDemo_1/login_icons.qrc)
target_include_directories(VChatStreamUiTest PRIVATE ../ChatDemo_1)
target_link_libraries(VChatStreamUiTest PRIVATE Qt::Core Qt::Network Qt::Widgets Qt::Svg)
qt_add_executable(VChatRoomUiIntegrationTest rooms_client_live_test.cpp
    ../ChatDemo_1/mainwindow.cpp ../ChatDemo_1/mainwindow.h ../ChatDemo_1/mainwindow.ui
    ../ChatDemo_1/aimarkdownwidget.cpp ../ChatDemo_1/aimarkdownwidget.h
    ../ChatDemo_1/mentioninput.cpp ../ChatDemo_1/mentioninput.h
    ../ChatDemo_1/roomsidebarwidget.cpp ../ChatDemo_1/roomsidebarwidget.h
    ../ChatDemo_1/roomcreatedialog.cpp ../ChatDemo_1/roomcreatedialog.h
    ../ChatDemo_1/roominvitedialog.cpp ../ChatDemo_1/roominvitedialog.h
    ../ChatDemo_1/roommanagementdialog.cpp ../ChatDemo_1/roommanagementdialog.h
    ../ChatDemo_1/login_icons.qrc)
target_include_directories(VChatRoomUiIntegrationTest PRIVATE ../ChatDemo_1)
target_link_libraries(VChatRoomUiIntegrationTest PRIVATE Qt::Core Qt::Network Qt::Widgets Qt::Svg)
qt_add_executable(VChatServerE2eTest server_e2e_test.cpp)
target_link_libraries(VChatServerE2eTest PRIVATE Qt::Core Qt::Network)
