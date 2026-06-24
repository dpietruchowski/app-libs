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

find_package(Qt6 REQUIRED COMPONENTS Core Quick Qml Sql)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_subdirectory(qmllive)
add_subdirectory(dbtoolkit)
add_subdirectory(eventbus)
add_subdirectory(utils)
add_subdirectory(icons)
add_subdirectory(agent)

if(NOT ANDROID)
    add_subdirectory(automation)
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
