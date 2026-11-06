file(GLOB_RECURSE AGENT_TEST_SOURCES "*.cpp")
list(FILTER AGENT_TEST_SOURCES EXCLUDE REGEX "/fakeclaude/")
file(GLOB_RECURSE AGENT_TEST_HEADERS "*.h")

add_executable(agent_tests ${AGENT_TEST_SOURCES} ${AGENT_TEST_HEADERS})

add_executable(fake_claude fakeclaude/main.cpp)
set_target_properties(fake_claude PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:agent_tests>"
)

add_dependencies(agent_tests fake_claude)

target_link_libraries(agent_tests PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
    gtest
    app_agent
)

add_test(NAME agent_tests COMMAND agent_tests)
