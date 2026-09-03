#!/usr/bin/env bash
set -e

. "$(dirname "$0")/common.sh"

BUILD_DIR="${APPIMAGE_BUILD_DIR:-build-release}"
APPDIR="AppDir"
OUTPUT_DIR="${APPIMAGE_OUTPUT_DIR:-$PROJECT_DIR/dist}"
SKIP_BUILD=false

if [[ "$1" == "--skip-build" ]]; then
    SKIP_BUILD=true
    echo "Skipping build step..."
fi

GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}=== Building AppImage for $APP_NAME v$VERSION_NAME ===${NC}"

mkdir -p "$OUTPUT_DIR"

LINUXDEPLOY="$OUTPUT_DIR/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="$OUTPUT_DIR/linuxdeploy-plugin-qt-x86_64.AppImage"

if [ ! -f "$LINUXDEPLOY" ]; then
    echo -e "${GREEN}Downloading linuxdeploy...${NC}"
    wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage -O "$LINUXDEPLOY"
    chmod +x "$LINUXDEPLOY"
fi

if [ ! -f "$LINUXDEPLOY_QT" ]; then
    echo -e "${GREEN}Downloading linuxdeploy-plugin-qt...${NC}"
    wget -q https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage -O "$LINUXDEPLOY_QT"
    chmod +x "$LINUXDEPLOY_QT"
fi

if [ "$SKIP_BUILD" = false ]; then
    echo -e "${GREEN}Cleaning previous build...${NC}"
    rm -rf "$BUILD_DIR" "$APPDIR" *.AppImage

    echo -e "${GREEN}Building application...${NC}"
    cmake -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="${APPIMAGE_CXX_COMPILER:-clang++-18}" \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DQML_LIVE_ENABLED=OFF \
        -DBUILD_TESTING=OFF

    cmake --build "$BUILD_DIR" -j$(nproc)

    echo -e "${GREEN}Installing to AppDir...${NC}"
    DESTDIR="$APPDIR" cmake --install "$BUILD_DIR"
else
    echo -e "${GREEN}Cleaning previous AppDir and AppImage...${NC}"
    rm -rf "$APPDIR" *.AppImage

    echo -e "${GREEN}Installing to AppDir...${NC}"
    DESTDIR="$APPDIR" cmake --install "$BUILD_DIR"
fi

echo -e "${GREEN}Creating desktop file...${NC}"
mkdir -p "$APPDIR/usr/share/applications"
cat > "$APPDIR/usr/share/applications/$APP_ID.desktop" << EOF
[Desktop Entry]
Type=Application
Name=$APP_NAME
Comment=${APPIMAGE_COMMENT:-$APP_NAME}
Exec=$APP_TARGET
Icon=$APP_ID
Categories=${APPIMAGE_CATEGORIES:-Qt;}
Terminal=false
EOF

echo -e "${GREEN}Creating icon...${NC}"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
ICON_TARGET="$APPDIR/usr/share/icons/hicolor/256x256/apps/$APP_ID.png"
if [ -n "$APPIMAGE_ICON" ] && [ -f "$APPIMAGE_ICON" ]; then
    cp "$APPIMAGE_ICON" "$ICON_TARGET"
else
    INITIALS=$(echo "${APP_NAME:0:2}" | tr '[:lower:]' '[:upper:]')
    convert -size 256x256 xc:blue -gravity center -pointsize 100 -fill white -annotate +0+0 "$INITIALS" "$ICON_TARGET" 2>/dev/null || echo "Warning: No icon created"
fi

echo -e "${GREEN}Finding Qt installation...${NC}"
export QMAKE=$(which qmake6 || which qmake)
if [ -z "$QMAKE" ]; then
    echo -e "${RED}ERROR: qmake not found${NC}"
    exit 1
fi
echo -e "${GREEN}Using qmake: $QMAKE${NC}"

QT_DIR=$($QMAKE -query QT_INSTALL_PREFIX)
QML_URI_PATH="${APP_QML_URI//.//}"
QML_LIB_DIR="$PROJECT_DIR/$BUILD_DIR/$APP_QML_SOURCE_DIR"
export QML_SOURCES_PATHS="$PROJECT_DIR/$APP_QML_SOURCE_DIR:$PROJECT_DIR/$BUILD_DIR/$QML_URI_PATH"
export LD_LIBRARY_PATH="$QT_DIR/lib:$LD_LIBRARY_PATH"

echo -e "${GREEN}Generating AppImage...${NC}"
export OUTPUT="${APP_ID}-${VERSION_NAME}-x86_64.AppImage"
LIBS_BUILD_DIR="$PROJECT_DIR/$BUILD_DIR/libs"
export LD_LIBRARY_PATH="$LIBS_BUILD_DIR/cpp/dbtoolkit:$LIBS_BUILD_DIR/cpp/qmllive:$LIBS_BUILD_DIR/qml/theme:$LIBS_BUILD_DIR/qml/themed:$LIBS_BUILD_DIR/qml/app:$QML_LIB_DIR:$LD_LIBRARY_PATH"
"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/$APP_TARGET" \
    --library "$LIBS_BUILD_DIR/cpp/dbtoolkit/libapp_dbtoolkit.so" \
    --library "$LIBS_BUILD_DIR/cpp/qmllive/libapp_qmllive.so" \
    --library "$LIBS_BUILD_DIR/qml/theme/libapp_theme_qml.so" \
    --library "$LIBS_BUILD_DIR/qml/themed/libapp_themed_qml.so" \
    --library "$LIBS_BUILD_DIR/qml/app/libapp_components_qml.so" \
    --library "$QML_LIB_DIR/lib${APP_QML_TARGET}.so" \
    --plugin qt \
    --output appimage

if [ -f "$OUTPUT" ]; then
    mv "$OUTPUT" "$OUTPUT_DIR/"
    echo -e "${BLUE}=== AppImage created successfully: $OUTPUT_DIR/$OUTPUT ===${NC}"
    echo -e "${GREEN}Run with: $OUTPUT_DIR/$OUTPUT${NC}"
else
    echo -e "${RED}ERROR: AppImage generation failed${NC}"
    exit 1
fi
