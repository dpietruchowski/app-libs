set(LIBRARY_NAME themed_components_qml)

file(GLOB QML_FILES "*.qml")

qt_add_qml_module(${LIBRARY_NAME}
    URI Themed.Components
    VERSION 1.0
    QML_FILES ${QML_FILES}
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Themed/Components
)

target_link_libraries(${LIBRARY_NAME}
    PUBLIC
        Qt6::Core
        Qt6::Quick
)