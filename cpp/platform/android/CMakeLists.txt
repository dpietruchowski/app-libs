add_subdirectory(jni)

target_sources(app_platform PRIVATE
    androidnotification.h
    androidnotification.cpp
    androidnotificationscheduler.h
    androidnotificationscheduler.cpp
    securestorageandroid.h
    securestorageandroid.cpp
    filesaverandroid.h
    filesaverandroid.cpp
    fileopenerandroid.h
    fileopenerandroid.cpp
    hapticsandroid.h
    hapticsandroid.cpp
)

target_include_directories(app_platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/jni
)

target_link_libraries(app_platform PRIVATE
    Qt6::Core
    Qt6::CorePrivate
)
