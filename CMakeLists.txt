find_package(Qt6 REQUIRED COMPONENTS Core Quick Qml Sql)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_subdirectory(qmllive)
add_subdirectory(dbtoolkit)
add_subdirectory(utils)
add_subdirectory(icons)

if(NOT QML_LIVE_ENABLED)
    add_subdirectory(qmlcomponents)
endif()
