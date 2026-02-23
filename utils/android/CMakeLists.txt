add_subdirectory(jni)

target_sources(app_utils PRIVATE
    androidnotification.h
    androidnotification.cpp
    androidnotificationscheduler.h
    androidnotificationscheduler.cpp
)

target_include_directories(app_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/jni
)

target_link_libraries(app_utils PRIVATE
    Qt6::Core
)
