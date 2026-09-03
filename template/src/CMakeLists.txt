find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

qt_standard_project_setup()

add_subdirectory(ui/qml)

qt_add_executable(${APP_TARGET}
    main.cpp
)

if(LIBS_AUTOMATION)
    target_link_libraries(${APP_TARGET} PRIVATE app_automation)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(${APP_TARGET} PRIVATE app_crashhandler)
endif()

target_compile_definitions(${APP_TARGET} PRIVATE
    APP_NAME="${APP_NAME}"
    APP_QML_URI="${APP_QML_URI}"
    APP_VERSION="${VERSION_NAME}"
    APP_VERSION_CODE="${VERSION_CODE}"
)

target_link_libraries(${APP_TARGET}
    PRIVATE
        app_async
        app_platform
        app_qmlutils
        app_utils
        -Wl,--no-as-needed
        app_theme_qml
        app_themed_qml
        app_components_qml
        app_icons
        ${APP_QML_TARGET}
        -Wl,--as-needed
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        $<$<PLATFORM_ID:Android>:log>
)

install(TARGETS ${APP_TARGET}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
