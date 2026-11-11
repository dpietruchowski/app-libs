file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

string(REPLACE "|" ";" class_sources "${CLASS_SOURCES}")

execute_process(
    COMMAND ${JAVA_EXECUTABLE} -m jdk.compiler/com.sun.tools.javac.Main
        -d ${WORK_DIR} ${class_sources} ${TEST_SOURCE}
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_output
    ERROR_VARIABLE compile_output)
if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR "compilation failed:\n${compile_output}")
endif()

execute_process(
    COMMAND ${JAVA_EXECUTABLE} -cp ${WORK_DIR} ${TEST_CLASS}
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_output
    ERROR_VARIABLE run_output)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "${TEST_CLASS} failed:\n${run_output}")
endif()
