set(LIBRARY_NAME themed_icons)

file(GLOB ICON_FILES_FULL "${CMAKE_CURRENT_SOURCE_DIR}/*.svg")

set(ICON_FILES "")
foreach(icon_file ${ICON_FILES_FULL})
    get_filename_component(file_name ${icon_file} NAME)
    list(APPEND ICON_FILES ${file_name})
endforeach()

qt_add_library(${LIBRARY_NAME} STATIC)

qt_add_resources(${LIBRARY_NAME} "app_icons"
    PREFIX "/Themed/Icons"
    FILES
        ${ICON_FILES}
)

target_link_libraries(${LIBRARY_NAME}
    PUBLIC
        Qt6::Core
)
