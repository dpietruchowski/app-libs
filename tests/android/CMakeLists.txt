find_program(JAVA_EXECUTABLE java)

if(JAVA_EXECUTABLE)
    set(JAVA_SOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../android/java/app/libs)

    add_test(NAME quick_entry_model_java
        COMMAND ${CMAKE_COMMAND}
            -DJAVA_EXECUTABLE=${JAVA_EXECUTABLE}
            -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/quick_entry_model
            -DCLASS_SOURCE=${JAVA_SOURCES_DIR}/QuickEntryModel.java
            -DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/QuickEntryModelTest.java
            -DTEST_CLASS=app.libs.QuickEntryModelTest
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_java_test.cmake
    )
endif()
