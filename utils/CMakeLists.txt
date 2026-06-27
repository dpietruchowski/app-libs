set(LIBRARY_NAME app_utils)

set(LIBSECRET_AVAILABLE FALSE)
if(NOT ANDROID)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBSECRET IMPORTED_TARGET libsecret-1)
        if(LIBSECRET_FOUND)
            set(LIBSECRET_AVAILABLE TRUE)
        endif()
    endif()
endif()

file(GLOB SOURCES "*.cpp")
file(GLOB HEADERS "*.h")

add_library(${LIBRARY_NAME} STATIC ${SOURCES} ${HEADERS})

if(ANDROID)
    add_subdirectory(android)
else()
    add_subdirectory(desktop)
    if(LIBSECRET_AVAILABLE)
        add_subdirectory(linux)
        target_compile_definitions(${LIBRARY_NAME} PRIVATE HAVE_LIBSECRET)
        target_link_libraries(${LIBRARY_NAME} PRIVATE app_securestorage_linux)
    endif()
endif()

target_include_directories(${LIBRARY_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(${LIBRARY_NAME}
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
)

target_compile_options(${LIBRARY_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)
