include(${LIBS_DIR}/cmake/AndroidPackage.cmake)

app_android_add_tile(NAME DrinkTile ACTION quick_add_drink LABEL "Log a drink" ICON ic_tile_drink)
app_android_add_tile(NAME "${NAME}" ACTION "${ACTION}" LABEL "${LABEL}" ICON "${ICON}")
