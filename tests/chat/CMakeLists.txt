file(GLOB_RECURSE CHAT_TEST_SOURCES "*.cpp")
file(GLOB_RECURSE CHAT_TEST_HEADERS "*.h")

add_executable(chat_tests ${CHAT_TEST_SOURCES} ${CHAT_TEST_HEADERS})

target_link_libraries(chat_tests PRIVATE
    Qt6::Core
    Qt6::Test
    gtest
    app_chat
)

add_test(NAME chat_tests COMMAND chat_tests)
