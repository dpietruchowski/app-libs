add_subdirectory(query)

add_library(dbtoolkit SHARED
    ${QUERY_SOURCES}
    dbtoolkit.h
    dbstorage.h
    dbstorage.cpp
    dbrepository.h
    dbrepository.cpp
)

target_link_libraries(dbtoolkit 
    PRIVATE 
        Qt6::Core 
        Qt6::Sql
)

target_include_directories(dbtoolkit
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)
