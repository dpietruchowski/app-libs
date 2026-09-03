add_subdirectory(query)

add_library(app_dbtoolkit SHARED
    ${QUERY_SOURCES}
    dbtoolkit.h
    dbstorage.h
    dbstorage.cpp
    dbrepository.h
    dbrepository.cpp
    migrationrunner.h
    migrationrunner.cpp
)

target_link_libraries(app_dbtoolkit 
    PRIVATE 
        Qt6::Core 
        Qt6::Sql
)

target_include_directories(app_dbtoolkit
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)
