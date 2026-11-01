file(GLOB_RECURSE CHAT_CLAUDE_TEST_SOURCES "*.cpp")
file(GLOB_RECURSE CHAT_CLAUDE_TEST_HEADERS "*.h")

add_executable(chatclaude_tests ${CHAT_CLAUDE_TEST_SOURCES} ${CHAT_CLAUDE_TEST_HEADERS})

target_link_libraries(chatclaude_tests PRIVATE
    Qt6::Core
    Qt6::Test
    gtest
    app_chat_claude
)

add_test(NAME chatclaude_tests COMMAND chatclaude_tests)
