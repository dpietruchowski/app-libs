set(LIBRARY_NAME app_texttospeech)

find_package(Qt6 REQUIRED COMPONENTS Core)

if(NOT ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS TextToSpeech)
endif()

set(SOURCES texttospeech.cpp)
set(HEADERS texttospeech.h)

if(ANDROID)
    list(APPEND SOURCES texttospeechandroid.cpp)
    list(APPEND HEADERS texttospeechandroid.h)
endif()

add_library(${LIBRARY_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${LIBRARY_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(${LIBRARY_NAME}
    PUBLIC
        Qt6::Core
)

if(NOT ANDROID)
    target_link_libraries(${LIBRARY_NAME} PUBLIC Qt6::TextToSpeech)
endif()

target_compile_options(${LIBRARY_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)
