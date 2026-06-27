target_sources(app_utils PRIVATE
    filesaverqt.h
    filesaverqt.cpp
    fileopenerqt.h
    fileopenerqt.cpp
)

target_include_directories(app_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
