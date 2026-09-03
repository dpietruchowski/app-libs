cmake_minimum_required(VERSION 3.16)

project(__APP_ID__ VERSION 0.1 LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)

include(libs/cmake/AppProject.cmake)

app_load_env(${CMAKE_SOURCE_DIR}/app.env)

app_project_setup(
    SOURCE_ROOT ${CMAKE_SOURCE_DIR}/src
    FORMAT_DIRS src tests
)

add_subdirectory(libs)
add_subdirectory(src)

if(NOT ANDROID AND BUILD_TESTING AND EXISTS ${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt)
    add_subdirectory(tests)
endif()

if(ANDROID)
    set(ANDROID_PACKAGE_SOURCE "")
    if(EXISTS ${CMAKE_SOURCE_DIR}/android)
        set(ANDROID_PACKAGE_SOURCE PACKAGE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/android)
    endif()

    app_configure_android(${APP_TARGET}
        PACKAGE_NAME "${ANDROID_PACKAGE}"
        VERSION_CODE ${VERSION_CODE}
        VERSION_NAME "${VERSION_NAME}"
        KEYSTORE "${CMAKE_SOURCE_DIR}/${ANDROID_KEYSTORE}"
        KEY_ALIAS "${ANDROID_KEY_ALIAS}"
        ${ANDROID_PACKAGE_SOURCE}
    )
endif()
