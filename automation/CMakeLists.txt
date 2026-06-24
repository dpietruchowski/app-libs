set(LIBRARY_NAME app_automation)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Network)

set(CMAKE_AUTOMOC ON)

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
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Network
        app_utils
)
