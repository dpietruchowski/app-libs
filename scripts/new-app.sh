#!/usr/bin/env bash
set -e

LIBS_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_DIR="$(cd "$LIBS_DIR/.." && pwd)"
TEMPLATE_DIR="$LIBS_DIR/template"

GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

APP_NAME="$1"

if [ -z "$APP_NAME" ]; then
    echo "Usage: $0 <AppName>"
    echo
    echo "Scaffolds a new app around the libs submodule, in the repo that contains"
    echo "this checkout of libs ($PROJECT_DIR). Existing files are never overwritten."
    exit 1
fi

if [ ! -d "$TEMPLATE_DIR" ]; then
    echo -e "${RED}ERROR: template directory missing: $TEMPLATE_DIR${NC}"
    exit 1
fi

APP_ID=$(echo "$APP_NAME" | tr '[:upper:]' '[:lower:]' | tr -cd '[:alnum:]')
APP_QML_URI=$(echo "$APP_NAME" | tr -cd '[:alnum:]')

if [ -z "$APP_ID" ]; then
    echo -e "${RED}ERROR: app name must contain at least one letter or digit${NC}"
    exit 1
fi

echo -e "${BLUE}=== Scaffolding $APP_NAME in $PROJECT_DIR ===${NC}"
echo "  app id      : $APP_ID"
echo "  target      : app$APP_ID"
echo "  QML URI     : $APP_QML_URI"

substitute() {
    sed -e "s/__APP_NAME__/$APP_NAME/g" \
        -e "s/__APP_ID__/$APP_ID/g" \
        -e "s/__APP_QML_URI__/$APP_QML_URI/g" "$1"
}

CREATED=0
SKIPPED=0

install_file() {
    local rel="$1"
    local target="$PROJECT_DIR/$rel"

    if [ -e "$target" ]; then
        echo "  skip    $rel (exists)"
        SKIPPED=$((SKIPPED + 1))
        return
    fi

    mkdir -p "$(dirname "$target")"
    substitute "$TEMPLATE_DIR/$rel" > "$target"
    echo -e "  ${GREEN}create${NC}  $rel"
    CREATED=$((CREATED + 1))
}

while IFS= read -r rel; do
    install_file "$rel"
done < <(cd "$TEMPLATE_DIR" && find . -type f -printf '%P\n' | sort)

if [ ! -e "$PROJECT_DIR/.clang-format" ]; then
    ln -s libs/.clang-format "$PROJECT_DIR/.clang-format"
    echo -e "  ${GREEN}link${NC}    .clang-format -> libs/.clang-format"
    CREATED=$((CREATED + 1))
fi

for skill in format-code commit todo bump-android-version ui-session; do
    target="$PROJECT_DIR/.claude/skills/$skill"
    if [ ! -e "$target" ]; then
        mkdir -p "$PROJECT_DIR/.claude/skills"
        ln -s "../../libs/claude/skills/$skill" "$target"
        echo -e "  ${GREEN}link${NC}    .claude/skills/$skill"
        CREATED=$((CREATED + 1))
    fi
done

if [ ! -e "$PROJECT_DIR/.claude/agents/committer.md" ]; then
    mkdir -p "$PROJECT_DIR/.claude/agents"
    ln -s "../../libs/claude/agents/committer.md" "$PROJECT_DIR/.claude/agents/committer.md"
    echo -e "  ${GREEN}link${NC}    .claude/agents/committer.md"
    CREATED=$((CREATED + 1))
fi

echo -e "${BLUE}=== $CREATED created, $SKIPPED skipped ===${NC}"
echo
echo "Next:"
echo "  cmake -B build-desktop -DCMAKE_BUILD_TYPE=Debug"
echo "  cmake --build build-desktop -j\$(nproc)"
echo "  ./build-desktop/src/app$APP_ID"
