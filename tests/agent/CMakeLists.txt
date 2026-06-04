file(GLOB_RECURSE AGENT_TEST_SOURCES "*.cpp")
file(GLOB_RECURSE AGENT_TEST_HEADERS "*.h")

add_executable(agent_tests ${AGENT_TEST_SOURCES} ${AGENT_TEST_HEADERS})

target_link_libraries(agent_tests PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
    gtest
    app_agent
)

add_test(NAME agent_tests COMMAND agent_tests)
