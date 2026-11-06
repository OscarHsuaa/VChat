cmake_minimum_required(VERSION 3.19)
project(VChatPreview LANGUAGES CXX)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network Svg)
qt_standard_project_setup()
set(CMAKE_AUTORCC ON)
qt_add_executable(VChatPreview WIN32 preview.cpp mainwindow.cpp mainwindow.h mainwindow.ui login_icons.qrc)
target_link_libraries(VChatPreview PRIVATE Qt::Core Qt::Widgets Qt::Network Qt::Svg)

target_include_directories(VChatPreview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
