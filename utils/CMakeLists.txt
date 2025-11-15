set(LIBRARY_NAME app_utils)

file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.h")

add_library(${LIBRARY_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${LIBRARY_NAME}
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(${LIBRARY_NAME}
    PUBLIC
        Qt6::Core
)

target_compile_options(${LIBRARY_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)