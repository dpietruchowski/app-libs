add_test(NAME android_package_cmake
    COMMAND ${CMAKE_COMMAND}
        -DLIBS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../..
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/package
        -P ${CMAKE_CURRENT_SOURCE_DIR}/android_package_test.cmake
)
