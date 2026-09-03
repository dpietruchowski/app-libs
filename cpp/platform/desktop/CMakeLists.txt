target_sources(app_platform PRIVATE
    filesaverqt.h
    filesaverqt.cpp
    fileopenerqt.h
    fileopenerqt.cpp
)

target_include_directories(app_platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
