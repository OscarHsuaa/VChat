cmake_minimum_required(VERSION 3.19)
project(ChatServer LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network Sql)

qt_standard_project_setup()

qt_add_executable(ChatServer
    main.cpp
    aiservice.cpp
    aiservice.h
    airoom.cpp
    airoom.h
)

set_target_properties(ChatServer PROPERTIES AUTOMOC OFF)

target_link_libraries(ChatServer
    PRIVATE
        Qt::Core
        Qt::Network
        Qt::Sql
)

include(GNUInstallDirs)

install(TARGETS ChatServer
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
