set(LIBRARY_NAME app_chat_qml)

file(GLOB QML_FILES_FULL "${CMAKE_CURRENT_SOURCE_DIR}/*.qml")
file(GLOB SOURCES_FULL "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

set(QML_FILES "")
foreach(qml_file ${QML_FILES_FULL})
    get_filename_component(file_name ${qml_file} NAME)
    list(APPEND QML_FILES ${file_name})
endforeach()

set(SOURCES "")
foreach(source_file ${SOURCES_FULL})
    get_filename_component(file_name ${source_file} NAME)
    list(APPEND SOURCES ${file_name})
endforeach()

qt_add_qml_module(${LIBRARY_NAME}
    URI Chat.Components
    VERSION 1.0
    QML_FILES ${QML_FILES}
    SOURCES ${SOURCES}
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Chat/Components
)

target_link_libraries(${LIBRARY_NAME}
    PUBLIC
        app_chat
        Qt6::Core
        Qt6::Quick
        Qt6::Qml
)
