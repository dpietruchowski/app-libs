#pragma once

#ifdef UNIT_TEST
#include <gtest/gtest.h>
#define FRIEND_TEST_UNIT(test_suite_name, test_name) FRIEND_TEST(test_suite_name, test_name)
#else
#define FRIEND_TEST_UNIT(test_suite_name, test_name)
#endif
