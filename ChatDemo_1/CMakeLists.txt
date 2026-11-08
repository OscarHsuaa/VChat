cmake_minimum_required(VERSION 3.19)
project(ChatDemo_1 LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network Svg)

qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

qt_add_executable(ChatDemo_1
    WIN32 MACOSX_BUNDLE
    main.cpp
    logindialog.cpp
    logindialog.h
    logindialog.ui
    login_icons.qrc
    mainwindow.cpp
    mainwindow.h
    aimarkdownwidget.cpp
    aimarkdownwidget.h
    mentioninput.cpp
    mentioninput.h
    roomsidebarwidget.cpp
    roomsidebarwidget.h
    mainwindow.ui
)

target_link_libraries(ChatDemo_1
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
        Qt::Svg
)

# uic-generated code includes the promoted header wave widget from the source tree.
target_include_directories(ChatDemo_1 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

include(GNUInstallDirs)

install(TARGETS ChatDemo_1
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET ChatDemo_1
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
