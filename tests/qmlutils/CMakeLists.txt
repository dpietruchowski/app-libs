file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.h")

add_executable(qmlutils_tests ${SOURCES} ${HEADERS})

target_link_libraries(qmlutils_tests
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
        gtest
        app_qmlutils
)

add_test(NAME qmlutils_tests COMMAND qmlutils_tests)
