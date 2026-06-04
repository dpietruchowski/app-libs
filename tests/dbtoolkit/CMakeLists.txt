file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.h")

add_executable(dbtoolkit_tests ${SOURCES} ${HEADERS})

target_link_libraries(dbtoolkit_tests
    PRIVATE
        Qt6::Core
        Qt6::Sql
        Qt6::Test
        gtest
        gtest_main
        dbtoolkit
)

add_test(NAME dbtoolkit_tests COMMAND dbtoolkit_tests)
