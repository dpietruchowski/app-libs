include_guard(GLOBAL)

set(_APP_LIBS_ANDROID_DIR "${CMAKE_CURRENT_LIST_DIR}/../android")

function(_app_android_require_match VALUE PATTERN DESCRIPTION)
    if(NOT "${VALUE}" MATCHES "${PATTERN}")
        message(FATAL_ERROR "app_android_add_tile: ${DESCRIPTION}, got '${VALUE}'")
    endif()
endfunction()

function(app_android_add_tile)
    cmake_parse_arguments(ARG "" "NAME;ACTION;LABEL;ICON" "" ${ARGN})

    foreach(required NAME ACTION LABEL ICON)
        if(NOT ARG_${required})
            message(FATAL_ERROR "app_android_add_tile: ${required} is required")
        endif()
    endforeach()

    _app_android_require_match("${ARG_NAME}" "^[A-Z][A-Za-z0-9]*$"
        "NAME must be a Java class name starting with an upper-case letter")
    _app_android_require_match("${ARG_ACTION}" "^[A-Za-z0-9_.]+$"
        "ACTION may contain only letters, digits, '_' and '.'")
    _app_android_require_match("${ARG_LABEL}" "^[^\"<>&]+$"
        "LABEL must not contain quotes, '<', '>' or '&'")
    _app_android_require_match("${ARG_ICON}" "^[a-z][a-z0-9_]*$"
        "ICON must be a drawable resource name")

    get_property(names GLOBAL PROPERTY APP_ANDROID_TILE_NAMES)
    list(FIND names "${ARG_NAME}" declared_index)
    if(NOT declared_index EQUAL -1)
        message(FATAL_ERROR "app_android_add_tile: tile ${ARG_NAME} is already declared")
    endif()

    set_property(GLOBAL APPEND PROPERTY APP_ANDROID_TILE_NAMES "${ARG_NAME}")
    set_property(GLOBAL PROPERTY APP_ANDROID_TILE_${ARG_NAME}_ACTION "${ARG_ACTION}")
    set_property(GLOBAL PROPERTY APP_ANDROID_TILE_${ARG_NAME}_LABEL "${ARG_LABEL}")
    set_property(GLOBAL PROPERTY APP_ANDROID_TILE_${ARG_NAME}_ICON "${ARG_ICON}")
endfunction()

function(app_android_manifest_entries OUT_VAR)
    get_property(names GLOBAL PROPERTY APP_ANDROID_TILE_NAMES)

    set(entries "")
    string(APPEND entries
        "        <activity\n"
        "            android:name=\"app.libs.LockScreenLaunchActivity\"\n"
        "            android:excludeFromRecents=\"true\"\n"
        "            android:exported=\"false\"\n"
        "            android:noHistory=\"true\"\n"
        "            android:showWhenLocked=\"true\"\n"
        "            android:taskAffinity=\"\"\n"
        "            android:theme=\"@android:style/Theme.NoDisplay\"/>\n")
    foreach(name IN LISTS names)
        get_property(label GLOBAL PROPERTY APP_ANDROID_TILE_${name}_LABEL)
        get_property(icon GLOBAL PROPERTY APP_ANDROID_TILE_${name}_ICON)
        string(APPEND entries
            "        <service\n"
            "            android:name=\"app.libs.tile.${name}\"\n"
            "            android:exported=\"true\"\n"
            "            android:icon=\"@drawable/${icon}\"\n"
            "            android:label=\"${label}\"\n"
            "            android:permission=\"android.permission.BIND_QUICK_SETTINGS_TILE\">\n"
            "            <intent-filter>\n"
            "                <action android:name=\"android.service.quicksettings.action.QS_TILE\"/>\n"
            "            </intent-filter>\n"
            "        </service>\n")
    endforeach()

    set(${OUT_VAR} "${entries}" PARENT_SCOPE)
endfunction()

function(app_android_install_java PACKAGE_DIR)
    set(java_root "${_APP_LIBS_ANDROID_DIR}/java")
    file(REMOVE_RECURSE "${PACKAGE_DIR}/src/app/libs")
    file(GLOB_RECURSE java_files RELATIVE "${java_root}" "${java_root}/*.java")
    foreach(java_file IN LISTS java_files)
        configure_file("${java_root}/${java_file}" "${PACKAGE_DIR}/src/${java_file}" COPYONLY)
    endforeach()

    get_property(names GLOBAL PROPERTY APP_ANDROID_TILE_NAMES)
    foreach(name IN LISTS names)
        get_property(TILE_ACTION GLOBAL PROPERTY APP_ANDROID_TILE_${name}_ACTION)
        set(TILE_NAME "${name}")
        configure_file("${_APP_LIBS_ANDROID_DIR}/templates/Tile.java.in"
            "${PACKAGE_DIR}/src/app/libs/tile/${name}.java" @ONLY)
    endforeach()
endfunction()
