set(LIBRARY_NAME app_crashhandler)

include(CheckCXXSourceCompiles)

set(STACKTRACE_TEST_SOURCE "
#include <stacktrace>
#include <string>
int main() { return std::to_string(std::stacktrace::current()).empty(); }
")

set(STACKTRACE_LIBRARY "")
set(SAVED_CXX_STANDARD ${CMAKE_CXX_STANDARD})
set(CMAKE_CXX_STANDARD 23)

set(CMAKE_REQUIRED_LIBRARIES stdc++exp)
check_cxx_source_compiles("${STACKTRACE_TEST_SOURCE}" HAVE_STACKTRACE_STDCXXEXP)
if(HAVE_STACKTRACE_STDCXXEXP)
    set(STACKTRACE_LIBRARY stdc++exp)
else()
    set(CMAKE_REQUIRED_LIBRARIES stdc++_libbacktrace)
    check_cxx_source_compiles("${STACKTRACE_TEST_SOURCE}" HAVE_STACKTRACE_LIBBACKTRACE)
    if(HAVE_STACKTRACE_LIBBACKTRACE)
        set(STACKTRACE_LIBRARY stdc++_libbacktrace)
    endif()
endif()

set(CMAKE_CXX_STANDARD ${SAVED_CXX_STANDARD})
unset(CMAKE_REQUIRED_LIBRARIES)

file(GLOB SOURCES "*.cpp")
file(GLOB HEADERS "*.h")

add_library(${LIBRARY_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${LIBRARY_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(${LIBRARY_NAME}
    PUBLIC
        Qt6::Core
        ${CMAKE_DL_LIBS}
)

if(STACKTRACE_LIBRARY)
    message(STATUS "Crash handler: std::stacktrace via ${STACKTRACE_LIBRARY}")
    target_compile_features(${LIBRARY_NAME} PRIVATE cxx_std_23)
    target_compile_definitions(${LIBRARY_NAME} PRIVATE CRASH_HANDLER_STD_STACKTRACE)
    target_link_libraries(${LIBRARY_NAME} PUBLIC ${STACKTRACE_LIBRARY})
else()
    message(STATUS "Crash handler: execinfo backtrace fallback")
    target_link_options(${LIBRARY_NAME} INTERFACE -rdynamic)
endif()

target_compile_options(${LIBRARY_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)
