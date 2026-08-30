file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.h")

add_executable(libs_utils_tests ${SOURCES} ${HEADERS})

target_link_libraries(libs_utils_tests
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
        gtest
        app_utils
)

add_test(NAME libs_utils_tests COMMAND libs_utils_tests)
