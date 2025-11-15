
add_subdirectory(qmllive)
add_subdirectory(dbtoolkit)

if(NOT QML_LIVE_ENABLED)
    add_subdirectory(qmlcomponents)
endif()
