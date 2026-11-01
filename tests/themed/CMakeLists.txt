file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.h")

add_executable(themed_tests ${SOURCES} ${HEADERS})

target_compile_definitions(themed_tests
    PRIVATE
        QML_IMPORT_ROOT="${CMAKE_BINARY_DIR}"
)

target_include_directories(themed_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../qml/themed
)

target_link_libraries(themed_tests
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
        gtest
        app_themed_qml
)

add_test(NAME themed_tests COMMAND themed_tests)
