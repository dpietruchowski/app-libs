include_guard(GLOBAL)

function(app_load_env PATH)
    file(READ "${PATH}" CONTENT)
    string(REPLACE ";" "\\;" CONTENT "${CONTENT}")
    string(REPLACE "\n" ";" LINES "${CONTENT}")
    foreach(line IN LISTS LINES)
        if(line MATCHES "^([A-Za-z_][A-Za-z0-9_]*)=(.*)$")
            set(key "${CMAKE_MATCH_1}")
            set(value "${CMAKE_MATCH_2}")
            if(value MATCHES "^\"(.*)\"$")
                set(value "${CMAKE_MATCH_1}")
            endif()
            set(${key} "${value}" PARENT_SCOPE)
        endif()
    endforeach()
endfunction()

function(_app_add_format_targets)
    find_program(CLANG_FORMAT "clang-format")
    if(NOT CLANG_FORMAT)
        message(WARNING "Clang-format not found")
        return()
    endif()

    set(EXISTING_DIRS "")
    foreach(dir IN LISTS ARGN)
        if(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/${dir})
            list(APPEND EXISTING_DIRS ${dir})
        endif()
    endforeach()

    if(NOT EXISTING_DIRS)
        message(WARNING "Clang-format: none of the requested directories exist: ${ARGN}")
        return()
    endif()

    string(REPLACE ";" " " DIRS_STRING "${EXISTING_DIRS}")
    set(FORMAT_FIND_EXPR
        "${DIRS_STRING} -type d -name third_party -prune -o -type f '(' -name '*.cpp' -o -name '*.h' -o -name '*.hpp' ')'"
    )

    add_custom_target(format
        COMMAND sh -c "find ${FORMAT_FIND_EXPR} -exec '${CLANG_FORMAT}' -style=file -i {} +"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Formatting files in directories: ${DIRS_STRING}"
        VERBATIM
    )

    add_custom_target(format_check
        COMMAND sh -c "find ${FORMAT_FIND_EXPR} -exec '${CLANG_FORMAT}' -style=file --dry-run -Werror {} +"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Checking format in directories: ${DIRS_STRING}"
        VERBATIM
    )

    message(STATUS "Clang-format found: ${CLANG_FORMAT} for dirs: ${DIRS_STRING}")
endfunction()

macro(app_project_setup)
    cmake_parse_arguments(APP_SETUP "" "SOURCE_ROOT" "FORMAT_DIRS" ${ARGN})

    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(APP_SOURCE_ROOT "${APP_SETUP_SOURCE_ROOT}")

    if(ANDROID AND NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release)
    endif()

    add_compile_definitions(
        $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG_OUTPUT>
        $<$<NOT:$<CONFIG:Debug>>:QT_NO_INFO_OUTPUT>
    )

    if(NOT ANDROID AND BUILD_TESTING)
        enable_testing()
        set(LIBS_BUILD_TESTS ON CACHE BOOL "" FORCE)
        add_compile_definitions(LIBS_TESTING)
    endif()

    if(APP_SETUP_FORMAT_DIRS)
        _app_add_format_targets(${APP_SETUP_FORMAT_DIRS})
    endif()
endmacro()

macro(app_option_debug_default NAME DOC)
    set(_app_option_default OFF)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(_app_option_default ON)
    endif()
    option(${NAME} "${DOC}" ${_app_option_default})
    if(${NAME})
        add_compile_definitions(${NAME})
    endif()
endmacro()

