#pragma once

#ifdef UNIT_TEST
#include <gtest/gtest.h>
#define FRIEND_TEST_FIXTURE(test_fixture) friend class test_fixture
#else
#define FRIEND_TEST_FIXTURE(test_fixture)
#endif
