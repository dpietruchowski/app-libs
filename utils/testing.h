#pragma once

#ifdef LIBS_TESTING
#define LIBS_TEST_FRIEND(Fixture) friend class Fixture;
#else
#define LIBS_TEST_FRIEND(Fixture)
#endif
