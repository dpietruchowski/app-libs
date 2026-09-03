cmake_minimum_required(VERSION 3.16)

# When built on its own (not as a submodule of the app) become a standalone
# project so the libs and their tests can be configured in isolation.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(app_libs LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    option(LIBS_BUILD_TESTS "Build libs unit tests" ON)
else()
    option(LIBS_BUILD_TESTS "Build libs unit tests" OFF)
endif()

option(QML_LIVE_ENABLED "Enable QML hot-reload via qmllive" OFF)
if(ANDROID)
    set(QML_LIVE_ENABLED OFF)
endif()

set(LIBS_AUTOMATION_DEFAULT OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(LIBS_AUTOMATION_DEFAULT ON)
endif()
option(LIBS_AUTOMATION "Build the UI automation server" ${LIBS_AUTOMATION_DEFAULT})

find_package(Qt6 REQUIRED COMPONENTS Core Quick Qml Sql)
if(ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS CorePrivate)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_subdirectory(qmllive)
if(QML_LIVE_ENABLED)
    target_compile_definitions(qmllive INTERFACE QML_LIVE_ENABLED)
endif()
add_subdirectory(dbtoolkit)
add_subdirectory(eventbus)
add_subdirectory(utils)
add_subdirectory(icons)
add_subdirectory(agent)

if(LIBS_AUTOMATION)
    add_subdirectory(automation)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_subdirectory(crashhandler)
endif()

if(NOT QML_LIVE_ENABLED)
    add_subdirectory(qmlcomponents)
    add_subdirectory(appcomponents)
endif()

if(LIBS_BUILD_TESTS AND NOT ANDROID)
    if(NOT TARGET gtest)
        add_subdirectory(third_party/googletest)
    endif()
    enable_testing()
    add_subdirectory(tests)
endif()
