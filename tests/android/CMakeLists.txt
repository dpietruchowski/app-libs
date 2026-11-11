find_program(JAVA_EXECUTABLE java)

if(JAVA_EXECUTABLE)
    set(JAVA_SOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../android/java/app/libs)

    function(app_add_java_test name test_class)
        set(sources "")
        foreach(source ${ARGN})
            list(APPEND sources ${JAVA_SOURCES_DIR}/${source}.java)
        endforeach()
        list(JOIN sources "|" class_sources)
        add_test(NAME ${name}
            COMMAND ${CMAKE_COMMAND}
                -DJAVA_EXECUTABLE=${JAVA_EXECUTABLE}
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/${name}
                -DCLASS_SOURCES=${class_sources}
                -DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/${test_class}.java
                -DTEST_CLASS=app.libs.${test_class}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/run_java_test.cmake
        )
    endfunction()

    app_add_java_test(quick_entry_model_java QuickEntryModelTest QuickEntryModel)
    app_add_java_test(quick_entry_queue_java QuickEntryQueueTest QuickEntryModel QuickEntryQueue)
    app_add_java_test(quick_entry_view_state_java QuickEntryViewStateTest QuickEntryModel QuickEntryViewState)
endif()
