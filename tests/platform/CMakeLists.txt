file(GLOB_RECURSE SOURCES "*.cpp")

add_executable(platform_tests ${SOURCES})

target_link_libraries(platform_tests
    PRIVATE
        Qt6::Core
        gtest
        gtest_main
        app_platform
)

add_test(NAME platform_tests COMMAND platform_tests)
