file(GLOB_RECURSE ASYNC_TEST_SOURCES "*.cpp")

add_executable(async_tests ${ASYNC_TEST_SOURCES})

target_link_libraries(async_tests PRIVATE
    Qt6::Core
    gtest
    app_async
)

add_test(NAME async_tests COMMAND async_tests)
