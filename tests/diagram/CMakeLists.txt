find_package(Qt6 QUIET OPTIONAL_COMPONENTS Svg)

file(GLOB SOURCES "*.cpp")
file(GLOB HEADERS "*.h")
if(NOT TARGET Qt6::Svg)
    list(FILTER SOURCES EXCLUDE REGEX "svgsmoke_test\\.cpp$")
endif()

add_executable(diagram_tests ${SOURCES} ${HEADERS})

target_link_libraries(diagram_tests
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
        gtest
        app_diagram
)

if(TARGET Qt6::Svg)
    target_link_libraries(diagram_tests PRIVATE Qt6::Svg)
endif()

add_test(NAME diagram_tests COMMAND diagram_tests)

add_executable(diagram_render render/diagram_render.cpp)
target_link_libraries(diagram_render PRIVATE Qt6::Core Qt6::Gui app_diagram)
