set(LIBRARY_NAME themed_components_qml)

file(GLOB QML_FILES_FULL "${CMAKE_CURRENT_SOURCE_DIR}/*.qml")

set(QML_FILES "")
foreach(qml_file ${QML_FILES_FULL})
    get_filename_component(file_name ${qml_file} NAME)
    list(APPEND QML_FILES ${file_name})
endforeach()

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
        Qt6::Qml
)
