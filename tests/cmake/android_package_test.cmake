include(${LIBS_DIR}/cmake/AndroidPackage.cmake)

macro(expect_contains TEXT NEEDLE)
    string(FIND "${TEXT}" "${NEEDLE}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "expected to find '${NEEDLE}' in:\n${TEXT}")
    endif()
endmacro()

macro(expect_not_contains TEXT NEEDLE)
    string(FIND "${TEXT}" "${NEEDLE}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "expected not to find '${NEEDLE}' in:\n${TEXT}")
    endif()
endmacro()

macro(expect_file_contains PATH NEEDLE)
    if(NOT EXISTS "${PATH}")
        message(FATAL_ERROR "expected file ${PATH} to exist")
    endif()
    file(READ "${PATH}" file_content)
    expect_contains("${file_content}" "${NEEDLE}")
endmacro()

function(expect_tile_rejected CASE_NAME NAME ACTION LABEL ICON MESSAGE)
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -DLIBS_DIR=${LIBS_DIR} -DNAME=${NAME} -DACTION=${ACTION} "-DLABEL=${LABEL}" -DICON=${ICON}
            -P ${CMAKE_CURRENT_LIST_DIR}/invalid_tile.cmake
        RESULT_VARIABLE result
        ERROR_VARIABLE error)
    if(result EQUAL 0)
        message(FATAL_ERROR "${CASE_NAME}: expected the tile to be rejected")
    endif()
    expect_contains("${error}" "${MESSAGE}")
endfunction()

file(REMOVE_RECURSE "${WORK_DIR}")

app_android_manifest_entries(no_tiles)
expect_contains("${no_tiles}" "android:name=\"app.libs.LockScreenLaunchActivity\"")
expect_contains("${no_tiles}" "android:showWhenLocked=\"true\"")
expect_contains("${no_tiles}" "android:exported=\"false\"")
expect_contains("${no_tiles}" "android:noHistory=\"true\"")
expect_contains("${no_tiles}" "android:excludeFromRecents=\"true\"")
expect_not_contains("${no_tiles}" "<service")

app_android_add_tile(NAME DrinkTile ACTION quick_add_drink LABEL "Log a drink" ICON ic_tile_drink)
app_android_add_tile(NAME PeeTile ACTION quick_add_pee LABEL "Log pee" ICON ic_tile_pee)

app_android_manifest_entries(entries)
expect_contains("${entries}" "android:name=\"app.libs.tile.DrinkTile\"")
expect_contains("${entries}" "android:label=\"Log a drink\"")
expect_contains("${entries}" "android:icon=\"@drawable/ic_tile_drink\"")
expect_contains("${entries}" "android:name=\"app.libs.tile.PeeTile\"")
expect_contains("${entries}" "android:label=\"Log pee\"")
expect_contains("${entries}" "android:icon=\"@drawable/ic_tile_pee\"")
expect_contains("${entries}" "android:permission=\"android.permission.BIND_QUICK_SETTINGS_TILE\"")
expect_contains("${entries}" "android.service.quicksettings.action.QS_TILE")

app_android_install_java("${WORK_DIR}")

expect_file_contains("${WORK_DIR}/src/app/libs/LaunchTileService.java" "class LaunchTileService")
expect_file_contains("${WORK_DIR}/src/app/libs/LockScreen.java" "class LockScreen")
expect_file_contains("${WORK_DIR}/src/app/libs/LockScreenActivity.java" "extends QtActivity")
expect_file_contains("${WORK_DIR}/src/app/libs/LockScreenLaunchActivity.java"
    "class LockScreenLaunchActivity")
expect_file_contains("${WORK_DIR}/src/app/libs/tile/DrinkTile.java"
    "public final class DrinkTile extends LaunchTileService")
expect_file_contains("${WORK_DIR}/src/app/libs/tile/DrinkTile.java" "return \"quick_add_drink\";")
expect_file_contains("${WORK_DIR}/src/app/libs/tile/PeeTile.java"
    "public final class PeeTile extends LaunchTileService")
expect_file_contains("${WORK_DIR}/src/app/libs/tile/PeeTile.java" "return \"quick_add_pee\";")

file(WRITE "${WORK_DIR}/src/app/libs/tile/RenamedTile.java" "stale")
file(WRITE "${WORK_DIR}/src/app/libs/RemovedHelper.java" "stale")
file(WRITE "${WORK_DIR}/src/com/example/Own.java" "kept")
app_android_install_java("${WORK_DIR}")
if(EXISTS "${WORK_DIR}/src/app/libs/tile/RenamedTile.java")
    message(FATAL_ERROR "a tile class that is no longer declared must be removed")
endif()
if(EXISTS "${WORK_DIR}/src/app/libs/RemovedHelper.java")
    message(FATAL_ERROR "a libs Java file that no longer exists must be removed")
endif()
expect_file_contains("${WORK_DIR}/src/app/libs/tile/DrinkTile.java" "class DrinkTile")
expect_file_contains("${WORK_DIR}/src/com/example/Own.java" "kept")

expect_tile_rejected("duplicate" DrinkTile other "Other" ic_other "already declared")
expect_tile_rejected("lower-case name" drinkTile act "Label" ic_other "NAME must be a Java class name")
expect_tile_rejected("action with a quote" OtherTile "a\"b" "Label" ic_other "ACTION may contain only")
expect_tile_rejected("label with a bracket" OtherTile act "A<b" ic_other "LABEL must not contain")
expect_tile_rejected("icon with a path" OtherTile act "Label" "res/icon" "ICON must be a drawable")
expect_tile_rejected("missing action" OtherTile "" "Label" ic_other "ACTION is required")