function(app_add_module NAME)
    cmake_parse_arguments(ARG "LENIENT" "" "DEPS;EXCLUDE_REGEX" ${ARGN})

    file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
    file(GLOB_RECURSE HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
    foreach(pattern IN LISTS ARG_EXCLUDE_REGEX)
        list(FILTER SOURCES EXCLUDE REGEX "${pattern}")
        list(FILTER HEADERS EXCLUDE REGEX "${pattern}")
    endforeach()

    add_library(${NAME} STATIC ${SOURCES} ${HEADERS})

    set_target_properties(${NAME} PROPERTIES AUTOMOC ON)

    target_include_directories(${NAME} PUBLIC ${APP_SOURCE_ROOT})

    target_link_libraries(${NAME} PUBLIC ${ARG_DEPS})

    if(NOT ARG_LENIENT)
        target_compile_options(${NAME} PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
        )
    endif()
endfunction()

function(app_qml_live_map TARGET)
    if(NOT QML_LIVE_ENABLED)
        return()
    endif()

    list(LENGTH ARGN PAIR_COUNT)
    math(EXPR REMAINDER "${PAIR_COUNT} % 2")
    if(NOT REMAINDER EQUAL 0)
        message(FATAL_ERROR "app_qml_live_map expects URI/source-directory pairs")
    endif()

    set(SPEC "")
    math(EXPR LAST "${PAIR_COUNT} - 1")
    foreach(index RANGE 0 ${LAST} 2)
        list(GET ARGN ${index} URI)
        math(EXPR VALUE_INDEX "${index} + 1")
        list(GET ARGN ${VALUE_INDEX} SOURCE_DIR)
        if(NOT IS_DIRECTORY "${SOURCE_DIR}")
            message(FATAL_ERROR "app_qml_live_map: no such directory for ${URI}: ${SOURCE_DIR}")
        endif()
        string(APPEND SPEC "${URI}=${SOURCE_DIR}|")
    endforeach()

    target_compile_definitions(${TARGET} PRIVATE APP_QML_LIVE_MAP="${SPEC}")
    message(STATUS "QML live source map for ${TARGET}: ${SPEC}")
endfunction()

function(app_add_qml_module NAME)
    cmake_parse_arguments(ARG "" "URI" "EXCLUDE_REGEX" ${ARGN})

    file(GLOB_RECURSE QML_FILES_FULL "${CMAKE_CURRENT_SOURCE_DIR}/*.qml")
    set(QML_FILES "")
    foreach(qml_file ${QML_FILES_FULL})
        file(RELATIVE_PATH rel_path ${CMAKE_CURRENT_SOURCE_DIR} ${qml_file})
        list(APPEND QML_FILES ${rel_path})
    endforeach()
    foreach(pattern IN LISTS ARG_EXCLUDE_REGEX)
        list(FILTER QML_FILES EXCLUDE REGEX "${pattern}")
    endforeach()

    string(REPLACE "." "/" URI_PATH "${ARG_URI}")

    qt_add_qml_module(${NAME}
        URI ${ARG_URI}
        VERSION 1.0
        QML_FILES ${QML_FILES}
        OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${URI_PATH}
        NO_PLUGIN
    )

    target_link_libraries(${NAME}
        PUBLIC
            Qt6::Core
            Qt6::Quick
            Qt6::Qml
    )

    install(TARGETS ${NAME}
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )

    install(DIRECTORY ${CMAKE_BINARY_DIR}/${URI_PATH}
        DESTINATION lib/qt6/qml
        FILES_MATCHING PATTERN "*.qml" PATTERN "*.qmltypes" PATTERN "qmldir"
    )
endfunction()

function(app_add_test NAME)
    cmake_parse_arguments(ARG "OWN_MAIN" "" "SOURCES;DEPS;DEFINITIONS" ${ARGN})

    find_package(Qt6 REQUIRED COMPONENTS Core Test)

    if(NOT ARG_SOURCES)
        file(GLOB_RECURSE ARG_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
    endif()

    add_executable(${NAME} ${ARG_SOURCES})

    target_compile_definitions(${NAME} PRIVATE UNIT_TEST ${ARG_DEFINITIONS})

    target_include_directories(${NAME}
        PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    set(RUNNER gtest_main)
    if(ARG_OWN_MAIN)
        set(RUNNER "")
    endif()

    target_link_libraries(${NAME}
        PRIVATE
            ${ARG_DEPS}
            gtest
            gmock
            ${RUNNER}
            Qt6::Core
            Qt6::Test
    )

    include(GoogleTest)
    gtest_discover_tests(${NAME})
endfunction()

function(app_configure_android TARGET)
    cmake_parse_arguments(ARG ""
        "PACKAGE_NAME;VERSION_CODE;VERSION_NAME;KEYSTORE;KEY_ALIAS;PACKAGE_SOURCE_DIR;TARGET_SDK;OPENSSL_CMAKE"
        "" ${ARGN})

    if(NOT ARG_TARGET_SDK)
        set(ARG_TARGET_SDK 36)
    endif()

    set_target_properties(${TARGET} PROPERTIES
        QT_ANDROID_PACKAGE_NAME "${ARG_PACKAGE_NAME}"
        QT_ANDROID_VERSION_CODE "${ARG_VERSION_CODE}"
        QT_ANDROID_VERSION_NAME "${ARG_VERSION_NAME}"
        QT_ANDROID_TARGET_SDK_VERSION ${ARG_TARGET_SDK}
        QT_ANDROID_PACKAGE_SOURCE_DIR "${ARG_PACKAGE_SOURCE_DIR}"
        QT_ANDROID_KEYSTORE_PATH "${ARG_KEYSTORE}"
        QT_ANDROID_KEY_ALIAS "${ARG_KEY_ALIAS}"
    )

    if(ARG_OPENSSL_CMAKE)
        include(${ARG_OPENSSL_CMAKE})
        set_target_properties(OpenSSL::Crypto OpenSSL::SSL PROPERTIES IMPORTED_GLOBAL TRUE)
        add_android_openssl_libraries(${TARGET})
    endif()

    qt_android_generate_deployment_settings(${TARGET})
    qt_android_add_apk_target(${TARGET})
endfunction()
